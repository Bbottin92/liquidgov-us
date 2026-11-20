"""ship_wip_check.py: scripts/ship.py deploy-wip / go-wip (Brandon 2026-11-19: "polish your workflow to create the least friction").
The same throwaway-repo method as ship_deploy_check.py: a real bare origin, a fake server side (sync dry run, make-go, a generated
go script), the shell is plain bash (SHIP_SH) and the "native WSL clone" is a temp dir (SHIP_NATIVE).
  deploy-wip: on a pushed wip/ branch: dry sync in the native clone (which must hold exactly the branch head) -> make-go auto with
              exactly the changed files -> pair committed and pushed ON THE BRANCH (master untouched) -> <name>-go.sh dry run
              until the CI gate says OK (a gate that says WOULD_REFUSE is retried, then given up on after --wait) -> SHIP_DRY_DONE.
              Refuses: master or a non-wip branch; a dirty tree; an unpushed head; nothing differing from live.
  go-wip:     the go script runs from the native clone with LIQUIDGOV_CI_GATE_MODE=block -> SHIP_LIVE + RECORD lines; refuses
              without a pair on the branch.
Usage: python tests/repo/ship_wip_check.py [repo root]  -> last line passes=N fails=M; exit 1 on any FAIL."""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SHIP = os.path.join(ROOT, "scripts", "ship.py")
BASH = shutil.which("bash") or "bash"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
passes = fails = 0

FAKE_SYNC = r'''#!/usr/bin/env bash
echo "Deploying commit abc1234"
if [ -f "$FAKE_DIR/no_changes" ]; then exit 0; fi
echo ".f....og... CMakeLists.txt"
echo ">f.st...... assets/a.css"
echo ">f+++++++++ src/b.cpp"
echo "Sync done."
'''

FAKE_MAKE_GO = r'''import os, sys, json
open(os.path.join(os.environ["FAKE_DIR"], "make_go_args.json"), "w").write(json.dumps(sys.argv[1:]))
name = sys.argv[2]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(root, "deploy", name + "-go.sh"), "w", newline="\n").write("""#!/usr/bin/env bash
echo "$LIQUIDGOV_CI_GATE_MODE $(pwd)" > "$FAKE_DIR/go_env_$1"
if [ "$1" = dry ]; then
  n=$(cat "$FAKE_DIR/gate_count" 2>/dev/null || echo 0); echo $((n+1)) > "$FAKE_DIR/gate_count"
  if [ "$n" -lt "$(cat "$FAKE_DIR/gate_refuse_n" 2>/dev/null || echo 0)" ]; then echo "CI_GATE_WOULD_REFUSE in_progress"; else echo "CI_GATE_OK tree"; fi
  echo DRY_OK; echo "DRY_ONLY_DONE: nothing changed."; exit 0
fi
echo GO_DONE; echo CHECK_DONE
echo "RECORD ctx log: python ctx.py log <who> deploy thing --result ok --ref abc1234"
echo "RECORD notes.md: - **2026-11-19 live.**"
""")
open(os.path.join(root, "deploy", name + "-remote.sh"), "w", newline="\n").write("#!/usr/bin/env bash\n")
print("auto: jbshell pair (assets/tests only)")
'''


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-700:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


def ship(tmp, work, *args):
    env = dict(ENV, SHIP_MAIN=work, SHIP_REPO=work, SHIP_SH=json.dumps([BASH, "-c"]), FAKE_DIR=tmp, SHIP_NATIVE=tmp + "/native",
               SHIP_LOG_DIR=tmp + "/logs", SHIP_POLL_SECONDS="0.2")
    p = subprocess.run([sys.executable, SHIP] + list(args), capture_output=True, text=True, env=env, timeout=120)
    return p.returncode, p.stdout + p.stderr


tmp = tempfile.mkdtemp(prefix="ship-wip-").replace("\\", "/")      # forward slashes: the fake shell is bash and these paths go into its commands
try:
    check("precondition: scripts/ship.py exists", os.path.isfile(SHIP), SHIP)
    origin, work, native = tmp + "/origin.git", tmp + "/work", tmp + "/native"
    sh(tmp, "git", "init", "-q", "--bare", origin)
    sh(tmp, "git", "init", "-q", "-b", "master", work)
    sh(tmp, "git", "init", "-q", "-b", "scratch", native)
    sh(work, "git", "config", "user.email", "t@t"); sh(work, "git", "config", "user.name", "t")
    os.makedirs(os.path.join(work, "deploy"))
    open(os.path.join(work, "deploy", "sync-to-droplet.sh"), "w", newline="\n").write(FAKE_SYNC)
    open(os.path.join(work, "deploy", "make-go.py"), "w", newline="\n").write(FAKE_MAKE_GO)
    open(os.path.join(work, "naming.toml"), "w").write("x = 1\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin); sh(work, "git", "push", "-q", "-u", "origin", "master")
    wait = lambda *a: None

    rc, out = ship(tmp, work, "deploy-wip", "thing1", "a thing")
    check("deploy-wip refuses master (it is for a wip/ branch)", rc == 1 and "wip/" in out and "SHIP_NOT_READY" in out, out)

    sh(work, "git", "checkout", "-q", "-b", "wip/9-x")
    open(os.path.join(work, "a.txt"), "w").write("1\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "work")
    rc, out = ship(tmp, work, "deploy-wip", "thing1", "a thing")
    check("deploy-wip refuses an unpushed head", rc == 1 and "not pushed" in out, out)
    sh(work, "git", "push", "-q", "-u", "origin", "wip/9-x")

    open(os.path.join(work, "stray.txt"), "w").write("x")
    rc, out = ship(tmp, work, "deploy-wip", "thing1", "a thing")
    check("deploy-wip refuses a dirty working copy", rc == 1 and "clean" in out, out)
    os.remove(os.path.join(work, "stray.txt"))

    open(os.path.join(tmp, "no_changes"), "w").write("1")
    rc, out = ship(tmp, work, "deploy-wip", "thing1", "a thing")
    check("deploy-wip refuses when nothing differs from live", rc == 1 and "nothing differs" in out, out)
    os.remove(os.path.join(tmp, "no_changes"))

    master_before = sh(origin, "git", "rev-parse", "master")
    open(os.path.join(tmp, "gate_refuse_n"), "w").write("2")
    rc, out = ship(tmp, work, "deploy-wip", "thing1", "a thing", "--wait", "1")
    check("deploy-wip: native dry sync -> make-go -> pair pushed on the branch -> dry retried until the CI gate says OK -> SHIP_DRY_DONE",
          rc == 0 and "SHIP_DRY_DONE thing1" in out and "CI not green yet" in out, out)
    args = json.load(open(os.path.join(tmp, "make_go_args.json")))
    check("...make-go got auto + name + purpose + exactly the changed files (no owner/group-only lines)",
          args == ["auto", "thing1", "a thing", "assets/a.css", "src/b.cpp"], args)
    check("...the pair is committed LOCALLY on the branch and NOT pushed (a push would start a second CI run that the gate waits for), master did not move",
          "deploy/thing1-go.sh" in sh(work, "git", "show", "--name-only", "--format=", "HEAD")
          and "deploy/thing1-go.sh" not in sh(origin, "git", "show", "--name-only", "--format=", "wip/9-x")
          and sh(origin, "git", "rev-parse", "master") == master_before and "NOT pushed" in out, out)
    check("...the dry runs ran in the native clone with the gate in block mode",
          open(os.path.join(tmp, "go_env_dry")).read().strip() == "block " + native.replace("\\", "/") or
          open(os.path.join(tmp, "go_env_dry")).read().startswith("block "), open(os.path.join(tmp, "go_env_dry")).read())
    check("...three dry runs: two refused, the third OK", open(os.path.join(tmp, "gate_count")).read().strip() == "3", out)

    open(os.path.join(tmp, "gate_refuse_n"), "w").write("999")
    rc, out = ship(tmp, work, "deploy-wip", "thing1", "a thing", "--wait", "0.02")
    check("a CI gate that never opens: gives up after --wait, not done", rc == 1 and "never went green" in out and "SHIP_DRY_DONE" not in out, out)
    os.remove(os.path.join(tmp, "gate_refuse_n"))

    rc, out = ship(tmp, work, "go-wip", "thing1")
    check("go-wip: runs the go script from the native clone with the gate in block mode -> SHIP_LIVE and both RECORD lines",
          rc == 0 and "SHIP_LIVE thing1" in out and "RECORD ctx log:" in out and "RECORD notes.md:" in out
          and open(os.path.join(tmp, "go_env_go")).read().startswith("block "), out)
    open(os.path.join(work, "src.txt"), "w").write("code\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "unpushed code")
    rc, out = ship(tmp, work, "go-wip", "thing1")
    check("a local commit that is not a deploy pair (code ahead of origin) is refused: only the pair may be unpushed", rc == 1 and "not pushed" in out, out)
    sh(work, "git", "reset", "-q", "--hard", "HEAD~1")
    rc, out = ship(tmp, work, "go-wip", "nopair")
    check("go-wip refuses a name with no pair on this branch", rc == 1 and "SHIP_GO_FAILED" in out and "no deploy/nopair-go.sh" in out, out)
    rc, out = ship(tmp, work, "deploy-wip", "Bad-Name", "x")
    check("a bad name is refused", rc == 2 or "SHIP_NOT_READY" in out or "Usage" in out or "ship:" in out, out)
    # ci-runs: the gate (deploy/ci-gate.sh) reads the NEWEST platform run for the liquidgov-platform tree; the pair commit does not change that tree
    cr = tmp + "/cr"
    sh(tmp, "git", "init", "-q", "-b", "master", cr)
    sh(cr, "git", "config", "user.email", "t@t"); sh(cr, "git", "config", "user.name", "t")
    os.makedirs(cr + "/liquidgov-platform"); os.makedirs(cr + "/deploy")
    open(cr + "/liquidgov-platform/a.txt", "w").write("1\n"); sh(cr, "git", "add", "."); sh(cr, "git", "commit", "-q", "-m", "c1")
    c1 = sh(cr, "git", "rev-parse", "HEAD")
    open(cr + "/deploy/pair.sh", "w").write("x\n"); sh(cr, "git", "add", "."); sh(cr, "git", "commit", "-q", "-m", "c2 pair only")
    c2 = sh(cr, "git", "rev-parse", "HEAD")
    open(cr + "/liquidgov-platform/a.txt", "w").write("2\n"); sh(cr, "git", "add", "."); sh(cr, "git", "commit", "-q", "-m", "c3 code")
    c3 = sh(cr, "git", "rev-parse", "HEAD")
    sh(cr, "git", "checkout", "-q", c2)
    fake_gh = tmp + "/fakegh.py"
    open(fake_gh, "w").write("import os,sys\nprint(open(os.environ['FAKE_RUNS']).read())\n")
    state = tmp + "/state.json"

    def runs(rows):
        open(tmp + "/runs.json", "w").write(json.dumps([{"databaseId": i, "headSha": s, "status": st, "conclusion": c, "createdAt": t, "headBranch": "b"}
                                                       for i, s, st, c, t in rows]))

    def ci(*extra):
        env = dict(ENV, SHIP_REPO=cr, SHIP_GH=json.dumps([sys.executable, fake_gh]), FAKE_RUNS=tmp + "/runs.json", CI_RUNS_STATE=state)
        p = subprocess.run([sys.executable, SHIP, "ci-runs"], capture_output=True, text=True, env=env, timeout=60)
        return p.returncode, p.stdout + p.stderr

    runs([(1, c1, "completed", "success", "2026-01-01T01:00:00Z"), (2, c2, "in_progress", None, "2026-01-01T02:00:00Z"),
          (3, c2, "in_progress", None, "2026-01-01T03:00:00Z"), (4, c3, "in_progress", None, "2026-01-01T04:00:00Z")])
    rc, out = ci()
    st = json.load(open(state))
    check("ci-runs: runs of the SAME tree (c1 and c2 share it, c3 does not); KEEP the newest (what the gate reads), the older in-flight one is the DUPLICATE",
          rc == 0 and st["keep"] == 3 and st["duplicates"] == [2] and "CI_RUNS_KEEP 3 DUPLICATES 2" in out, out + str(st))
    check("...a run of another tree (c3) is neither kept nor listed as a duplicate", 4 not in st["duplicates"] and "44" not in out.replace("4 ", "").replace("liquidgov", ""), out)
    runs([(1, c1, "completed", "success", "2026-01-01T01:00:00Z"), (3, c2, "completed", "cancelled", "2026-01-01T03:00:00Z")])
    rc, out = ci()
    check("ci-runs: a CANCELLED newest run makes the gate refuse even though an older run passed: it says so and names the rerun",
          rc == 0 and "GATE_VIEW" in out and "gh run rerun 3" in out and json.load(open(state))["duplicates"] == [], out)
    runs([])
    rc, out = ci()
    check("ci-runs with no run for the tree: nothing to keep or cancel", rc == 0 and "nothing to keep" in out and json.load(open(state))["keep"] is None, out)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
