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
PLATFORM_NAME = "platform"  # the `name:` of .github/workflows/platform.yml (read via gh, 2026-11-11)
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
    # Every other workflow on this commit counts too (2026-11-11: a red commons run was invisible to ship).
    # commons-daemon/tests/ship_workflows_check.py
    out = run(GH + ["run", "list"] + GH_REPO + ["--commit", sha, "--limit", "50",
                                                "--json", "workflowName,status,conclusion"], "gh run list (all)")
    try:
        others = [o for o in json.loads(out or "[]") if o.get("workflowName") not in (None, "", PLATFORM_NAME)]
    except ValueError:
        raise Fail("gh run list (all) printed something that is not JSON: %r" % out[:200])
    for o in others:
        name = o["workflowName"]
        if o.get("status") != "completed":
            reasons.append("%s still running" % name)
        elif o.get("conclusion") not in ("success", "skipped", "neutral"):
            reasons.append("%s %s" % (name, o.get("conclusion")))
    print("7. other workflows on this commit: " + (", ".join("%s %s" % (o["workflowName"], o.get("conclusion")
                                                                        or o.get("status")) for o in others) or "none"))
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


# ---- deploy / go (Brandon 2026-11-11 "yes build it"): the liquidgov-deploy order, steps 3-9, driven by the scripts
# that already hold the guards (sync-to-droplet.sh dry run, make-go.py auto, <name>-go.sh with the CI gate, backup,
# rollback guard and public checks). ship adds no server logic of its own.
MAIN = os.environ.get("SHIP_MAIN") or ""
SH = json.loads(os.environ["SHIP_SH"]) if os.environ.get("SHIP_SH") else ["wsl.exe", "bash", "-lc"]
DEPLOY_SSH = ("/mnt/c/Windows/System32/OpenSSH/ssh.exe -i C:/Users/Brandon/.ssh/id_ed25519_claude "
              "-o IdentitiesOnly=yes")
# rsync --itemize-changes: only '<', '>' or 'c' in column 1 move content; '.f....og...' is an owner/group-only
# difference (the real dry run 2026-11-11 listed 149 of those) and is not a file to deploy.
ITEM = __import__("re").compile(r"^[<>c]f\S{9} (\S.*)$")


def main_checkout():
    if MAIN:
        return MAIN
    common = run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], "git rev-parse").strip()
    return os.path.dirname(common)


def log_path(name, what):
    d = os.environ.get("SHIP_LOG_DIR") or os.path.join(os.path.expanduser("~"), "scratch",
                                                      datetime.date.today().isoformat() + "-claude-ship")
    os.makedirs(d, exist_ok=True)
    return os.path.join(d, "%s-%s.log" % (name, what))


def shell(main, command, log):
    """Run one bash command in the main checkout (WSL for real); the whole output goes to log; returns (rc, text)."""
    env = dict(ENV, LIQUIDGOV_DEPLOY_SSH=DEPLOY_SSH)
    try:
        p = subprocess.run(SH + [command], cwd=main, capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=3600, env=env)
    except (OSError, subprocess.SubprocessError) as e:
        raise Fail("could not run %r: %s" % (command, e))
    text = (p.stdout or "") + (p.stderr or "")
    with open(log, "w", encoding="utf-8") as f:
        f.write(text)
    return p.returncode, text


def gitm(main, *args):
    return run(["git", "-C", main] + list(args), "git " + args[0]).strip()


def master_ready(main):
    if gitm(main, "rev-parse", "--abbrev-ref", "HEAD") != "master":
        raise Fail("the main checkout %s is not on master" % main)
    if gitm(main, "status", "--porcelain"):
        raise Fail("the main checkout is not clean (only committed code goes live)")
    gitm(main, "fetch", "-q", "origin")
    if gitm(main, "rev-parse", "master") != gitm(main, "rev-parse", "origin/master"):
        raise Fail("local master != origin/master (pull or merge first; GitHub and live must agree)")


def deploy(name, purpose):
    """Only after Brandon's "merge" landed: dry-run file list -> make-go auto -> pair committed + pushed -> go.sh dry.
    Changes nothing on the server. Last line SHIP_DRY_DONE <name>; then show Brandon and wait for his go."""
    main = main_checkout()
    master_ready(main)
    go = "deploy/%s-go.sh" % name
    if os.path.exists(os.path.join(main, go)):
        print("1-3. pair %s already committed: reusing it (no make-go, no new commit)" % go)
    else:
        rc, text = shell(main, "LIQUIDGOV_DEPLOY_DRY_RUN=1 bash deploy/sync-to-droplet.sh", log_path(name, "sync-dry"))
        if rc != 0:
            raise Fail("the sync dry run failed (exit %d): %s" % (rc, text.strip()[-300:]))
        files = [m.group(1).strip() for m in map(ITEM.match, text.splitlines()) if m]
        if not files:
            raise Fail("nothing differs from live (sync dry run listed no files)")
        print("1. sync dry run: %d file(s) differ from live:" % len(files))
        for f in files:
            print("     " + f)
        out = run([sys.executable, os.path.join(main, "deploy", "make-go.py"), "auto", name, purpose] + files,
                  "make-go.py")
        print("2. make-go: " + "; ".join(l for l in out.splitlines() if l.startswith("auto:")))
        pair = [go, "deploy/%s-remote.sh" % name] + (["naming.toml"] if gitm(main, "status", "--porcelain",
                                                                              "naming.toml") else [])
        gitm(main, "add", *pair)
        gitm(main, "commit", "-q", "-m", "deploy: %s pair (%s)\n\nRead: generated by deploy/make-go.py auto from the "
             "sync dry run's file list (%s).\nAttacker: n/a - a deploy script pair; nothing runs until Brandon's go.\n"
             "Undo: revert this commit; nothing on the server changed." % (name, purpose, ", ".join(files)))
        gitm(main, "push", "-q", "origin", "master")
        gitm(main, "fetch", "-q", "origin")
        if gitm(main, "rev-parse", "master") != gitm(main, "rev-parse", "origin/master"):
            raise Fail("pushed the pair but origin/master does not match local master")
        print("3. pair committed and pushed: %s" % gitm(main, "rev-parse", "--short", "master"))
    log = log_path(name, "dry")
    rc, text = shell(main, "bash %s dry" % go, log)
    shown = [l for l in text.splitlines() if any(k in l for k in ("CI_GATE", "DRY_OK", "DRY_ONLY_DONE", "REFUS",
                                                                  "FAIL", "WARNING"))]
    print("4. %s dry (exit %d; full log %s):" % (go, rc, log))
    for l in shown:
        print("     " + l)
    if rc != 0 or "DRY_ONLY_DONE" not in text:
        raise Fail("the dry run did not finish (exit %d)" % rc)
    if "CI_GATE_OK" not in text:
        raise Fail("the CI gate would refuse this deploy (see the dry-run lines above)")
    print("\nShow Brandon the dry run and the screenshots from `ship.py check`; only on his go: "
          "python scripts/ship.py go %s" % name)
    print("SHIP_DRY_DONE %s" % name)
    return 0


def go_live(name):
    """Only on Brandon's go for THIS change: <name>-go.sh go (CI gate, backup, sync, chown, build/restart, public
    checks, RECORD lines). SHIP_LIVE <name> only with exit 0, CHECK_DONE and both RECORD lines."""
    main = main_checkout()
    go = "deploy/%s-go.sh" % name
    if not os.path.exists(os.path.join(main, go)):
        print("SHIP_GO_FAILED %s: no %s (run ship.py deploy first)" % (name, go))
        return 1
    master_ready(main)
    log = log_path(name, "go")
    rc, text = shell(main, "bash %s go" % go, log)
    lines = text.splitlines()
    for l in lines[-15:]:
        print("     " + l)
    records = [l for l in lines if l.startswith("RECORD ctx log:") or l.startswith("RECORD notes.md:")]
    if rc != 0 or "CHECK_DONE" not in text or "RECORD_FAILED" in text or len(records) < 2:
        why = ("exit %d" % rc if rc != 0 else "no CHECK_DONE" if "CHECK_DONE" not in text
               else "RECORD_FAILED (write the records by hand from the log)")
        print("SHIP_GO_FAILED %s: %s; full log %s" % (name, why, log))
        return 1
    print("Records from this run (use these, never from memory):")
    for r in records:
        print("  " + r)
    print("SHIP_LIVE %s" % name)
    return 0


def main(argv):
    import re
    if len(argv) >= 2 and argv[0] in ("deploy", "go"):
        if not re.fullmatch(r"[a-z][a-z0-9]*", argv[1]) or (argv[0] == "deploy" and len(argv) != 3):
            print(__doc__)
            return 2
        try:
            return deploy(argv[1], argv[2]) if argv[0] == "deploy" else go_live(argv[1])
        except Fail as e:
            print(("SHIP_NOT_READY %s: %s" if argv[0] == "deploy" else "SHIP_GO_FAILED %s: %s") % (argv[1], e))
            return 1
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
