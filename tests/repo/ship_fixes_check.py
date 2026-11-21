"""ship_fixes_check.py: the blockers Grok Bot's review found in scripts/ship.py (2026-11-11; Brandon "yes fix the ship
blockers first"; scratch/2026-11-11-grokbot-ship-review/REVIEW.md).
  B1 merge pushes exactly the commit check verified; a branch that moved during the CI wait is refused.
  B2 go streams its output to the log as it runs (no time limit); an abnormal end (killed) is SHIP_GO_UNKNOWN, exit 3,
     and the log still holds what ran.
  B3 the shell is a fake WSL that starts from an EMPTY environment (as wsl.exe does here: WSLENV is empty); Claude's own
     key must still reach the sync dry run and the go script, because ship puts it in the command text.
  H2 deploy never commits on master: the pair goes on its own wip branch in its own worktree, pushed; master and the
     main checkout are untouched; go refuses until that branch is merged.
  M3 override variables (LIQUIDGOV_CI_OVERRIDE, ..._ALLOW_DIRTY, ..._NO_ROLLBACK_OK, ..._LIVE_COMMIT) in the caller's
     environment are refused, not passed along (an OK'd one goes through --override-file: ship_override_check.py).
  N1 merge fast-forwards the checkout it runs from to the checked commit before pushing, so the pre-push hook checks
     the files being pushed (lesson merging-or-pushing.md:48); a dirty checkout is refused.
Usage: python tests/repo/ship_fixes_check.py [repo root] -> last line passes=N fails=M; exit 1 on any FAIL."""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SHIP = os.path.join(ROOT, "scripts", "ship.py")
BASH = shutil.which("bash") or "bash"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and not k.startswith("LIQUIDGOV_")}
CLAUDE_KEY = "id_ed25519_claude"
passes = fails = 0

FAKE_WSL = '#!/usr/bin/env bash\nexec env -i PATH="$PATH" HOME="$HOME" FAKE_DIR="$FAKE_DIR" bash -c "$1"\n'
FAKE_SYNC = r'''#!/usr/bin/env bash
echo "SYNC_SSH=${LIQUIDGOV_DEPLOY_SSH:-unset}" >> "$FAKE_DIR/seen.txt"
echo "Deploying commit abc1234"
echo ">f.st...... assets/a.css"
echo "Sync done."
'''
FAKE_MAKE_GO = r'''import os, sys
name = sys.argv[2]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(root, "deploy", name + "-go.sh"), "w", newline="\n").write("""#!/usr/bin/env bash
echo "GO_KEY=${LIQUIDGOV_DEPLOY_KEY:-unset}" >> "$FAKE_DIR/seen.txt"
mode="$(cat "$FAKE_DIR/go_mode" 2>/dev/null || echo ok)"
if [ "$1" = dry ]; then echo "CI_GATE_OK tree"; echo DRY_OK; echo "DRY_ONLY_DONE: nothing changed."; exit 0; fi
echo STARTED
if [ "$mode" = killed ]; then kill -9 $$; fi
echo BACKUP_DONE; echo GO_DONE; echo CHECK_DONE
echo "RECORD ctx log: python ctx.py log <who> deploy x --result ok --ref abc1234"
echo "RECORD notes.md: - x LIVE (commit abc1234, PID 42, public checks FAILS 0)"
""")
open(os.path.join(root, "deploy", name + "-remote.sh"), "w", newline="\n").write("#!/usr/bin/env bash\n")
print("auto: jbshell pair (assets only)")
'''
# B1: on its first platform query the fake gh moves the branch (a commit made "during the wait"), then reports green
FAKE_GH = r'''
import json, os, subprocess, sys
st = os.environ["FAKE_GH_STATE"]
s = json.load(open(st, encoding="utf-8"))
a = sys.argv[1:]
def g(*x):
    subprocess.run(["git", "-C", os.environ["FAKE_WORK"]] + list(x), check=True, capture_output=True)
if a[:2] == ["run", "list"] and "--workflow" in a:
    if s.get("move_once"):
        s["move_once"] = False
        json.dump(s, open(st, "w", encoding="utf-8"))
        g("checkout", "-q", os.environ["FAKE_BRANCH"])
        open(os.path.join(os.environ["FAKE_WORK"], "liquidgov-platform", "assets", "late.css"), "w").write("x")
        g("add", "."); g("commit", "-q", "-m", "late"); g("push", "-q", "origin", os.environ["FAKE_BRANCH"])
        g("checkout", "-q", "master")
        print(json.dumps([{"databaseId": 7, "status": "in_progress", "conclusion": ""}]))
    else:
        print(json.dumps([{"databaseId": 7, "status": "completed", "conclusion": "success"}]))
elif a[:2] == ["run", "list"]:
    print("[]")
elif a[:2] == ["run", "view"]:
    print("x VERDICT ci: ALL-PASS  (3 checks)")
elif a[:2] == ["run", "download"]:
    d = a[a.index("-D") + 1]; os.makedirs(d, exist_ok=True); open(os.path.join(d, "p.png"), "wb").write(b"p")
else:
    sys.exit(3)
'''


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-700:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


tmp = tempfile.mkdtemp(prefix="ship-fixes-")
try:
    check("precondition: scripts/ship.py exists", os.path.isfile(SHIP), SHIP)
    origin, work = os.path.join(tmp, "origin.git"), os.path.join(tmp, "work")
    sh(tmp, "git", "init", "-q", "--bare", "-b", "master", origin)
    sh(tmp, "git", "init", "-q", "-b", "master", work)
    sh(work, "git", "config", "user.email", "t@t"); sh(work, "git", "config", "user.name", "t")
    os.makedirs(os.path.join(work, "deploy")); os.makedirs(os.path.join(work, "liquidgov-platform", "assets"))
    open(os.path.join(work, "deploy", "sync-to-droplet.sh"), "w", newline="\n").write(FAKE_SYNC)
    open(os.path.join(work, "deploy", "make-go.py"), "w", newline="\n").write(FAKE_MAKE_GO)
    open(os.path.join(work, "naming.toml"), "w").write("x = 1\n")
    open(os.path.join(work, "liquidgov-platform", "assets", "a.css"), "w").write("a\n")
    open(os.path.join(tmp, "fake_wsl.sh"), "w", newline="\n").write(FAKE_WSL)
    open(os.path.join(tmp, "fake_gh.py"), "w", encoding="utf-8").write(FAKE_GH)
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin); sh(work, "git", "push", "-q", "-u", "origin", "master")
    om = lambda: sh(origin, "git", "rev-parse", "master")
    seen = lambda: open(os.path.join(tmp, "seen.txt"), encoding="utf-8").read() if os.path.exists(
        os.path.join(tmp, "seen.txt")) else ""

    def ship(*args, extra=None, state=None, branch=""):
        sp = os.path.join(tmp, "gh_state.json")
        json.dump(state or {}, open(sp, "w", encoding="utf-8"))
        env = dict(ENV, SHIP_MAIN=work, SHIP_REPO=work, SHIP_SH=json.dumps([BASH, os.path.join(tmp, "fake_wsl.sh")]),
                   FAKE_DIR=tmp, SHIP_LOG_DIR=os.path.join(tmp, "logs"), SHIP_POLL_SECONDS="0", SHIP_NO_FETCH="1",
                   SHIP_GH=json.dumps([sys.executable, os.path.join(tmp, "fake_gh.py")]), FAKE_GH_STATE=sp,
                   FAKE_WORK=work, FAKE_BRANCH=branch)
        env.update(extra or {})
        p = subprocess.run([sys.executable, SHIP] + list(args), capture_output=True, text=True, env=env, timeout=300)
        return p.returncode, p.stdout + p.stderr

    # ---- M3
    rc, out = ship("deploy", "thing1", "a thing", extra={"LIQUIDGOV_CI_OVERRIDE": "because"})
    check("M3: deploy refuses an override variable in the caller's environment",
          rc == 1 and "LIQUIDGOV_CI_OVERRIDE" in out and not os.path.exists(os.path.join(tmp, "seen.txt")), out)

    # ---- H2 + B3 (deploy)
    before = om()
    rc, out = ship("deploy", "thing1", "a thing")
    check("deploy: SHIP_DRY_DONE", rc == 0 and "SHIP_DRY_DONE thing1" in out, out)
    check("H2: origin/master did not move", om() == before, out)
    check("H2: the main checkout is still a clean master",
          sh(work, "git", "rev-parse", "--abbrev-ref", "HEAD") == "master" and sh(work, "git", "status", "--porcelain") == "",
          sh(work, "git", "status", "--porcelain"))
    branches = sh(origin, "git", "branch", "--list", "wip/*-deploy-thing1")
    bname = branches.strip().lstrip("* ").strip()
    check("H2: the pair is on its own pushed branch wip/NN-deploy-thing1",
          bool(bname) and "deploy/thing1-go.sh" in sh(origin, "git", "show", "--name-only", "--format=", bname), branches)
    check("...and the next step printed is a merge of that branch", bname and ("merge " + bname) in out, out)
    s = seen()
    check("B3: the sync dry run got Claude's own key through an empty-env WSL", "SYNC_SSH=" in s and CLAUDE_KEY in
          s.split("SYNC_SSH=")[1].splitlines()[0], s)
    check("B3: the go script (dry) got LIQUIDGOV_DEPLOY_KEY = Claude's key", "GO_KEY=" in s and CLAUDE_KEY in
          s.split("GO_KEY=")[1].splitlines()[0], s)

    # ---- go before the merge
    rc, out = ship("go", "thing1")
    check("H2: go refuses while the pair is not on master", rc == 1 and "SHIP_GO_FAILED" in out, out)
    sh(work, "git", "fetch", "-q", "origin"); sh(work, "git", "merge", "-q", "--ff-only", "origin/" + bname)
    sh(work, "git", "push", "-q", "origin", "master")

    # ---- M3 (go)
    rc, out = ship("go", "thing1", extra={"LIQUIDGOV_NO_ROLLBACK_OK": "x"})
    check("M3: go refuses an override variable too", rc == 1 and "LIQUIDGOV_NO_ROLLBACK_OK" in out, out)

    # ---- B2
    open(os.path.join(tmp, "go_mode"), "w").write("killed")
    rc, out = ship("go", "thing1")
    log = os.path.join(tmp, "logs", "thing1-go.log")
    check("B2: a go that is killed mid-way is SHIP_GO_UNKNOWN, exit 3", rc == 3 and "SHIP_GO_UNKNOWN" in out, (rc, out))
    check("B2: the log holds what ran before it died",
          os.path.exists(log) and "STARTED" in open(log, encoding="utf-8").read(), log)
    open(os.path.join(tmp, "go_mode"), "w").write("ok")
    rc, out = ship("go", "thing1")
    check("go after the merge: SHIP_LIVE, with Claude's key", rc == 0 and "SHIP_LIVE thing1" in out
          and CLAUDE_KEY in seen().split("GO_KEY=")[-1].splitlines()[0], out)

    # ---- B1
    sh(work, "git", "checkout", "-q", "-b", "wip/9-change", "master")
    open(os.path.join(work, "liquidgov-platform", "assets", "b.css"), "w").write("b\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "change")
    sh(work, "git", "push", "-q", "origin", "wip/9-change"); sh(work, "git", "checkout", "-q", "master")
    sh(work, "git", "fetch", "-q", "origin")
    before = om()
    rc, out = ship("merge", "wip/9-change", "--wait", "5", "--out", os.path.join(tmp, "o"),
                   state={"move_once": True}, branch="wip/9-change")
    check("precondition: the branch moved during the check's wait",
          "late" in sh(work, "git", "log", "-1", "--format=%s", "wip/9-change"), out)
    check("B1: merge refuses a branch that moved during the check, and master stays put",
          rc == 1 and "moved" in out and om() == before, out)

    # ---- N1 (2026-11-16 and 2026-11-21): the pre-push hook checks the WORKING FILES of the checkout the push runs
    # from. Pushed from a main checkout still on old master, it judged the old files and refused. This hook refuses
    # unless the working tree holds the file the branch adds, the way the real one judged the stale test.
    sh(work, "git", "checkout", "-q", "-b", "wip/10-n1", "master")
    open(os.path.join(work, "liquidgov-platform", "assets", "n1.css"), "w").write("n1\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "n1")
    sh(work, "git", "push", "-q", "origin", "wip/10-n1"); sh(work, "git", "checkout", "-q", "master")
    sh(work, "git", "fetch", "-q", "origin")
    hook = os.path.join(work, ".git", "hooks", "pre-push")
    open(hook, "w", newline="\n").write("#!/bin/sh\n[ -f liquidgov-platform/assets/n1.css ] || "
                                        "{ echo 'pre-push: stale working tree'; exit 1; }\n")
    os.chmod(hook, 0o755)
    n1_sha = sh(work, "git", "rev-parse", "wip/10-n1")
    rc, out = ship("merge", "wip/10-n1", "--wait", "5", "--out", os.path.join(tmp, "o1"), branch="wip/10-n1")
    check("N1: merge from a checkout on old master fast-forwards it first, so the pre-push hook passes",
          rc == 0 and "SHIP_MERGED wip/10-n1" in out and om() == n1_sha, out)
    check("N1: ...and that checkout is now at the merged commit", sh(work, "git", "rev-parse", "HEAD") == n1_sha,
          sh(work, "git", "log", "-1", "--oneline"))
    os.remove(hook)
    sh(work, "git", "checkout", "-q", "-b", "wip/11-n1b", "master")
    open(os.path.join(work, "liquidgov-platform", "assets", "n1b.css"), "w").write("n1b\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "n1b")
    sh(work, "git", "push", "-q", "origin", "wip/11-n1b")
    open(os.path.join(work, "liquidgov-platform", "assets", "dirty.css"), "w").write("x\n")
    sh(work, "git", "checkout", "-q", "master")
    before = om()
    rc, out = ship("merge", "wip/11-n1b", "--wait", "5", "--out", os.path.join(tmp, "o2"), branch="wip/11-n1b")
    check("N1: a dirty checkout is refused instead of fast-forwarded, and master stays put",
          rc == 1 and "clean master checkout" in out and om() == before, out)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
