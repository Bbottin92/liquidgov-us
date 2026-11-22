"""ship: one read-only report that says whether a branch is ready to merge and deploy, and the exact next commands.
Brandon 2026-11-11 ("yes fix the guard, then do ship"): a one-file change took ~15 hand-run steps, each a place to
slip. ship runs the checks; Brandon's one word (merge / go) still does the changing.
Usage: python scripts/ship.py check <branch> [--wait MIN] [--out DIR]
       python scripts/ship.py merge <branch> [--wait MIN] [--out DIR]   (only on Brandon's "merge": runs check,
         fast-forwards this checkout to the checked commit, then moves origin/master to exactly it by fast-forward
         push, never --force; last line SHIP_MERGED)
       python scripts/ship.py deploy <name> "<purpose>" [--override-file F]   python scripts/ship.py go <name> [--override-file F]
         F: `field: value` lines variable / deploy / said / reason / undo, for an override Brandon OK'd in chat (his
         whole message in `said`); only LIQUIDGOV_NO_ROLLBACK_OK so far
  1. pushed: <branch> == origin/<branch>             2. fast-forward: origin/master is an ancestor of <branch>
  3. files changed vs origin/master, and the deploy pair make-go.py auto would pick (or "no deploy needed")
  4. CI: the newest platform.yml run for exactly the branch head commit; waits up to --wait minutes (default 20);
     no run at all for a change that would deploy stops it (LIQUIDGOV_CI_GATE_MODE=warn: only a WARNING)
  5. phone VERDICT lines from that run's log; ALL-PASS required when site files (src/, assets/) changed
  6. the phone-screenshots artifact downloaded to --out (default ~/scratch/<date>-claude-ship/<branch>), PNGs listed
Last line: SHIP_READY <branch> or SHIP_NOT_READY <branch>: <reasons>; exit 0 when ready, 1 when not, 2 on bad usage.
Nothing here merges, pushes or deploys. A gh or git error counts as not ready (fails closed).
Test seams: SHIP_GH (JSON list, the gh command), SHIP_REPO (repo root), SHIP_NO_FETCH=1, SHIP_POLL_SECONDS.
Tests: tests/repo/ship_check.py"""
import datetime
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

REPO = os.environ.get("SHIP_REPO") or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GH = json.loads(os.environ["SHIP_GH"]) if os.environ.get("SHIP_GH") else ["gh"]
GH_REPO = ["--repo", "Bbottin92/liquidgov-us"]
POLL = float(os.environ.get("SHIP_POLL_SECONDS") or 30)
RUN_GRACE_LOOKS = 4  # extra looks, POLL apart, for a platform run GitHub has not listed yet (about two minutes)
PLATFORM = "liquidgov-platform/"
PLATFORM_NAME = "platform"  # the `name:` of .github/workflows/platform.yml (read via gh, 2026-11-11)
NO_RESTART = (PLATFORM + "assets/", PLATFORM + "tests/")
SITE = (PLATFORM + "src/", PLATFORM + "assets/")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}


class Fail(Exception):
    pass


VERIFIED = {}  # branch -> the commit sha check() verified


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
    VERIFIED[branch] = sha  # the one commit every check below is about; merge pushes exactly this (review B1)
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

    # A local CI record (scripts/local_ci.py, Brandon 2026-11-15: "can we skip github if it costs money now?"): when the
    # branch head carries deploy/ci-pass/<its liquidgov-platform tree>.txt saying PASS, that run of every workflow
    # stands in for GitHub's; its VERDICT lines are the phone verdicts. Without one, GitHub is asked as before.
    rec = local_record(branch)
    if rec is not None:
        print("4. CI: local record deploy/ci-pass/%s.txt (%s)" % (rec["tree"], rec["when"] or "no time"))
        print("5. phone VERDICT: " + ("; ".join(rec["verdicts"]) if rec["verdicts"] else "none in the record"))
        if site_changed and (not rec["verdicts"] or any("ALL-PASS" not in v for v in rec["verdicts"])):
            reasons.append("phone VERDICT not ALL-PASS")
        print("6. screenshots: on the machine that ran local_ci.py (not uploaded)")
        print("7. other workflows: in the same local record")
        return finish(branch, reasons, pair, files)

    r = newest_run(sha)
    deadline = time.time() + wait_min * 60
    # GitHub lists a run a few seconds after the push. A check started right after `git push` saw none and, for a
    # change with no site files, called the branch ready with no verdicts (three times on 2026-11-14). Look again a
    # few times before believing there is none (ship_recheck_check.py).
    looks = 0
    while not r and looks < RUN_GRACE_LOOKS and time.time() < deadline:
        print("   no platform run for %s yet; looking again..." % sha[:7], flush=True)
        time.sleep(POLL)
        r = newest_run(sha)
        looks += 1
    while r and r.get("status") != "completed" and time.time() < deadline:
        print("   CI run %s is %s; waiting..." % (r.get("databaseId"), r.get("status")), flush=True)
        time.sleep(POLL)
        r = newest_run(sha)
    if not r:
        if pair != "none":
            print("4. CI: NO platform run for %s" % sha[:7])
            # Block by default (2026-11-20, as deploy/ci-gate.sh since wip/159): a missing result stops a merge like a
            # failed one. LIQUIDGOV_CI_GATE_MODE=warn brings back the 2026-11-16 warn-only behaviour.
            if os.environ.get("LIQUIDGOV_CI_GATE_MODE", "block") == "warn":
                WARNINGS.append("no CI run for the head commit")
            else:
                reasons.append("no CI run for the head commit (LIQUIDGOV_CI_GATE_MODE=warn only warns)")
        else:
            print("4. CI: no platform run (its paths filter skips this change); nothing to deploy")
        print("5. phone VERDICT: none (no run)")
        if site_changed:
            WARNINGS.append("no phone VERDICT")
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
            # gh will not write over a file that is already there, so a second check of the same branch (ship.py merge
            # runs one) reported "screenshots: NONE ... The file exists" and, for a site change, refused
            # (2026-11-14). Download beside the folder, then move each file in over the old one.
            fresh = tempfile.mkdtemp(prefix=".ship-shots-", dir=os.path.dirname(os.path.abspath(out_dir)))
            try:
                run(GH + ["run", "download", rid] + GH_REPO + ["-n", "phone-screenshots", "-D", fresh],
                    "gh run download")
                pngs = []
                for dp, _, fs in os.walk(fresh):
                    for f in fs:
                        dst = os.path.join(out_dir, os.path.relpath(os.path.join(dp, f), fresh))
                        os.makedirs(os.path.dirname(dst), exist_ok=True)
                        os.replace(os.path.join(dp, f), dst)
                        if f.endswith(".png"):
                            pngs.append(dst)
                pngs.sort()
            except Fail as e:
                print("6. screenshots: NONE (%s)" % e)
                pngs = None
            finally:
                shutil.rmtree(fresh, ignore_errors=True)
            if pngs is not None:
                print("6. screenshots (%d) in %s" % (len(pngs), out_dir))
                for p in pngs:
                    print("     " + p)
            if site_changed and not pngs:
                reasons.append("no phone screenshots")
    # Every other workflow on this commit counts too (2026-11-11: a red commons run was invisible to ship).
    # tests/repo/ship_workflows_check.py
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
    return finish(branch, reasons, pair, files)


def local_record(branch):
    """The branch head's local CI pass record for its own liquidgov-platform tree, or None."""
    try:
        tree = git("rev-parse", branch + ":liquidgov-platform")
        text = git("show", "%s:deploy/ci-pass/%s.txt" % (branch, tree))
    except Fail:
        return None
    lines = text.splitlines()
    if not lines or lines[0].strip() != "LIQUIDGOV_LOCAL_CI PASS":
        return None
    when = next((l[5:].strip() for l in lines if l.startswith("when ")), "")
    return {"tree": tree, "when": when, "verdicts": [l.strip() for l in lines if l.startswith("VERDICT ")]}


WARNINGS = []   # things worth a loud line that do not stop a merge (no CI result only with LIQUIDGOV_CI_GATE_MODE=warn)


def finish(branch, reasons, pair, files):
    print()
    for w in WARNINGS:
        print("WARNING: %s (not blocking)" % w)
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


def push(sha, log):
    """git push of sha to master with NO time limit, its output (the pre-push hook's checks) written to log line by line
    as it runs. run()'s 300 s cap killed the push of PR #31 mid-checks (2026-11-21, q310); never --no-verify.
    tests/repo/ship_check.py (slow pre-push hook)"""
    os.makedirs(os.path.dirname(log) or ".", exist_ok=True)
    lines = []
    try:
        with open(log, "w", encoding="utf-8") as f:
            p = subprocess.Popen(["git", "push", "origin", "%s:refs/heads/master" % sha], cwd=REPO, env=ENV,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                                 errors="replace")
            for line in p.stdout:
                f.write(line)
                f.flush()
                lines.append(line)
            rc = p.wait()
    except OSError as e:
        raise Fail("git push origin master could not run: %s" % e)
    if rc != 0:
        raise Fail("git push origin master failed (exit %d): %s" % (rc, why_refused("".join(lines), log)))


def merge(branch, wait_min, out_dir):
    """Only on Brandon's "merge": the full check first; then origin/master moves to exactly the branch head by a plain
    push (git refuses anything but a fast-forward; never --force), and the result is read back."""
    if check(branch, wait_min, out_dir) != 0:
        return 1
    sha = VERIFIED[branch]
    if os.environ.get("SHIP_NO_FETCH") != "1":
        git("fetch", "-q", "origin")
    now_local = git("rev-parse", branch)
    try:
        now_origin = git("rev-parse", "origin/" + branch)
    except Fail:
        now_origin = ""
    if now_local != sha or now_origin != sha:
        # a commit made during the CI wait was never checked (review B1, 2026-11-11): never push it
        raise Fail("the branch moved during the check (checked %s, now local %s / origin %s); run check again"
                   % (sha[:7], now_local[:7], now_origin[:7] or "none"))
    # The pre-push hook checks the files of the checkout the push runs from, not the commit being pushed. Run from the
    # main checkout on old master, it judged a test already fixed on the branch and refused (2026-11-16, and again
    # 2026-11-21 for nov20; lesson merging-or-pushing.md:48). So that checkout first moves to exactly the checked commit.
    # tests/repo/ship_fixes_check.py (N1)
    if git("rev-parse", "HEAD") != sha:
        here = git("rev-parse", "--abbrev-ref", "HEAD")
        if here != "master" or git("status", "--porcelain"):
            raise Fail("the checkout running ship is on %s%s, so its pre-push hook would check other files than %s; "
                       "run merge from a clean master checkout" % (here, " with changes" if here == "master" else "", sha[:7]))
        git("merge", "--ff-only", "-q", sha)
        print("   this checkout fast-forwarded to %s, so the pre-push hook checks the files being pushed" % sha[:7])
    push_log = os.path.join(out_dir, "push.log")
    print("   pushing; the pre-push checks write to %s as they run (no time limit)" % push_log)
    push(sha, push_log)
    git("fetch", "-q", "origin")
    now = git("rev-parse", "origin/master")
    if now != sha:
        raise Fail("after the push origin/master is %s, not %s" % (now[:7], sha[:7]))
    print("origin/master is now %s (= %s), and so is this checkout." % (sha[:7], branch))
    print("SHIP_MERGED %s %s" % (branch, sha[:7]))
    return 0


# ---- deploy / go (Brandon 2026-11-11 "yes build it"): the liquidgov-deploy order, steps 3-9, driven by the scripts
# that already hold the guards (sync-to-droplet.sh dry run, make-go.py auto, <name>-go.sh with the CI gate, backup,
# rollback guard and public checks). ship adds no server logic of its own.
MAIN = os.environ.get("SHIP_MAIN") or ""
SH = json.loads(os.environ["SHIP_SH"]) if os.environ.get("SHIP_SH") else ["wsl.exe", "bash", "-lc"]
# Claude's own key (CLAUDE.md: the server logs must name who connected). These go INSIDE the command text: wsl.exe does
# not pass Windows environment variables through (WSLENV is empty here), so the first real dry run used the shared
# deploy key (Grok Bot's review B3, 2026-11-11). tests/repo/ship_fixes_check.py
DEPLOY_KEY = "C:/Users/Brandon/.ssh/id_ed25519_claude"
DEPLOY_SSH = "/mnt/c/Windows/System32/OpenSSH/ssh.exe -i %s -o IdentitiesOnly=yes" % DEPLOY_KEY
# `export ...;` (not a `NAME=value cmd` prefix): it covers every command in the line, including shell builtins; a
# prefix reaches only the first program, and a real-WSL probe with `echo $NAME` showed it empty (2026-11-11).
KEY_PREFIX = "export LIQUIDGOV_DEPLOY_KEY=%s LIQUIDGOV_DEPLOY_SSH=%s; " % (__import__("shlex").quote(DEPLOY_KEY),
                                                                         __import__("shlex").quote(DEPLOY_SSH))
# Emergency switches the deploy scripts honour only with Brandon's explicit OK for that deploy (review M3): never
# inherited from whatever shell ran ship.
OVERRIDES = ("LIQUIDGOV_CI_OVERRIDE", "LIQUIDGOV_DEPLOY_ALLOW_DIRTY", "LIQUIDGOV_NO_ROLLBACK_OK",
             "LIQUIDGOV_LIVE_COMMIT")
# The switches an --override-file may set (q274, Brandon 2026-11-21: "plain yes in chat is enough", judged on what his
# whole message means). nov20 (2026-11-20) needed LIQUIDGOV_NO_ROLLBACK_OK and ship refused it, so every step after the
# guard was run by hand. The CI, dirty-tree and live-commit switches stay hand-run: they skip a check instead of
# accepting a named risk with an undo plan. tests/repo/ship_override_check.py
OVERRIDE_FILE_OK = ("LIQUIDGOV_NO_ROLLBACK_OK",)
OVERRIDE_FIELDS = ("variable", "deploy", "said", "reason", "undo")
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


def read_override(path, name):
    """An --override-file: `field: value` lines (variable, deploy, said, reason, undo; '#' starts a comment). Returns
    (variable, value, summary); refuses a missing field, a variable not in OVERRIDE_FILE_OK, or another deploy's file.
    `said` is Brandon's whole message, quoted, so the log shows what the OK was."""
    try:
        text = open(path, encoding="utf-8-sig").read()
    except OSError as e:
        raise Fail("the override file %s cannot be read: %s" % (path, e))
    fields = {}
    for line in text.splitlines():
        if line.strip().startswith("#") or ":" not in line:
            continue
        key, val = line.split(":", 1)
        fields[key.strip().lower()] = val.strip()
    missing = [k for k in OVERRIDE_FIELDS if not fields.get(k)]
    if missing:
        raise Fail("the override file %s has no %s (each is one `field: value` line)" % (path, ", ".join(missing)))
    var = fields["variable"]
    if var not in OVERRIDE_FILE_OK:
        raise Fail("the override file %s sets %s; an override file may set only %s (the others stay hand-run)"
                   % (path, var, ", ".join(OVERRIDE_FILE_OK)))
    if fields["deploy"] != name:
        raise Fail("the override file %s is for deploy %r, not %r (an OK covers one deploy)" % (path, fields["deploy"], name))
    value = "Brandon said: %s | reason: %s | undo: %s" % (fields["said"], fields["reason"], fields["undo"])
    return var, value, "OVERRIDE %s for %s (from %s): %s" % (var, name, path, value)


def override_prefix(override):
    """The `export VAR=...;` text for an override, inside the command like KEY_PREFIX (wsl.exe passes no env)."""
    return "" if not override else "export %s=%s; " % (override[0], __import__("shlex").quote(override[1]))


def why_refused(text, log):
    """Every guard/refusal line of a failed run with the indented list under it, then the log path. The old message was
    the run's last 300 characters, which cut the rollback guard's record list off (nov20: the fmix line; lesson
    deploying-site.md:86)."""
    lines = text.splitlines()
    keep = []
    for i, line in enumerate(lines):
        if any(k in line for k in ("ROLLBACK_GUARD", "REFUS", "Refusing", "FAIL")):
            keep.append(line)
            j = i + 1
            while j < len(lines) and lines[j].startswith("  "):
                keep.append(lines[j])
                j += 1
    body = "\n".join(keep) if keep else text.strip()[-300:]
    return "%s\n(full log: %s)" % (body, log)


def shell(cwd, command, log, override=None):
    """Run one bash command in cwd (WSL for real), with Claude's key in the command text. Output is written to log
    line by line AS IT RUNS and there is no time limit (review B2: a timeout could kill a deploy mid-way and the log
    was only written at the end). Returns (rc, text); rc < 0 or >= 128 means the shell was killed. An override from
    read_override() goes into the command text too; override variables from the caller's own env never do."""
    env = {k: v for k, v in ENV.items() if k not in OVERRIDES}
    lines = []
    try:
        with open(log, "w", encoding="utf-8") as f:
            if override:
                f.write(override[2] + "\n")
            p = subprocess.Popen(SH + [KEY_PREFIX + override_prefix(override) + command], cwd=cwd, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", env=env)
            for line in p.stdout:
                f.write(line)
                f.flush()
                lines.append(line)
            rc = p.wait()
    except OSError as e:
        raise Fail("could not run %r: %s" % (command, e))
    return rc, "".join(lines)


def no_overrides():
    found = [k for k in OVERRIDES if os.environ.get(k)]
    if found:
        raise Fail("override variable(s) set in this shell: %s. ship never passes them on; an override needs "
                   "Brandon's explicit OK for that one deploy, run by hand" % ", ".join(found))


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


def deploy(name, purpose, override_file=None):
    """Only after Brandon's "merge" landed: dry-run file list -> make-go auto -> pair committed + pushed -> go.sh dry.
    Changes nothing on the server. Last line SHIP_DRY_DONE <name>; then show Brandon and wait for his go.
    --override-file: see read_override(); its variable reaches the sync dry run and the go script's dry run."""
    no_overrides()
    override = read_override(override_file, name) if override_file else None
    if override:
        print(override[2])
    main = main_checkout()
    master_ready(main)
    go = "deploy/%s-go.sh" % name
    # The pair never goes on master here (review H2): it gets its own wip branch + worktree, pushed, and reaches
    # master only through `ship.py merge` on Brandon's "merge". A rerun reuses that branch.
    mine = [b.strip().lstrip("*+ ").strip() for b in gitm(main, "branch", "--list", "wip/*-deploy-" + name).splitlines()]
    mine = [b for b in mine if __import__("re").fullmatch(r"wip/\d+-deploy-" + name, b)]
    wt = os.path.join(os.path.dirname(main), os.path.basename(main) + "-deploy-" + name)
    if mine:
        branch = mine[0]
        if not os.path.isdir(wt):
            gitm(main, "worktree", "add", "-q", wt, branch)
    else:
        nums = [int(n) for n in __import__("re").findall(r"wip/(\d+)-", gitm(main, "branch", "-a"))]
        branch = "wip/%d-deploy-%s" % (max(nums + [0]) + 1, name)
        gitm(main, "worktree", "add", "-q", "-b", branch, wt, "master")
    print("0. deploy branch %s in %s" % (branch, wt))
    if os.path.exists(os.path.join(wt, go)):
        print("1-3. pair %s already committed on %s: reusing it (no make-go, no new commit)" % (go, branch))
    else:
        sync_log = log_path(name, "sync-dry")
        rc, text = shell(wt, "LIQUIDGOV_DEPLOY_DRY_RUN=1 bash deploy/sync-to-droplet.sh", sync_log, override)
        if rc != 0:
            raise Fail("the sync dry run failed (exit %d):\n%s" % (rc, why_refused(text, sync_log)))
        files = [m.group(1).strip() for m in map(ITEM.match, text.splitlines()) if m]
        if not files:
            raise Fail("nothing differs from live (sync dry run listed no files)")
        print("1. sync dry run: %d file(s) differ from live:" % len(files))
        for f in files:
            print("     " + f)
        out = run([sys.executable, os.path.join(wt, "deploy", "make-go.py"), "auto", name, purpose] + files,
                  "make-go.py")
        print("2. make-go: " + "; ".join(l for l in out.splitlines() if l.startswith("auto:")))
        pair = [go, "deploy/%s-remote.sh" % name] + (["naming.toml"] if gitm(wt, "status", "--porcelain",
                                                                              "naming.toml") else [])
        gitm(wt, "add", *pair)
        gitm(wt, "commit", "-q", "-m", "deploy: %s pair (%s)\n\nRead: generated by deploy/make-go.py auto from the "
             "sync dry run's file list (%s).\nAttacker: n/a - a deploy script pair; nothing runs until Brandon's go.\n"
             "Undo: revert this commit; nothing on the server changed." % (name, purpose, ", ".join(files)))
        gitm(wt, "push", "-q", "origin", branch)
        gitm(wt, "fetch", "-q", "origin")
        if gitm(wt, "rev-parse", branch) != gitm(wt, "rev-parse", "origin/" + branch):
            raise Fail("SHIP_PAIR_PUSHED_UNVERIFIED: pushed %s but origin/%s does not match" % (branch, branch))
        print("3. pair committed on %s and pushed: %s" % (branch, gitm(wt, "rev-parse", "--short", branch)))
    log = log_path(name, "dry")
    rc, text = shell(wt, "bash %s dry" % go, log, override)
    shown = [l for l in text.splitlines() if any(k in l for k in ("CI_GATE", "DRY_OK", "DRY_ONLY_DONE", "REFUS",
                                                                  "FAIL", "WARNING"))]
    print("4. %s dry (exit %d; full log %s):" % (go, rc, log))
    for l in shown:
        print("     " + l)
    if rc != 0 or "DRY_ONLY_DONE" not in text:
        raise Fail("the dry run did not finish (exit %d):\n%s" % (rc, why_refused(text, log)))
    if "CI_GATE_OK" not in text:
        raise Fail("the CI gate would refuse this deploy (see the dry-run lines above)")
    print("\nShow Brandon the dry run and the screenshots from `ship.py check`. On his \"merge\": "
          "python scripts/ship.py merge %s ; then only on his go: python scripts/ship.py go %s%s"
          % (branch, name, (" --override-file " + override_file) if override else ""))
    print("SHIP_DRY_DONE %s" % name)
    return 0


def go_live(name, override_file=None):
    """Only on Brandon's go for THIS change: <name>-go.sh go (CI gate, backup, sync, chown, build/restart, public
    checks, RECORD lines). SHIP_LIVE <name> only with exit 0, CHECK_DONE and both RECORD lines.
    --override-file: see read_override(); checked again here, so a file for another deploy is refused."""
    no_overrides()
    override = read_override(override_file, name) if override_file else None
    if override:
        print(override[2])
    main = main_checkout()
    go = "deploy/%s-go.sh" % name
    master_ready(main)
    if not os.path.exists(os.path.join(main, go)):
        print("SHIP_GO_FAILED %s: no %s on master (ship.py deploy, then ship.py merge its branch on Brandon's word)"
              % (name, go))
        return 1
    log = log_path(name, "go")
    rc, text = shell(main, "bash %s go" % go, log, override)
    lines = text.splitlines()
    for l in lines[-15:]:
        print("     " + l)
    if rc < 0 or rc >= 128:  # killed (signal), not a script decision: the server may be half-changed (review B2)
        print("SHIP_GO_UNKNOWN %s: the go script was killed (exit %d) part-way. Do NOT rerun go blindly: read %s "
              "for the last step reached, run `bash %s dry` for the server's state (preflight: PID, ACTIVE), and use "
              "the script's own rollback notes before anything else." % (name, rc, log, go))
        return 3
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
        args, override_file = list(argv), None
        if "--override-file" in args:
            i = args.index("--override-file")
            if i + 1 >= len(args):
                print(__doc__)
                return 2
            override_file = args[i + 1]
            del args[i:i + 2]
        if not re.fullmatch(r"[a-z][a-z0-9]*", args[1]) or len(args) != (3 if args[0] == "deploy" else 2):
            print(__doc__)
            return 2
        try:
            return (deploy(args[1], args[2], override_file) if args[0] == "deploy"
                    else go_live(args[1], override_file))
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
