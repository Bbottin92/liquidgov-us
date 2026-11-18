#!/usr/bin/env python3
"""The checks never get the hook's own control variable LG_CHECK_MODE (q97, 2026-11-17).
.githooks/pre-commit sets LG_CHECK_MODE=--staged for pre-push (the selector reads it). pre-push dropped the GIT_* variables
before it ran the checks but left LG_CHECK_MODE in their environment. A check that builds a throwaway repo with the real hooks
(hook_linux_only_skip_check.py) then ran ITS hooks in staged mode: the inner push saw no staged file, "0 changed file(s)",
the failing check was never selected, and two cases (L4) failed. It failed only inside a real commit that changed
scripts/affected_checks.py (which runs every check), so it looked flaky: it passed alone, passed in run_hook_checks.py on a
clean tree, and refused the commit twice (2026-11-17).

Here: a throwaway repo with the real .githooks, the real selector and ONE probe check that writes down whether
LG_CHECK_MODE is in its environment. A real `git commit` (pre-commit sets it) and a real `git push` run the hooks. Each
time: the probe ran (precondition) and was given no LG_CHECK_MODE. The selector still gets it: the commit picks the probe
because the staged file is not in the selector's map (every check runs) - the same trigger hook_git_env_check.py uses.
This file's own git calls drop GIT_* and LG_CHECK_MODE.
    python3 hook_check_mode_env_check.py [repo root]
Prints 'hook_check_mode_env_check: N checks, F failed'; exit 1 if any failed."""
import os
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
checks = failed = 0
CLEAN = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and k != "LG_CHECK_MODE"}
CLEAN["PYTHONDONTWRITEBYTECODE"] = "1"

PROBE = r'''#!/usr/bin/env python3
"""probe: records whether the hook's control variable reached the check."""
import os
with open(os.environ["LG_PROBE_OUT"], "a", encoding="utf-8") as f:
    f.write("RAN " + ("LG_CHECK_MODE=" + os.environ["LG_CHECK_MODE"] if "LG_CHECK_MODE" in os.environ else "clean") + "\n")
print("probe_check: 1 checks, 0 failed")
'''


def check(ok, what, detail=""):
    global checks, failed
    checks += 1
    print(("PASS " if ok else "FAIL ") + what + ("" if ok else "  :: %s" % (detail,)))
    if not ok:
        failed += 1


def git(cwd, *args, env=None):
    cmd = ["git", "-c", "user.name=lg", "-c", "user.email=lg@example.invalid", "-c", "commit.gpgsign=false"]
    return subprocess.run(cmd + list(args), cwd=cwd, capture_output=True, text=True, env=env or CLEAN, stdin=subprocess.DEVNULL)


def rmtree(path):
    def writable(func, p, _):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    shutil.rmtree(path, onerror=writable)


def build(tmp):
    main, origin = os.path.join(tmp, "main"), os.path.join(tmp, "origin.git")
    git(tmp, "init", "-q", "--bare", origin)
    git(tmp, "init", "-q", main)
    git(main, "checkout", "-q", "-b", "master")
    for rel in (".githooks/pre-commit", ".githooks/pre-push", "scripts/affected_checks.py", "scripts/run_hook_checks.py"):
        dst = os.path.join(main, *rel.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.join(ROOT, *rel.split("/")), dst)
        if rel.startswith(".githooks/"):
            os.chmod(dst, 0o755)
    tests = os.path.join(main, "tests", "repo")
    os.makedirs(tests)
    with open(os.path.join(tests, "probe_check.py"), "w", encoding="utf-8", newline="\n") as f:
        f.write(PROBE)
    with open(os.path.join(main, "notes.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("base\n")
    git(main, "add", "-A")
    git(main, "update-index", "--chmod=+x", ".githooks/pre-commit", ".githooks/pre-push")
    git(main, "commit", "-qm", "base")   # hooks are not on yet
    git(main, "remote", "add", "origin", origin)
    git(main, "push", "-q", "origin", "master")
    git(main, "config", "core.hooksPath", ".githooks")
    return main


def lines(path):
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip()]


tmp = tempfile.mkdtemp(prefix="hook-check-mode-")
try:
    main = build(tmp)
    probe_out = os.path.join(tmp, "probe.out")
    env = dict(CLEAN, LG_PROBE_OUT=probe_out)
    check(git(main, "config", "--get", "core.hooksPath").stdout.strip() == ".githooks",
          "precondition: throwaway repo built, the real hooks are on")

    with open(os.path.join(main, "notes.md"), "a", encoding="utf-8", newline="\n") as f:
        f.write("edit\n")
    git(main, "add", "notes.md")
    r = git(main, "commit", "-qm", "edit", env=env)
    got = lines(probe_out)
    check(r.returncode == 0 and len(got) >= 1, "commit: the git command succeeds and the hook ran the probe (precondition)",
          (r.returncode, (r.stdout + r.stderr)[-300:], got))
    check(bool(got) and all(l == "RAN clean" for l in got), "commit: the check is given no LG_CHECK_MODE (pre-commit sets it for the selector)", got)
    if os.path.exists(probe_out):
        os.remove(probe_out)

    r = git(main, "push", "-q", "origin", "master", env=env)
    got = lines(probe_out)
    check(r.returncode == 0 and len(got) >= 1, "push: the git command succeeds and the hook ran the probe (precondition)",
          (r.returncode, (r.stdout + r.stderr)[-300:], got))
    check(bool(got) and all(l == "RAN clean" for l in got), "push: the check is given no LG_CHECK_MODE", got)
finally:
    rmtree(tmp)

print("hook_check_mode_env_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
