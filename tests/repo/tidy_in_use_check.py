"""tidy_in_use_check.py: scripts/tidy_worktrees.py keeps a worktree folder another process is using (2026-11-08, twice:
the Claude session's shell had its cwd in the worktree; git unregistered it, could not delete the folder - Permission
denied - and left an empty folder plus the branch). The tool's own-cwd guard could not see a different process, so the
tool now probes the folder (rename away and back) before git touches it. Here a second process holds its cwd in the
worktree, as the session shell did. Windows only (that is where folders in use cannot be renamed).
Usage: python tests/repo/tidy_in_use_check.py <repo root>   (exit 0 only when every case passed)"""
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time
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


if os.name != "nt":
    print("tidy_in_use_check: passes=0 fails=0 (skipped: not Windows)")
    sys.exit(0)
tmp = Path(tempfile.mkdtemp(prefix="tidy-inuse-"))
holder = None
try:
    main = tmp / "main"
    main.mkdir()
    for a in (["init", "-q", "-b", "master"], ["config", "user.email", "t@t"], ["config", "user.name", "t"]):
        git(main, *a)
    (main / "a.txt").write_text("base\n")
    git(main, "add", "."), git(main, "commit", "-q", "-m", "base")
    busy, free = tmp / "wt-busy", tmp / "wt-free"
    git(main, "worktree", "add", "-q", "-b", "wip/busy", str(busy), "master")
    git(main, "worktree", "add", "-q", "-b", "wip/free", str(free), "master")
    holder = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(120)"], cwd=str(busy))
    time.sleep(1)
    p = subprocess.run([sys.executable, str(TOOL), "--repo", str(main), "--live", "yes"], cwd=str(main),
                       capture_output=True, text=True, env=ENV, timeout=120)
    out = p.stdout + p.stderr
    check("precondition: the free merged worktree is removed", not free.exists(), out)
    check("the worktree another process stands in is kept, with that reason, and nothing half-done",
          busy.exists() and "in use" in out and "FAILED" not in out, out)
    check("...it is still registered and its branch kept", str(busy).replace("\\", "/").lower() in
          git(main, "worktree", "list").stdout.replace("\\", "/").lower() and "wip/busy" in git(main, "branch").stdout, out)
finally:
    if holder:
        holder.kill()
        holder.wait()
    rmtree(tmp)
print("tidy_in_use_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
