"""local_ci_worktree_check.py: scripts/local_ci.py notices a Windows-made worktree and plans a WSL-native clone (q47, 2026-11-19).
A worktree made on Windows has a .git FILE whose first line is `gitdir: C:/Users/.../repo/.git/worktrees/<name>`. WSL git cannot read
that path, so local_ci.py died at once with git exit 128 and the wrapper's exit 0 hid it. Now, on Linux, the script detects it,
says so in one line, and re-runs itself from a native clone of the main repo checked out at the worktree's branch and commit.

  W1  a .git file naming a C:/ gitdir is detected on Linux (and ignored when not on Linux)
  W2  a normal .git folder, a .git file naming a Linux path, and no .git at all are not worktree cases
  W3  C:/Users/B/repo -> /mnt/c/Users/B/repo, backslashes too; a Linux path is unchanged
  W4  the plan names the main repo (the part before /.git/worktrees/) and the worktree's own gitdir, both as WSL paths
  W5  the worktree's branch is read from its HEAD file ("ref: refs/heads/wip/148-x" -> wip/148-x; a detached sha -> None)

Only the pure functions are tested here; the clone itself needs WSL and is run by hand (stated in the commit message).
Fails on the old code (the functions do not exist). Usage: python tests/repo/local_ci_worktree_check.py [repo root] -> last line
passes=N fails=M; exit 1 on FAIL."""
import importlib.util
import os
import shutil
import sys
import tempfile
import types

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPT = os.path.join(ROOT, "scripts", "local_ci.py")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


sys.modules.setdefault("yaml", types.ModuleType("yaml"))   # local_ci imports yaml at the top and uses it only to read workflows
spec = importlib.util.spec_from_file_location("local_ci_under_test", SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
need = ("windows_worktree_gitdir", "wsl_path", "native_clone_plan", "worktree_branch")
missing = [n for n in need if not hasattr(mod, n)]
check("W0 the functions exist", not missing, missing)
if not missing:
    tmp = tempfile.mkdtemp(prefix="localci-wt-")
    try:
        def repo_with(dotgit_text):
            d = tempfile.mkdtemp(dir=tmp)
            if dotgit_text is not None:
                open(os.path.join(d, ".git"), "w", encoding="utf-8").write(dotgit_text)
            return d
        gd = "C:/Users/B/Projects/LiquidGov/repo/.git/worktrees/148-x"
        check("W1 a C:/ gitdir in .git is detected on Linux", mod.windows_worktree_gitdir(repo_with("gitdir: " + gd + "\n"), linux=True) == gd)
        check("W1 ...and ignored when not on Linux", mod.windows_worktree_gitdir(repo_with("gitdir: " + gd + "\n"), linux=False) is None)
        d = tempfile.mkdtemp(dir=tmp)
        os.makedirs(os.path.join(d, ".git"))
        check("W2 a normal .git folder is not a worktree case", mod.windows_worktree_gitdir(d, linux=True) is None)
        check("W2 a .git file with a Linux path is not a worktree case",
              mod.windows_worktree_gitdir(repo_with("gitdir: /home/b/repo/.git/worktrees/x\n"), linux=True) is None)
        check("W2 no .git at all is not a worktree case", mod.windows_worktree_gitdir(repo_with(None), linux=True) is None)
        check("W3 C:/ path to /mnt/c", mod.wsl_path("C:/Users/B/repo") == "/mnt/c/Users/B/repo", mod.wsl_path("C:/Users/B/repo"))
        check("W3 backslashes and a lower-case drive", mod.wsl_path("d:\\keys\\x") == "/mnt/d/keys/x", mod.wsl_path("d:\\keys\\x"))
        check("W3 a Linux path is unchanged", mod.wsl_path("/home/b/x") == "/home/b/x")
        main_repo, wt_gitdir = mod.native_clone_plan(gd)
        check("W4 the plan names the main repo as a WSL path", main_repo == "/mnt/c/Users/B/Projects/LiquidGov/repo", main_repo)
        check("W4 ...and the worktree's own gitdir as a WSL path", wt_gitdir == "/mnt/c/Users/B/Projects/LiquidGov/repo/.git/worktrees/148-x", wt_gitdir)
        check("W5 a branch ref in HEAD gives the branch", mod.worktree_branch("ref: refs/heads/wip/148-x\n") == "wip/148-x")
        check("W5 a detached sha gives None", mod.worktree_branch("3b9d5ba4f1c2e8a7d6b5c4e3f2a1b0c9d8e7f6a5\n") is None)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
