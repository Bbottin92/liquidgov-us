"""tidy_worktrees_check.py: scripts/tidy_worktrees.py removes only merged, clean worktrees (2026-11-08).

In a throwaway repo it builds one worktree per case and checks: dry run removes nothing; "--live true" stays a dry run;
"--live yes" removes ONLY the merged+clean one (and its branch); an unmerged, a dirty, one with an ignored .lgp store,
and one with a junction are kept with their reason; the junction's target survives. Never touches the real repo.
Usage: python tests/repo/tidy_worktrees_check.py <repo root>   (exit 0 only when every case passed)"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent.parent).resolve()
TOOL = ROOT / "scripts" / "tidy_worktrees.py"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}  # lesson: hooks export GIT_DIR etc.
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


def git(cwd, *a):
    return subprocess.run(["git", "-C", str(cwd)] + list(a), capture_output=True, text=True, env=ENV)


def tool(repo, *a):
    p = subprocess.run([sys.executable, str(TOOL), "--repo", str(repo)] + list(a), capture_output=True, text=True, env=ENV,
                       timeout=120)
    return p.returncode, p.stdout + p.stderr


tmp = Path(tempfile.mkdtemp(prefix="tidy-check-"))
target = tmp / "junction-target"
try:
    check("precondition: the tool exists", TOOL.is_file(), TOOL)
    main = tmp / "main"
    main.mkdir()
    for a in (["init", "-q", "-b", "master"], ["config", "user.email", "t@t"], ["config", "user.name", "t"],
              ["config", "core.autocrlf", "false"]):
        git(main, *a)
    (main / ".gitignore").write_text("*.lgp\nbuild/\n")
    (main / "a.txt").write_text("base\n")
    git(main, "add", "."), git(main, "commit", "-q", "-m", "base")
    wt = {}
    for name in ("clean", "unmerged", "dirty", "store", "junction"):
        wt[name] = tmp / ("wt-" + name)
        git(main, "worktree", "add", "-q", "-b", "wip/" + name, str(wt[name]), "master")
    (wt["unmerged"] / "u.txt").write_text("only on this branch\n")
    git(wt["unmerged"], "add", "."), git(wt["unmerged"], "commit", "-q", "-m", "unmerged work")
    (wt["dirty"] / "a.txt").write_text("changed\n")
    (wt["store"] / "accounts.lgp").write_text("real data\n")
    (wt["clean"] / "build").mkdir()
    (wt["clean"] / "build" / "x.o").write_text("obj\n")  # build output alone does not keep a worktree
    # a NESTED __pycache__ is build output too (2026-11-08: the first real dry run kept 9 worktrees for
    # tests/repo/__pycache__/ alone)
    (wt["clean"] / "sub" / "tests" / "__pycache__").mkdir(parents=True)
    (wt["clean"] / "sub" / "tests" / "__pycache__" / "m.pyc").write_text("pyc\n")
    (main / ".gitignore").write_text("*.lgp\nbuild/\n__pycache__/\n")
    git(main, "add", "."), git(main, "commit", "-q", "-m", "ignore pycache")
    for name in wt:
        git(wt[name], "merge", "-q", "--ff-only", "master") if name != "unmerged" else None
    target.mkdir()
    (target / "keep.txt").write_text("must survive\n")
    made = subprocess.run(["cmd", "/c", "mklink", "/J", str(wt["junction"] / "linked"), str(target)],
                          capture_output=True, text=True)
    check("precondition: the junction was made", (wt["junction"] / "linked" / "keep.txt").exists(), made.stdout + made.stderr)

    rc, out = tool(main)
    check("dry run lists the clean one for removal", "REMOVE" in out and "wt-clean" in out and "dry run" in out, out)
    check("...and removes nothing", all(p.exists() for p in wt.values()), out)
    rc, out = tool(main, "--live", "true")
    check('"--live true" stays a dry run', "staying a dry run" in out and wt["clean"].exists(), out)

    rc, out = tool(main, "--live", "yes")
    check("live removes the merged+clean worktree", not wt["clean"].exists() and "REMOVED" in out, out)
    check("...and its branch", "wip/clean" not in git(main, "branch").stdout, git(main, "branch").stdout)
    check("an unmerged branch is kept", wt["unmerged"].exists() and "not merged" in out, out)
    check("a dirty worktree is kept", wt["dirty"].exists() and "uncommitted" in out, out)
    check("an ignored .lgp store keeps its worktree", (wt["store"] / "accounts.lgp").exists() and "accounts.lgp" in out, out)
    check("a junction keeps its worktree, and the target survives",
          wt["junction"].exists() and (target / "keep.txt").exists() and "junction" in out, out)
    check("the main checkout is never touched", (main / "a.txt").exists() and "main checkout" in out, out)
    check("exit 0 when every removal succeeded", rc == 0, rc)
finally:
    j = tmp / "wt-junction" / "linked"
    if j.exists():
        subprocess.run(["cmd", "/c", "rmdir", str(j)], capture_output=True)  # rule #6f37f: unlink the junction first
    shutil.rmtree(tmp, ignore_errors=True)
print("tidy_worktrees_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
