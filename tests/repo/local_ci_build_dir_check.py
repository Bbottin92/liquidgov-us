"""local_ci_build_dir_check.py: scripts/local_ci.py creates its build folder itself (q49, 2026-11-19). It made only the folder's
PARENT and then symlinked $RUNNER_TEMP/lg to the build folder, so on a fresh machine (no ~/.cache/liquidgov-local-ci/lg yet) the link
dangled and cmake could not create CMakeFiles through it: both server builds failed on the first run.

  B0  precondition: scripts/local_ci.py exists and pyyaml imports
  B1  run in a throwaway one-commit repo with --build <fresh>/a/b/lg --only none --no-record: the build folder exists afterwards
      (--only none skips every workflow, so this is seconds, not the hour a real run takes)

Fails on the old code (the build folder is never created). Usage: python tests/repo/local_ci_build_dir_check.py [repo root] -> last
line passes=N fails=M; exit 1 on FAIL."""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPT = os.path.join(ROOT, "scripts", "local_ci.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


def git(cwd, *a):
    return subprocess.run(["git"] + list(a), cwd=cwd, env=ENV, capture_output=True, text=True)


check("B0 local_ci.py exists", os.path.isfile(SCRIPT), SCRIPT)
tmp = tempfile.mkdtemp(prefix="localci-builddir-")
try:
    # local_ci.py imports yaml at the top but only reads workflow files with it; --only none skips them all, so a stub keeps this
    # check runnable on a machine without pyyaml (this Windows Python) instead of skipping it
    stubs = os.path.join(tmp, "stubs")
    os.makedirs(stubs)
    open(os.path.join(stubs, "yaml.py"), "w").write("def safe_load(*a, **k):\n    raise RuntimeError('stub yaml: not expected to be called')\n")
    ENV = dict(ENV, PYTHONPATH=stubs)
    repo = os.path.join(tmp, "repo")
    os.makedirs(os.path.join(repo, "scripts"))
    os.makedirs(os.path.join(repo, "liquidgov-platform"))
    shutil.copy(SCRIPT, os.path.join(repo, "scripts", "local_ci.py"))
    open(os.path.join(repo, "liquidgov-platform", "x.txt"), "w").write("x\n")
    git(repo, "init", "-q")
    git(repo, "config", "user.email", "t@example.com")
    git(repo, "config", "user.name", "t")
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "init")
    build = os.path.join(tmp, "fresh", "a", "b", "lg")
    p = subprocess.run([sys.executable, os.path.join(repo, "scripts", "local_ci.py"), "--build", build, "--only", "none", "--no-record"],
                       cwd=repo, env=ENV, capture_output=True, text=True, timeout=120)
    check("B1 the build folder exists after a run on a fresh machine (the symlink does not dangle)", os.path.isdir(build),
          (p.returncode, p.stdout[-200:], p.stderr[-200:]))
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
