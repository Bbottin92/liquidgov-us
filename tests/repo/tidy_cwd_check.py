"""tidy_cwd_check.py: scripts/tidy_worktrees.py never removes the worktree the caller is standing in (2026-11-08: run
from a shell whose cwd was inside liquidgov-us-archive, git unregistered it, failed to delete the folder (Permission
denied) and left an empty folder plus an undeleted branch). Throwaway repo; never touches the real one.
Usage: python tests/repo/tidy_cwd_check.py <repo root>   (exit 0 only when every case passed)"""
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent.parent).resolve()
TOOL = ROOT / "scripts" / "tidy_worktrees.py"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


def git(cwd, *a):
    return subprocess.run(["git", "-C", str(cwd)] + list(a), capture_output=True, text=True, env=ENV)


def rmtree(p):
    def again(func, path, _exc):
        os.chmod(path, stat.S_IWRITE)
        func(path)
    shutil.rmtree(p, onexc=again)


tmp = Path(tempfile.mkdtemp(prefix="tidy-cwd-"))
try:
    main = tmp / "main"
    main.mkdir()
    for a in (["init", "-q", "-b", "master"], ["config", "user.email", "t@t"], ["config", "user.name", "t"]):
        git(main, *a)
    (main / "a.txt").write_text("base\n")
    git(main, "add", "."), git(main, "commit", "-q", "-m", "base")
    here, other = tmp / "wt-here", tmp / "wt-other"
    git(main, "worktree", "add", "-q", "-b", "wip/here", str(here), "master")
    git(main, "worktree", "add", "-q", "-b", "wip/other", str(other), "master")
    (here / "sub").mkdir()
    p = subprocess.run([sys.executable, str(TOOL), "--repo", str(main), "--live", "yes"], cwd=str(here / "sub"),
                       capture_output=True, text=True, env=ENV, timeout=120)
    out = p.stdout + p.stderr
    check("precondition: the other merged clean worktree is removed", not other.exists(), out)
    check("the worktree holding the caller's cwd is kept, with that reason",
          here.exists() and "current directory" in out and "wip/here" in git(main, "branch").stdout, out)
finally:
    rmtree(tmp)
print("tidy_cwd_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
