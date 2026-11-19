"""local_ci: run the repo's GitHub workflows here instead of on GitHub, and write a pass record the deploy gate accepts.
Brandon 2026-11-15: "can we skip github if it costs money now?" (the repo is private; GitHub Actions minutes ran out
and runs were blocked by billing). Keeping the code on GitHub is free; only the Actions runs cost. This runs every
`run:` step of .github/workflows/platform.yml and hook-checks.yml, in order,
the way GitHub would on a push, with the same rules: a step marked continue-on-error (the report-only browser checks)
is reported but never fails the run; any other failing step does.

When everything that gates passes, it writes deploy/ci-pass/<liquidgov-platform tree hash>.txt, first line
"LIQUIDGOV_LOCAL_CI PASS", with the commit, the time, each step's result and the phone VERDICT lines. Commit that file
(it touches only deploy/, so the platform tree, and the record's name, stay the same) and push; deploy/ci-gate.sh and
scripts/ship.py accept it in place of a GitHub run for exactly that tree.

Runs each workflow on the branches it used to start on (--only forces one).
Usage: python3 scripts/local_ci.py [--build DIR] [--only platform|hook-checks] [--no-record]
  --build DIR: the build folder to use (and reuse between runs); default ~/.cache/liquidgov-local-ci/lg
Refuses to run with uncommitted changes (the record must describe a commit). Prints LOCAL_CI_PASS or LOCAL_CI_FAIL
last; exit 0 / 1. Steps that only set up a GitHub runner (checkout, caches, installing tools, uploads) are skipped:
the machine running this needs cmake, a compiler, python3, node and the pre-installed Chromium (LG_BROWSER)."""
import datetime
import fnmatch
import os
import re
import subprocess
import sys
import tempfile

import yaml

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORKFLOWS = ["platform", "hook-checks"]
# setup steps a GitHub runner needs and this machine does not (by step name, start of the name)
SKIP_NAMES = ("tools the build", "did the platform change", "git identity")
# The branches each workflow started on before it was made manual-only (2026-11-15; see git history): platform and
# hook-checks on master and wip/** (a third, commons, on master only, was removed with the Commons 2026-11-15). A run
# here follows the same triggers, so the pass record
# means what a green GitHub run used to mean; `--only <name>` runs one whatever the branch.
TRIGGER_BRANCHES = {"platform": ("master", "wip/*"), "hook-checks": ("master", "wip/*")}


def git(*args):
    return subprocess.run(["git", "-C", REPO] + list(args), check=True, capture_output=True, text=True).stdout.strip()


def windows_worktree_gitdir(repo, linux=None):
    """The `C:/...` gitdir a Windows-made worktree's .git FILE names, when this runs on Linux; else None (q47, 2026-11-19).
    WSL git cannot read that path, so every git call here would die with exit 128."""
    if linux is None:
        linux = os.name == "posix"
    dotgit = os.path.join(repo, ".git")
    if not linux or not os.path.isfile(dotgit):
        return None
    try:
        with open(dotgit, encoding="utf-8") as f:
            first = f.readline().strip()
    except OSError:
        return None
    m = re.match(r"gitdir:\s*([A-Za-z]:[\\/].*)$", first)
    return m.group(1) if m else None


def wsl_path(p):
    """C:/Users/B/x or C:\\Users\\B\\x -> /mnt/c/Users/B/x; anything else unchanged."""
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", p)
    return "/mnt/%s/%s" % (m.group(1).lower(), m.group(2).replace("\\", "/")) if m else p


def native_clone_plan(gitdir):
    """(main repo as a WSL path, the worktree's own gitdir as a WSL path) from `C:/.../repo/.git/worktrees/<name>`."""
    g = gitdir.replace("\\", "/")
    return wsl_path(g.split("/.git/worktrees/")[0]), wsl_path(g)


def worktree_branch(head_text):
    """The branch named in a worktree's HEAD file ('ref: refs/heads/wip/x' -> 'wip/x'), None for a detached sha."""
    m = re.match(r"ref:\s*refs/heads/(\S+)", head_text.strip())
    return m.group(1) if m else None


def rerun_in_native_clone(gitdir, argv):
    """Run this same script from a WSL-native LOCAL clone of the main repo at the worktree's branch and commit (no GitHub:
    the clone's origin is the main repo's folder). Returns the exit code; every failure is printed, never swallowed."""
    main_repo, wt_gitdir = native_clone_plan(gitdir)
    cache = os.path.expanduser("~/.cache/liquidgov-local-ci/clone")

    def g(*a):
        return subprocess.run(["git"] + list(a), check=True, capture_output=True, text=True).stdout.strip()
    try:
        with open(os.path.join(wt_gitdir, "HEAD"), encoding="utf-8") as f:
            head = f.read()
        branch = worktree_branch(head)
        sha = g("-C", main_repo, "rev-parse", branch or head.strip())
        if os.path.isdir(os.path.join(cache, ".git")):
            g("-C", cache, "fetch", "-q", "origin")           # origin is the local main repo folder
        else:
            os.makedirs(os.path.dirname(cache), exist_ok=True)
            g("clone", "-q", "--no-hardlinks", main_repo, cache)
        g("-C", cache, "checkout", "-q", "-B", branch, sha) if branch else g("-C", cache, "checkout", "-q", "--detach", sha)
    except (subprocess.CalledProcessError, OSError) as e:
        print("LOCAL_CI_FAIL this is a Windows-made worktree (.git names %s) and the WSL-native clone could not be set up: %s"
              % (gitdir, (getattr(e, "stderr", "") or str(e)).strip()[-200:]))
        return 1
    print("LOCAL_CI_NOTE Windows-made worktree (.git names %s): WSL git cannot read it, so this runs from a WSL-native clone at %s "
          "(a local clone of %s, no GitHub) at %s on %s" % (gitdir, cache, main_repo, sha[:7], branch or "a detached head"))
    return subprocess.run([sys.executable, os.path.join(cache, "scripts", "local_ci.py")] + list(argv)).returncode


def expand(text, workspace):
    # GitHub's ${{ }} expressions, as a push to a branch would fill them
    def one(m):
        e = m.group(1).strip()
        if e == "github.workspace":
            return workspace
        if e == "github.event_name":
            return "push"
        return ""
    return re.sub(r"\$\{\{(.*?)\}\}", one, text)


def main():
    args = sys.argv[1:]
    build = os.path.expanduser("~/.cache/liquidgov-local-ci/lg")
    only, record = None, True
    while args:
        a = args.pop(0)
        if a == "--build" and args:
            build = os.path.abspath(args.pop(0))
        elif a == "--only" and args:
            only = args.pop(0)
        elif a == "--no-record":
            record = False
        else:
            print(__doc__)
            return 2
    wt = windows_worktree_gitdir(REPO)
    if wt:
        return rerun_in_native_clone(wt, sys.argv[1:])   # q47: before any git call, which would die with exit 128 here
    if git("status", "--porcelain"):
        print("LOCAL_CI_FAIL uncommitted changes: commit first, so the record describes a commit")
        return 1
    if git("rev-parse", "--is-shallow-repository") == "true":
        subprocess.run(["git", "-C", REPO, "fetch", "-q", "--unshallow"], check=False)   # some hook checks read history
    subprocess.run(["git", "-C", REPO, "fetch", "-q", "origin", "master"], check=False)
    head = git("rev-parse", "HEAD")
    branch = git("rev-parse", "--abbrev-ref", "HEAD")
    tree = git("rev-parse", "HEAD:liquidgov-platform")
    runner = tempfile.mkdtemp(prefix="lg-local-ci-")
    os.makedirs(build, exist_ok=True)   # the build folder itself, not just its parent: on a fresh machine the symlink below
    #                                     would dangle and cmake could not create CMakeFiles through it (q49, 2026-11-19)
    os.symlink(build, os.path.join(runner, "lg"))   # $RUNNER_TEMP/lg is the build, kept between runs
    env = dict(os.environ, RUNNER_TEMP=runner, GITHUB_WORKSPACE=REPO, CI="true",
               GIT_AUTHOR_NAME=os.environ.get("GIT_AUTHOR_NAME", "local-ci"), GIT_AUTHOR_EMAIL="local-ci@localhost",
               GIT_COMMITTER_NAME="local-ci", GIT_COMMITTER_EMAIL="local-ci@localhost")
    if not env.get("LG_BROWSER"):
        for b in ("/opt/pw-browsers/chromium-1243/chrome-linux64/chrome", "/opt/pw-browsers/chromium"):
            if os.path.exists(b):
                env["LG_BROWSER"] = b
                break
    results, verdicts, failed = [], set(), []
    for wf in WORKFLOWS:
        if only and wf != only:
            continue
        if not only and not any(fnmatch.fnmatch(branch, g) for g in TRIGGER_BRANCHES[wf]):
            results.append("skip  %s (it started only on %s, not on %s)" % (wf, " and ".join(TRIGGER_BRANCHES[wf]), branch))
            continue
        spec = yaml.safe_load(open(os.path.join(REPO, ".github", "workflows", wf + ".yml")))
        for jname, job in spec["jobs"].items():
            jenv = {k: expand(str(v), REPO) for k, v in (job.get("env") or {}).items()}
            for st in job.get("steps", []):
                if "run" not in st:
                    continue
                name = st.get("name", "(unnamed)")
                if name.lower().startswith(SKIP_NAMES):
                    continue
                if str(st.get("if", "")).strip() == "failure()":
                    continue
                cwd = os.path.join(REPO, st.get("working-directory", "")) if st.get("working-directory") else REPO
                senv = dict(env, **jenv, **{k: expand(str(v), REPO) for k, v in (st.get("env") or {}).items()})
                # a browser step finds the browser itself on a runner (command -v google-chrome ...): keep ours
                script = expand(st["run"], REPO).replace(
                    'export LG_BROWSER="$(command -v google-chrome || command -v chromium || true)"',
                    'export LG_BROWSER="${LG_BROWSER:-$(command -v google-chrome || command -v chromium || true)}"')
                timeout = int(st.get("timeout-minutes", 30)) * 60
                print("== %s / %s / %s" % (wf, jname, name), flush=True)
                try:
                    p = subprocess.run(["bash", "-c", script], cwd=cwd, env=senv, capture_output=True, text=True,
                                       timeout=timeout)
                    out, code = (p.stdout or "") + (p.stderr or ""), p.returncode
                except subprocess.TimeoutExpired:
                    out, code = "timed out after %d s" % timeout, 124
                for line in out.splitlines():
                    if "VERDICT " in line:
                        verdicts.add(line[line.index("VERDICT "):].strip())
                report_only = bool(st.get("continue-on-error"))
                mark = "pass" if code == 0 else ("warn" if report_only else "FAIL")
                results.append("%s  %s / %s / %s" % (mark, wf, jname, name))
                print("   %s (exit %d)" % (mark, code), flush=True)
                if code != 0:
                    tail = "\n".join(out.splitlines()[-15:])
                    print("   " + tail.replace("\n", "\n   "), flush=True)
                    if not report_only:
                        failed.append("%s / %s / %s" % (wf, jname, name))
    ok = not failed
    print()
    for r in results:
        print(r)
    for v in sorted(verdicts):
        print(v)
    if ok and record and not only:
        path = os.path.join(REPO, "deploy", "ci-pass", tree + ".txt")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write("LIQUIDGOV_LOCAL_CI PASS\n")
            f.write("tree %s\ncommit %s\nwhen %s\n" % (tree, head, datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")))
            for r in results:
                f.write(r + "\n")
            for v in sorted(verdicts):
                f.write(v + "\n")
        print("record: %s (commit and push it; the deploy gate reads it)" % os.path.relpath(path, REPO))
    print("LOCAL_CI_PASS" if ok else "LOCAL_CI_FAIL " + "; ".join(failed))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
