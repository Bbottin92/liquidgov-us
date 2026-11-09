"""Remove liquidgov-us worktrees that are merged into master and hold nothing else (Brandon OK'd 2026-11-08: "Ports +
worktree tidy tool", "Merged liquidgov worktrees"). jam-partner has tools/tidy_worktrees.py; this is liquidgov's.

DRY RUN unless the live switch is the exact string `--live yes` (hard rule #412ea: anything that deletes defaults to a
dry run). A worktree is removed only when ALL hold, else it is kept and the reason printed:
  - its branch is an ancestor of master (merged);
  - `git status --porcelain` is empty (no changes, no untracked files);
  - no IGNORED files except build output (a local *.lgp store or .env would be lost: memory "never delete shared
    test data"), see KEEP_IF_IGNORED;
  - no junction or symlink inside (hard rule #6f37f: a plain delete follows a junction into its target).
Removal is `git worktree remove <path>` (git itself refuses a dirty tree; never --force) then `git branch -d <branch>`
(refuses an unmerged branch). Usage: python scripts/tidy_worktrees.py [--repo <main checkout>] [--live yes]
Exit 0 when every step that ran succeeded; 1 when any removal failed (each failure printed)."""
import argparse
import os
import subprocess
import sys
from pathlib import Path

BUILD_DIRS = ("build/", "liquidgov-platform/build/")
CACHE_SEGMENTS = {"__pycache__", ".pytest_cache"}  # anywhere in the path (2026-11-08: nested ones kept 9 worktrees)


def is_build_output(rel):
    return rel.startswith(BUILD_DIRS) or bool(CACHE_SEGMENTS & set(rel.rstrip("/").split("/")))


def git(cwd, *args):
    p = subprocess.run(["git", "-C", str(cwd)] + list(args), capture_output=True, text=True, encoding="utf-8",
                       errors="replace")
    return p.returncode, p.stdout, p.stderr


def worktrees(repo):
    rc, out, err = git(repo, "worktree", "list", "--porcelain")
    if rc != 0:
        raise SystemExit("tidy_worktrees: git worktree list failed: " + err.strip())
    items, cur = [], {}
    for line in out.splitlines():
        if line.startswith("worktree "):
            if cur:
                items.append(cur)
            cur = {"path": Path(line[9:])}
        elif line.startswith("branch "):
            cur["branch"] = line[7:].replace("refs/heads/", "", 1)
        elif line == "detached":
            cur["branch"] = None
    if cur:
        items.append(cur)
    return items


def has_links(root):
    """First junction/symlink found within the tree (not following it), or None. A .git FILE is normal for a worktree."""
    for dirpath, dirnames, filenames in os.walk(root):
        for name in dirnames + filenames:
            p = Path(dirpath) / name
            try:
                if p.is_symlink() or (hasattr(p, "is_junction") and p.is_junction()):
                    return p
            except OSError:  # err-ok: an unreadable entry is reported as a reason to keep the worktree
                return p
        dirnames[:] = [d for d in dirnames if d != ".git" and not (Path(dirpath) / d).is_symlink()]
    return None


def judge(repo, wt):
    path, branch = wt["path"], wt.get("branch")
    if path.resolve() == Path(repo).resolve():
        return "main checkout"
    if not branch:
        return "detached HEAD"
    if branch in ("master", "main"):
        return "on " + branch
    if not path.exists():
        return "folder missing (run git worktree prune by hand after checking)"
    rc, _, _ = git(repo, "merge-base", "--is-ancestor", branch, "master")
    if rc != 0:
        return "branch not merged into master"
    rc, out, err = git(path, "status", "--porcelain")
    if rc != 0:
        return "git status failed: " + err.strip()[:120]
    if out.strip():
        return "%d uncommitted/untracked path(s)" % len(out.splitlines())
    # --untracked-files=all: without it git collapses a folder holding only ignored files into "sub/", hiding that it
    # is only __pycache__ (measured in tidy_worktrees_check, 2026-11-08)
    rc, out, _ = git(path, "status", "--porcelain", "--ignored", "--untracked-files=all")
    ignored = [l[3:] for l in out.splitlines() if l.startswith("!! ")]
    kept = [i for i in ignored if not is_build_output(i)]
    if kept:
        return "ignored files that are not build output: " + ", ".join(kept[:5])
    link = has_links(path)
    if link:
        return "contains a junction/symlink: %s" % link
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--live", default="", help='only the exact value "yes" removes anything')
    a = ap.parse_args(argv)
    live = a.live == "yes"
    if a.live and not live:
        print('tidy_worktrees: --live must be exactly "yes"; staying a dry run')
    rc_all = 0
    for wt in worktrees(a.repo):
        why = judge(a.repo, wt)
        label = "%s [%s]" % (wt["path"], wt.get("branch"))
        if why:
            print("KEEP   %s: %s" % (label, why))
            continue
        if not live:
            print("REMOVE %s (dry run)" % label)
            continue
        rc, _, err = git(a.repo, "worktree", "remove", str(wt["path"]))
        if rc != 0:
            print("FAILED %s: git worktree remove: %s" % (label, err.strip()))
            rc_all = 1
            continue
        rc, _, err = git(a.repo, "branch", "-d", wt["branch"])
        print(("REMOVED %s" % label) if rc == 0 else ("REMOVED %s; branch kept: %s" % (label, err.strip())))
        if rc != 0:
            rc_all = 1
    print("tidy_worktrees: %s" % ("done" if live else "dry run (nothing removed; add --live yes)"))
    return rc_all


if __name__ == "__main__":
    sys.exit(main())
