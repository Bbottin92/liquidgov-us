"""sync_bytecode_exclude_check.py: deploy/sync-to-droplet.sh must never send Python bytecode (q89, 2026-11-17).
Why: running the browser checks leaves tests/browser/__pycache__/*.pyc in the worktree; the dry sync listed them as files to
send (seen 2026-11-12 and again 2026-11-17, lessons.md:368). The script hardcodes its rsync exclusions so a stray local
file cannot ship; bytecode belongs in that list.
Runs the real sync-to-droplet.sh in a throwaway git repo with a fake rsync that records its arguments (nothing leaves this
machine), reads the --exclude patterns it was given, and applies them the way rsync does to a pattern with no slash (it
matches any path component by name).
  precondition: the dry run reaches the fake rsync and still excludes the old patterns (build, .git, *.lgp): the
                instrument works and the parse sees real excludes
  a __pycache__ directory is excluded
  a .pyc file is excluded
  a source file (a.cpp) and an asset (lgmap.js) are NOT excluded
Usage: python tests/repo/sync_bytecode_exclude_check.py <repo root>   -> last line passes=N fails=M; exit 1 on any FAIL."""
import fnmatch
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))
    passes += bool(ok)
    fails += not ok


def fwd(p):
    return p.replace("\\", "/")


def git(cwd, *args):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    r = subprocess.run(["git"] + list(args), cwd=cwd, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise RuntimeError("git %s failed: %s" % (" ".join(args), r.stderr.strip()))
    return r.stdout.strip()


def excluded(path, patterns):
    """rsync's rule for a pattern with no slash: it matches the final name of any path component, so a directory match
    excludes everything under it."""
    parts = path.split("/")
    return any(fnmatch.fnmatch(part, pat) for pat in patterns for part in parts)


tmp = tempfile.mkdtemp(prefix="sync-bytecode-check-")
try:
    repo = os.path.join(tmp, "repo")
    os.makedirs(os.path.join(repo, "deploy"))
    os.makedirs(os.path.join(repo, "liquidgov-platform"))
    for f in ("sync-to-droplet.sh", "ci-gate.sh", "git-in-repo.sh", "rollback_guard.py"):
        src = os.path.join(ROOT, "deploy", f)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(repo, "deploy", f))
    with open(os.path.join(repo, "liquidgov-platform", "a.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write("int a = 1;\n")
    git(repo, "init", "-q")
    git(repo, "config", "user.email", "t@t")
    git(repo, "config", "user.name", "t")
    git(repo, "config", "core.autocrlf", "false")
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "code")
    head = git(repo, "rev-parse", "HEAD")

    bin_dir = os.path.join(tmp, "bin")
    os.makedirs(bin_dir)
    marker = os.path.join(tmp, "rsync-called.txt")
    with open(os.path.join(bin_dir, "rsync"), "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\necho "$@" >> "%s"\nexit 0\n' % fwd(marker))
    os.chmod(os.path.join(bin_dir, "rsync"), 0o755)

    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and not k.startswith("LIQUIDGOV_")}
    env.update({"PATH": bin_dir + os.pathsep + os.environ.get("PATH", ""), "LIQUIDGOV_DEPLOY_SSH": "true",
                "LIQUIDGOV_DEPLOY_KEY": "/nonexistent", "LIQUIDGOV_DEPLOY_DRY_RUN": "1",
                "LIQUIDGOV_LIVE_COMMIT": head, "LIQUIDGOV_NO_ROLLBACK_OK": "sync_bytecode_exclude_check tests the exclusions only"})
    try:
        r = subprocess.run([BASH, fwd(os.path.join(repo, "deploy", "sync-to-droplet.sh")), "nobody@127.0.0.1:/nowhere/"],
                           capture_output=True, text=True, env=env, stdin=subprocess.DEVNULL, timeout=60)
        rc, out = r.returncode, (r.stdout + r.stderr).strip()
    except subprocess.TimeoutExpired:
        rc, out = 124, "TIMEOUT"
    called = open(marker, encoding="utf-8").read().strip() if os.path.exists(marker) else ""
    toks = shlex.split(called) if called else []
    pats = [toks[i + 1] for i, t in enumerate(toks) if t == "--exclude" and i + 1 < len(toks)]
    pats += [t[len("--exclude="):] for t in toks if t.startswith("--exclude=")]

    check("precondition: the dry run reaches the fake rsync, and the old exclusions (build, .git, *.lgp) are in its arguments",
          rc == 0 and bool(called) and {"build", ".git", "*.lgp"} <= set(pats), (rc, pats, out[-300:]))
    check("a __pycache__ directory is excluded (a non-.pyc name inside it, so only the directory pattern can match)",
          excluded("tests/browser/__pycache__/notes.txt", pats), pats)
    check("a .pyc file is excluded", excluded("tests/browser/walker_map_clarity_check.cpython-314.pyc", pats), pats)
    check("a source file and an asset are NOT excluded",
          not excluded("src/a.cpp", pats) and not excluded("assets/lgmap.js", pats), pats)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
