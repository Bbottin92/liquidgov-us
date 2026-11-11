"""ship: one read-only report that says whether a branch is ready to merge and deploy, and the exact next commands.
Brandon 2026-11-11 ("yes fix the guard, then do ship"): a one-file change took ~15 hand-run steps, each a place to
slip. ship runs the checks; Brandon's one word (merge / go) still does the changing.
Usage: python scripts/ship.py check <branch> [--wait MIN] [--out DIR]
       python scripts/ship.py merge <branch> [--wait MIN] [--out DIR]   (only on Brandon's "merge": runs check, then
         moves origin/master to exactly the branch head by fast-forward push, never --force; last line SHIP_MERGED)
  1. pushed: <branch> == origin/<branch>             2. fast-forward: origin/master is an ancestor of <branch>
  3. files changed vs origin/master, and the deploy pair make-go.py auto would pick (or "no deploy needed")
  4. CI: the newest platform.yml run for exactly the branch head commit; waits up to --wait minutes (default 20)
  5. phone VERDICT lines from that run's log; ALL-PASS required when site files (src/, assets/) changed
  6. the phone-screenshots artifact downloaded to --out (default ~/scratch/<date>-claude-ship/<branch>), PNGs listed
Last line: SHIP_READY <branch> or SHIP_NOT_READY <branch>: <reasons>; exit 0 when ready, 1 when not, 2 on bad usage.
Nothing here merges, pushes or deploys. A gh or git error counts as not ready (fails closed).
Test seams: SHIP_GH (JSON list, the gh command), SHIP_REPO (repo root), SHIP_NO_FETCH=1, SHIP_POLL_SECONDS.
Tests: commons-daemon/tests/ship_check.py"""
import datetime
import json
import os
import subprocess
import sys
import time

REPO = os.environ.get("SHIP_REPO") or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GH = json.loads(os.environ["SHIP_GH"]) if os.environ.get("SHIP_GH") else ["gh"]
GH_REPO = ["--repo", "Bbottin92/liquidgov-us"]
POLL = float(os.environ.get("SHIP_POLL_SECONDS") or 30)
PLATFORM = "liquidgov-platform/"
NO_RESTART = (PLATFORM + "assets/", PLATFORM + "tests/")
SITE = (PLATFORM + "src/", PLATFORM + "assets/")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}


class Fail(Exception):
    pass


def run(cmd, what):
    try:
        p = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, encoding="utf-8", errors="replace",
                           timeout=300, env=ENV)
    except (OSError, subprocess.SubprocessError) as e:
        raise Fail("%s could not run: %s" % (what, e))
    if p.returncode != 0:
        raise Fail("%s failed (exit %d): %s" % (what, p.returncode, (p.stderr or p.stdout).strip()[:300]))
    return p.stdout


def git(*args):
    return run(["git"] + list(args), "git " + args[0]).strip()


def pair_for(files):
    site = [f for f in files if f.startswith(PLATFORM)]
    if not site:
        return "none", "no liquidgov-platform/ file changes: no deploy needed"
    if all(f.startswith(NO_RESTART) for f in site):
        return "jbshell", "assets/tests only: no build, no restart"
    return "care", "code changes: build + restart"


def newest_run(sha):
    out = run(GH + ["run", "list"] + GH_REPO + ["--workflow", "platform.yml", "--commit", sha, "--limit", "1",
                                                "--json", "databaseId,status,conclusion"], "gh run list")
    try:
        runs = json.loads(out or "[]")
    except ValueError:
        raise Fail("gh run list printed something that is not JSON: %r" % out[:200])
    return runs[0] if runs else None


def check(branch, wait_min, out_dir):
    reasons = []
    if os.environ.get("SHIP_NO_FETCH") != "1":
        git("fetch", "-q", "origin")
    sha = git("rev-parse", branch)
    try:
        osha = git("rev-parse", "origin/" + branch)
    except Fail:
        osha = ""
    print("1. pushed: %s" % ("yes (%s)" % sha[:7] if osha == sha
                             else "NO (local %s, origin %s)" % (sha[:7], osha[:7] or "none")))
    if osha != sha:
        reasons.append("not pushed")
    try:
        git("merge-base", "--is-ancestor", "origin/master", branch)
        print("2. fast-forward onto master: yes")
    except Fail:
        print("2. fast-forward onto master: NO (rebase onto origin/master first)")
        reasons.append("not a fast-forward of master")
    files = [f for f in git("diff", "--name-only", "origin/master..." + branch).splitlines() if f]
    pair, why = pair_for(files)
    print("3. %d file(s) changed; deploy pair: %s (%s)" % (len(files), pair, why))
    for f in files:
        print("     " + f)
    site_changed = any(f.startswith(SITE) for f in files)

    r = newest_run(sha)
    deadline = time.time() + wait_min * 60
    while r and r.get("status") != "completed" and time.time() < deadline:
        print("   CI run %s is %s; waiting..." % (r.get("databaseId"), r.get("status")), flush=True)
        time.sleep(POLL)
        r = newest_run(sha)
    if not r:
        if pair != "none":
            print("4. CI: NO platform run for %s" % sha[:7])
            reasons.append("no CI run for the head commit")
        else:
            print("4. CI: no platform run (its paths filter skips this change); nothing to deploy")
        print("5. phone VERDICT: none (no run)")
        if site_changed:
            reasons.append("no phone VERDICT")
    else:
        rid = str(r.get("databaseId"))
        print("4. CI run %s: %s/%s" % (rid, r.get("status"), r.get("conclusion")))
        if r.get("status") != "completed":
            reasons.append("CI still running after %g min" % wait_min)
        else:
            if r.get("conclusion") != "success":
                reasons.append("CI " + str(r.get("conclusion")))
            log = run(GH + ["run", "view", rid] + GH_REPO + ["--log"], "gh run view --log")
            verdicts = sorted({l[l.index("VERDICT "):].strip() for l in log.splitlines() if "VERDICT " in l})
            print("5. phone VERDICT: " + ("; ".join(verdicts) if verdicts else "none in the log"))
            if site_changed and (not verdicts or any("ALL-PASS" not in v for v in verdicts)):
                reasons.append("phone VERDICT not ALL-PASS")
            os.makedirs(out_dir, exist_ok=True)
            try:
                run(GH + ["run", "download", rid] + GH_REPO + ["-n", "phone-screenshots", "-D", out_dir],
                    "gh run download")
                pngs = sorted(os.path.join(dp, f) for dp, _, fs in os.walk(out_dir) for f in fs if f.endswith(".png"))
            except Fail as e:
                print("6. screenshots: NONE (%s)" % e)
                pngs = None
            if pngs is not None:
                print("6. screenshots (%d) in %s" % (len(pngs), out_dir))
                for p in pngs:
                    print("     " + p)
            if site_changed and not pngs:
                reasons.append("no phone screenshots")
    print()
    if reasons:
        print("SHIP_NOT_READY %s: %s" % (branch, "; ".join(reasons)))
        return 1
    print("Next, on Brandon's word:")
    print("  merge:  git checkout master && git merge --ff-only %s && git push origin master" % branch)
    if pair != "none":
        names = " ".join(f[len(PLATFORM):] for f in files if f.startswith(PLATFORM))
        print("  deploy: python deploy/make-go.py auto <name> \"<purpose>\" %s" % names)
        print("          then deploy/<name>-go.sh dry, show Brandon, and only on his go: deploy/<name>-go.sh go")
    print("SHIP_READY %s" % branch)
    return 0


def merge(branch, wait_min, out_dir):
    """Only on Brandon's "merge": the full check first; then origin/master moves to exactly the branch head by a plain
    push (git refuses anything but a fast-forward; never --force), and the result is read back."""
    if check(branch, wait_min, out_dir) != 0:
        return 1
    sha = git("rev-parse", branch)
    run(["git", "push", "origin", "%s:refs/heads/master" % sha], "git push origin master")
    git("fetch", "-q", "origin")
    now = git("rev-parse", "origin/master")
    if now != sha:
        raise Fail("after the push origin/master is %s, not %s" % (now[:7], sha[:7]))
    print("origin/master is now %s (= %s). Local master in the main checkout: git pull --ff-only" % (sha[:7], branch))
    print("SHIP_MERGED %s %s" % (branch, sha[:7]))
    return 0


def main(argv):
    if len(argv) < 2 or argv[0] not in ("check", "merge"):
        print(__doc__)
        return 2
    branch, wait_min = argv[1], 20.0
    out_dir = os.path.join(os.path.expanduser("~"), "scratch",
                           datetime.date.today().isoformat() + "-claude-ship", branch.replace("/", "-"))
    rest = argv[2:]
    try:
        while rest:
            flag, val = rest[0], rest[1]
            if flag == "--wait":
                wait_min = float(val)
            elif flag == "--out":
                out_dir = val
            else:
                raise IndexError(flag)
            rest = rest[2:]
    except (IndexError, ValueError):
        print(__doc__)
        return 2
    try:
        return (merge if argv[0] == "merge" else check)(branch, wait_min, out_dir)
    except Fail as e:
        print("SHIP_NOT_READY %s: %s" % (branch, e))
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
