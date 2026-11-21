"""hook_pass_record_check.py: the pre-push hook does not rerun a check that already passed at commit on the very same
files (q278, Brandon 2026-11-21 "yes, make it the first item"). 2026-11-21: commits that touched
scripts/affected_checks.py ran every check at commit and again at push on an identical tree, twice in one night.
Runs the real .githooks/pre-push + pre-commit in a throwaway repo whose selector always picks one fake check that counts
its runs (no network, no server).
  R1 commit, then push the same commit with the disk unchanged: the check ran once; the push says SKIPPED (passed at
     commit ...), never a pass line
  R2 an unstaged edit on disk at push: the check runs again (the checks read the disk)
  R3 a check that FAILS at commit leaves no record, so a later push of that tree reruns it
  R4 two refs in one push: no skipping
  R5 each check that runs prints its duration [Ns]
Usage: python tests/repo/hook_pass_record_check.py [repo root] -> last line passes=N fails=M; exit 1 on any FAIL."""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
# GIT_*, LG_CHECK_MODE and bytecode handled by the shared helper (hook_env_isolation_check.py keeps it that way)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "scripts"))
from hook_env import clean_env  # noqa: E402  (scripts/hook_env.py)
ENV = clean_env()
passes = fails = 0

FAKE_SELECTOR = r'''import sys
sys.stdin.read() if not sys.stdin.isatty() else None
if "--linux-only" not in sys.argv:
    print("fake_check.py")
'''
FAKE_CHECK = r'''import os, sys
open(os.environ["FAKE_RUNS"], "a").write("ran\n")
bad = os.environ.get("FAKE_FAIL") == "1"
print("FAIL fake" if bad else "PASS fake")
print("passes=%d fails=%d" % (0 if bad else 1, 1 if bad else 0))
sys.exit(1 if bad else 0)
'''


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-600:]))
    passes += bool(ok)
    fails += not ok


tmp = tempfile.mkdtemp(prefix="hook-pass-")
runs = os.path.join(tmp, "runs.txt")
env = dict(ENV, FAKE_RUNS=runs)


def git(cwd, *args, extra=None, ok=True):
    p = subprocess.run(["git"] + list(args), cwd=cwd, capture_output=True, text=True, env=dict(env, **(extra or {})))
    if ok and p.returncode != 0:
        raise RuntimeError("git %s: %s" % (" ".join(args), p.stdout + p.stderr))
    return p


def count():
    return open(runs).read().count("ran") if os.path.exists(runs) else 0


try:
    hook = os.path.join(ROOT, ".githooks", "pre-push")
    check("precondition: .githooks/pre-push exists", os.path.isfile(hook), hook)
    origin, work = os.path.join(tmp, "origin.git"), os.path.join(tmp, "work")
    git(tmp, "init", "-q", "--bare", "-b", "master", origin)
    git(tmp, "init", "-q", "-b", "master", work)
    git(work, "config", "user.email", "t@t"); git(work, "config", "user.name", "t")
    os.makedirs(os.path.join(work, ".githooks")); os.makedirs(os.path.join(work, "scripts"))
    os.makedirs(os.path.join(work, "tests", "repo"))
    for h in ("pre-push", "pre-commit"):
        shutil.copy2(os.path.join(ROOT, ".githooks", h), os.path.join(work, ".githooks", h))
    open(os.path.join(work, "scripts", "affected_checks.py"), "w", newline="\n").write(FAKE_SELECTOR)
    open(os.path.join(work, "tests", "repo", "fake_check.py"), "w", newline="\n").write(FAKE_CHECK)
    open(os.path.join(work, "a.txt"), "w").write("a\n")
    git(work, "add", "."); git(work, "commit", "-q", "--no-verify", "-m", "base")
    git(work, "config", "core.hooksPath", ".githooks")
    git(work, "remote", "add", "origin", origin); git(work, "push", "-q", "--no-verify", "origin", "master")

    # ---- R1 + R5
    open(os.path.join(work, "a.txt"), "w").write("a2\n")
    git(work, "add", "a.txt")
    p = git(work, "commit", "-m", "r1", ok=False)
    check("R1: the commit ran the check once", p.returncode == 0 and count() == 1, (p.returncode, count(), p.stdout + p.stderr))
    check("R5: the run printed its duration [Ns]", "s]" in (p.stdout + p.stderr) and "passes=1 fails=0  [" in (p.stdout + p.stderr),
          p.stdout + p.stderr)
    p = git(work, "push", "origin", "master", ok=False)
    out = p.stdout + p.stderr
    check("R1: the push of the same files did not rerun it", p.returncode == 0 and count() == 1, (p.returncode, count(), out))
    check("R1: ...and said SKIPPED (passed at commit), not a pass line",
          "SKIPPED (passed at commit on tree" in out and "fake_check.py" in out and "passes=1" not in out, out)

    # ---- R2
    open(os.path.join(work, "a.txt"), "w").write("a3\n"); git(work, "add", "a.txt"); git(work, "commit", "-q", "-m", "r2")
    before = count()
    open(os.path.join(work, "a.txt"), "w").write("edited on disk, not committed\n")
    p = git(work, "push", "origin", "master", ok=False)
    check("R2: with an unstaged edit on disk the push reruns the check", p.returncode == 0 and count() == before + 1,
          (count(), before, p.stdout + p.stderr))
    git(work, "checkout", "--", "a.txt")

    # ---- R3
    open(os.path.join(work, "a.txt"), "w").write("a4\n"); git(work, "add", "a.txt")
    p = git(work, "commit", "-m", "r3", ok=False, extra={"FAKE_FAIL": "1"})
    check("precondition R3: the failing check refused the commit", p.returncode != 0, p.stdout + p.stderr)
    git(work, "commit", "-q", "--no-verify", "-m", "r3")
    before = count()
    p = git(work, "push", "origin", "master", ok=False)
    check("R3: a tree whose check failed at commit is rerun at push", p.returncode == 0 and count() == before + 1,
          (count(), before, p.stdout + p.stderr))

    # ---- R4
    open(os.path.join(work, "a.txt"), "w").write("a5\n"); git(work, "add", "a.txt"); git(work, "commit", "-q", "-m", "r4")
    git(work, "branch", "-f", "side", "HEAD")
    before = count()
    p = git(work, "push", "origin", "master", "side", ok=False)
    check("R4: two refs in one push are not skipped", p.returncode == 0 and count() == before + 1,
          (count(), before, p.stdout + p.stderr))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
