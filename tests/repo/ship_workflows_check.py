"""ship_workflows_check.py: ship.py check reads EVERY workflow run on the head commit, not just platform.yml
(Brandon 2026-11-11 "yes build it"; real probe: a commons run on that commit had failed and ship ignored it).
Fake gh: `run list --workflow platform.yml` gives the platform run, `run list` without --workflow gives all runs.
  not ready: another workflow (commons) failed on the commit, with a green platform run; and with no platform run.
  ready: the other workflow was skipped or succeeded; an other workflow still running is reported but waited out by
  nobody: it counts as not ready (a red result could still come).
Usage: python tests/repo/ship_workflows_check.py [repo root] -> last line passes=N fails=M; exit 1 on FAIL."""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SHIP = os.path.join(ROOT, "scripts", "ship.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
passes = fails = 0

FAKE_GH = r'''
import json, os, sys
s = json.load(open(os.environ["FAKE_GH_STATE"], encoding="utf-8"))
a = sys.argv[1:]
if a[:2] == ["run", "list"] and "--workflow" in a:
    print(json.dumps([s["platform"]] if s.get("platform") else []))
elif a[:2] == ["run", "list"]:
    print(json.dumps(s.get("all", [])))
elif a[:2] == ["run", "view"]:
    print("x VERDICT ci: ALL-PASS  (1 checks)")
elif a[:2] == ["run", "download"]:
    d = a[a.index("-D") + 1]; os.makedirs(d, exist_ok=True); open(os.path.join(d, "p.png"), "wb").write(b"p")
else:
    sys.exit(3)
'''
GREEN = {"databaseId": 7, "status": "completed", "conclusion": "success"}
PLAT_OK = {"workflowName": "platform", "status": "completed", "conclusion": "success"}


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-500:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


tmp = tempfile.mkdtemp(prefix="ship-wf-")
try:
    open(os.path.join(tmp, "fake_gh.py"), "w", encoding="utf-8").write(FAKE_GH)
    origin, work = os.path.join(tmp, "origin.git"), os.path.join(tmp, "work")
    sh(tmp, "git", "init", "-q", "--bare", origin)
    sh(tmp, "git", "init", "-q", "-b", "master", work)
    sh(work, "git", "config", "user.email", "t@t"); sh(work, "git", "config", "user.name", "t")
    open(os.path.join(work, "README.md"), "w").write("x\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin); sh(work, "git", "push", "-q", "origin", "master")
    for b, path in (("wip/1-code", "liquidgov-platform/src/a.cpp"), ("wip/2-docs", "Persistence/a.md")):
        sh(work, "git", "checkout", "-q", "-b", b, "master")
        os.makedirs(os.path.dirname(os.path.join(work, path)), exist_ok=True)
        open(os.path.join(work, path), "w").write("x\n")
        sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", b); sh(work, "git", "push", "-q", "origin", b)
        sh(work, "git", "checkout", "-q", "master")
    sh(work, "git", "fetch", "-q", "origin")

    def ship(branch, state):
        sp = os.path.join(tmp, "state.json")
        json.dump(state, open(sp, "w", encoding="utf-8"))
        env = dict(ENV, SHIP_REPO=work, SHIP_NO_FETCH="1", SHIP_POLL_SECONDS="0", FAKE_GH_STATE=sp,
                   SHIP_GH=json.dumps([sys.executable, os.path.join(tmp, "fake_gh.py")]))
        p = subprocess.run([sys.executable, SHIP, "check", branch, "--wait", "0", "--out", os.path.join(tmp, "o", branch)],
                           capture_output=True, text=True, env=env, timeout=120)
        return p.returncode, p.stdout + p.stderr

    rc, out = ship("wip/1-code", {"platform": GREEN, "all": [PLAT_OK]})
    check("precondition: green platform run and nothing else -> ready", rc == 0 and "SHIP_READY" in out, out)
    rc, out = ship("wip/1-code", {"platform": GREEN, "all": [
        PLAT_OK, {"workflowName": "commons", "status": "completed", "conclusion": "failure"}]})
    check("commons failed on the same commit (platform green): not ready", rc == 1 and "commons failure" in out, out)
    rc, out = ship("wip/2-docs", {"platform": None, "all": [
        {"workflowName": "commons", "status": "completed", "conclusion": "failure"}]})
    check("commons failed and there is no platform run: not ready", rc == 1 and "commons failure" in out, out)
    rc, out = ship("wip/1-code", {"platform": GREEN, "all": [
        PLAT_OK, {"workflowName": "commons", "status": "completed", "conclusion": "skipped"}]})
    check("a skipped other workflow does not block", rc == 0 and "SHIP_READY" in out, out)
    rc, out = ship("wip/1-code", {"platform": GREEN, "all": [
        PLAT_OK, {"workflowName": "commons", "status": "in_progress", "conclusion": ""}]})
    check("an other workflow still running: not ready", rc == 1 and "commons still running" in out, out)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
