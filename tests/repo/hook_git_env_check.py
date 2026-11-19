#!/usr/bin/env python3
"""The checks never get git's hook variables, so they can never act on the repository they are run from (2026-11-14).
Git runs a hook with GIT_DIR, GIT_INDEX_FILE and friends set. .githooks/pre-push (pre-commit execs it) passed them on
to every check, and prebuilt_check.py's throwaway repo (git init, commit "one", commit "two") then landed in the real
one: core.bare=true in the main checkout, and "one"/"two" commits on a linked worktree's branch (twice, 2026-11-12).
Reproduced 2026-11-14 with a hook that looked after each check: only prebuilt_check.py did it, from pre-commit (the
core.bare flip) and from pre-push (the flip and both commits).

Here: a throwaway repo with a bare origin and a linked worktree gets the real .githooks, the real selector and ONE
check, a probe that writes down the GIT_* variables it was given and then does what prebuilt_check.py does (git init,
add, commit "one" in a temp folder). A real `git commit` and a real `git push` run the hooks, in the linked worktree
and in the main checkout. After each: the probe ran, saw no GIT_* variable, core.bare is still false, and the branch
has exactly the commits made here. scripts/run_hook_checks.py (CI, by hand) gets the same probe with a hostile GIT_DIR.
This file's own git calls drop GIT_* too, so running it from a hook on the unfixed code harms nothing.
    python3 hook_git_env_check.py [repo root]
Prints 'hook_git_env_check: N checks, F failed'; exit 1 if any failed."""
import os
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
checks = failed = 0
# GIT_*, LG_CHECK_MODE and bytecode handled by the shared helper (hook_env_isolation_check.py keeps it that way)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "scripts"))
from hook_env import clean_env  # noqa: E402  (scripts/hook_env.py)
CLEAN = clean_env()

PROBE = r'''#!/usr/bin/env python3
"""probe: records the GIT_* variables a check is given, then builds a throwaway repo the way prebuilt_check.py does."""
import os, subprocess, sys, tempfile
seen = sorted(k for k in os.environ if k.startswith("GIT_"))
with open(os.environ["LG_PROBE_OUT"], "a", encoding="utf-8") as f:
    f.write("RAN " + " ".join(seen) + "\n")
with tempfile.TemporaryDirectory() as repo:
    os.makedirs(os.path.join(repo, "liquidgov-platform", "src"))
    with open(os.path.join(repo, "liquidgov-platform", "src", "a.cpp"), "w") as f:
        f.write("int a = 1;\n")
    for a in (["init", "-q"], ["add", "-A"], ["commit", "-qm", "one"]):
        subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t"] + a, cwd=repo, capture_output=True,
                       stdin=subprocess.DEVNULL)
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
    return subprocess.run(cmd + list(args), cwd=cwd, capture_output=True, text=True, env=env or CLEAN,
                          stdin=subprocess.DEVNULL)


def out(cwd, *args):
    return git(cwd, *args).stdout.strip()


def rmtree(path):
    def writable(func, p, _):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    shutil.rmtree(path, onerror=writable)


def build(tmp):
    """main checkout on master + linked worktree on wip/1-probe + bare origin, hooks on. Returns (main, wt, origin)."""
    main, wt, origin = (os.path.join(tmp, n) for n in ("main", "wt", "origin.git"))
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
    git(main, "commit", "-qm", "base")  # hooks are not on yet
    git(main, "remote", "add", "origin", origin)
    git(main, "push", "-q", "origin", "master")
    git(main, "worktree", "add", "-q", "-b", "wip/1-probe", wt)
    git(main, "config", "core.hooksPath", ".githooks")
    return main, wt, origin


def probe_lines(path):
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip()]


def state(main, tree):
    return {"bare": out(main, "config", "--get", "core.bare"),
            "subjects": out(tree, "log", "--format=%s").splitlines(),
            "files": sorted(out(tree, "ls-files").splitlines()),
            "status": git(tree, "status", "--porcelain")}


def judge(label, main, tree, probe_out, r, expect_subjects, files):
    lines = probe_lines(probe_out)
    check(r.returncode == 0, "%s: the git command succeeds" % label, (r.returncode, (r.stdout + r.stderr)[-300:]))
    check(len(lines) >= 1, "%s: the hook ran the probe check (precondition)" % label, (r.stdout + r.stderr)[-300:])
    seen = sorted(set(" ".join(l[4:] for l in lines).split()))
    check(lines and not seen, "%s: the check is given no GIT_* variable" % label, seen)
    s = state(main, tree)
    check(s["bare"] == "false", "%s: core.bare in the main checkout is still false" % label, s["bare"])
    check(s["subjects"] == expect_subjects, "%s: the branch has only the commits made here (no 'one')" % label,
          s["subjects"])
    check(s["files"] == files and "liquidgov-platform/src/a.cpp" not in s["files"],
          "%s: the tracked files are unchanged" % label, s["files"])
    check(s["status"].returncode == 0 and s["status"].stdout.strip() == "",
          "%s: git status works and is clean" % label, (s["status"].stdout + s["status"].stderr)[-200:])
    if os.path.exists(probe_out):
        os.remove(probe_out)


def repair(main):
    """so a failure in one case does not also fail the next for the same reason"""
    git(main, "config", "core.bare", "false")


def run_cases(tmp):
    main, wt, origin = build(tmp)
    probe_out = os.path.join(tmp, "probe.out")
    env = dict(CLEAN, LG_PROBE_OUT=probe_out)
    files = sorted(out(main, "ls-files").splitlines())
    check(out(main, "config", "--get", "core.bare") == "false" and len(files) == 6,
          "precondition: throwaway repo built (main checkout, linked worktree, hooks on)", files)

    for label, tree, branch in (("linked worktree", wt, "wip/1-probe"), ("main checkout", main, "master")):
        # pre-commit (every check: the staged file is not in the selector's map, the probe is an unmapped check)
        with open(os.path.join(tree, "notes.md"), "a", encoding="utf-8", newline="\n") as f:
            f.write(label + "\n")
        git(tree, "add", "notes.md")
        r = git(tree, "commit", "-qm", "edit in " + label, env=env)
        want = ["edit in " + label, "base"]
        tip = out(tree, "rev-parse", "HEAD")
        judge("commit in a " + label, main, tree, probe_out, r, want, files)
        repair(main)
        if out(tree, "rev-parse", "HEAD") != tip or out(tree, "log", "-1", "--format=%s") != want[0]:
            git(tree, "reset", "-q", "--hard", tip)
        # pre-push, by explicit commit id as in the 2026-11-12 sightings
        r = git(tree, "push", "-q", "origin", "%s:refs/heads/%s" % (out(tree, "rev-parse", "HEAD"), branch), env=env)
        judge("push from a " + label, main, tree, probe_out, r, want, files)
        repair(main)

    # the commit -a path: the selector must still read git's temporary index (the variables go only AFTER it)
    with open(os.path.join(wt, "scripts", "affected_checks.py"), "a", encoding="utf-8", newline="\n") as f:
        f.write("# touched\n")
    r = git(wt, "commit", "-qam", "selector edit", env=env)
    check(r.returncode == 0 and "hook or selector changed (scripts/affected_checks.py)" in r.stderr,
          "commit -a: the selector still sees the files being committed", (r.returncode, r.stderr[-300:]))
    if os.path.exists(probe_out):
        os.remove(probe_out)
    repair(main)

    # CI and by hand: run_hook_checks.py, started with a hostile GIT_DIR/GIT_INDEX_FILE (as if from inside a hook)
    gitdir = out(wt, "rev-parse", "--absolute-git-dir")
    tip = out(wt, "rev-parse", "HEAD")
    hostile = dict(env, GIT_DIR=gitdir, GIT_INDEX_FILE=os.path.join(gitdir, "index"))
    r = subprocess.run([sys.executable, os.path.join(main, "scripts", "run_hook_checks.py"), main],
                       capture_output=True, text=True, env=hostile, stdin=subprocess.DEVNULL)
    lines = probe_lines(probe_out)
    check(r.returncode == 0 and len(lines) == 1, "run_hook_checks.py: runs the probe check", (r.returncode, r.stdout[-300:]))
    check(lines == ["RAN"], "run_hook_checks.py: the check is given no GIT_* variable", lines)
    check(out(main, "config", "--get", "core.bare") == "false" and out(wt, "rev-parse", "HEAD") == tip,
          "run_hook_checks.py: core.bare and the branch tip are untouched",
          (out(main, "config", "--get", "core.bare"), out(wt, "log", "-3", "--format=%s")))


tmp = tempfile.mkdtemp(prefix="lg-hook-git-env-")
try:
    run_cases(tmp)
except Exception as e:  # a broken case is a failure, never a quiet pass
    check(False, "the cases ran to the end", repr(e))
finally:
    rmtree(tmp)

print("hook_git_env_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
