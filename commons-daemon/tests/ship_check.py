"""ship_check.py: scripts/ship.py check <branch> (Brandon 2026-11-11 "yes fix the guard, then do ship").
Runs ship.py against a throwaway repo with a real bare "origin" and a fake gh (no network, no GitHub), both directions:
  ready:   pushed + fast-forward + green run + VERDICT ALL-PASS + screenshots (code change -> pair care); assets-only
           change -> pair jbshell; docs-only change with no run -> ready, "no deploy needed"; a run still in progress
           is waited for and then counts.
  refuses: red run; VERDICT not ALL-PASS; no screenshots; unpushed commit; master moved on (not a fast-forward);
           site change with no CI run; gh failing (fails closed); a run still in progress past --wait.
Usage: python commons-daemon/tests/ship_check.py [repo root]   -> last line passes=N fails=M; exit 1 on any FAIL."""
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
state_path = os.environ["FAKE_GH_STATE"]
s = json.load(open(state_path, encoding="utf-8"))
a = sys.argv[1:]
if s.get("broken"):
    sys.stderr.write("gh: network down\n"); sys.exit(1)
if a[:2] == ["run", "list"]:
    runs = s["runs"]
    cur = runs.pop(0) if len(runs) > 1 else runs[0]
    json.dump(s, open(state_path, "w", encoding="utf-8"))
    print(json.dumps([cur] if cur else []))
elif a[:2] == ["run", "view"]:
    print(s.get("log", ""))
elif a[:2] == ["run", "download"]:
    if not s.get("pngs"):
        sys.stderr.write("no artifact\n"); sys.exit(1)
    d = a[a.index("-D") + 1]
    os.makedirs(os.path.join(d, "phone"), exist_ok=True)
    open(os.path.join(d, "phone", "map.png"), "wb").write(b"png")
else:
    sys.stderr.write("fake gh: unexpected %r\n" % a); sys.exit(3)
'''


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-600:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


GREEN = {"databaseId": 7, "status": "completed", "conclusion": "success"}
LOG_OK = "phone\t2026-11-11T00:00:00Z VERDICT ci: ALL-PASS  (9 checks; report r.json)"


def ship(tmp, branch, state, wait="20", extra_env=None):
    sp = os.path.join(tmp, "gh_state.json")
    json.dump(state, open(sp, "w", encoding="utf-8"))
    env = dict(ENV, SHIP_REPO=os.path.join(tmp, "work"), SHIP_NO_FETCH="1", SHIP_POLL_SECONDS="0",
               SHIP_GH=json.dumps([sys.executable, os.path.join(tmp, "fake_gh.py")]), FAKE_GH_STATE=sp)
    env.update(extra_env or {})
    p = subprocess.run([sys.executable, SHIP, "check", branch, "--wait", wait, "--out", os.path.join(tmp, "out", branch)],
                       capture_output=True, text=True, env=env, timeout=120)
    return p.returncode, p.stdout + p.stderr


def branch_with(work, name, path, push=True):
    sh(work, "git", "checkout", "-q", "-b", name, "master")
    full = os.path.join(work, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    open(full, "w", encoding="utf-8").write(name + "\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", name)
    if push:
        sh(work, "git", "push", "-q", "origin", name)
    sh(work, "git", "checkout", "-q", "master")


tmp = tempfile.mkdtemp(prefix="ship-check-")
try:
    check("precondition: scripts/ship.py exists", os.path.isfile(SHIP), SHIP)
    open(os.path.join(tmp, "fake_gh.py"), "w", encoding="utf-8").write(FAKE_GH)
    origin, work = os.path.join(tmp, "origin.git"), os.path.join(tmp, "work")
    sh(tmp, "git", "init", "-q", "--bare", origin)
    sh(tmp, "git", "init", "-q", "-b", "master", work)
    sh(work, "git", "config", "user.email", "t@t"); sh(work, "git", "config", "user.name", "t")
    open(os.path.join(work, "README.md"), "w").write("x\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin); sh(work, "git", "push", "-q", "origin", "master")
    sh(work, "git", "fetch", "-q", "origin")
    branch_with(work, "wip/1-code", "liquidgov-platform/src/a.cpp")
    branch_with(work, "wip/2-assets", "liquidgov-platform/assets/a.css")
    branch_with(work, "wip/3-docs", "Persistence/a.md")
    branch_with(work, "wip/4-local", "liquidgov-platform/src/b.cpp", push=False)
    sh(work, "git", "fetch", "-q", "origin")
    check("precondition: wip/4-local has no origin copy",
          "wip/4-local" not in sh(work, "git", "branch", "-r"))

    ok_state = {"runs": [GREEN], "log": LOG_OK, "pngs": True}
    rc, out = ship(tmp, "wip/1-code", ok_state)
    check("code change, green, ALL-PASS, screenshots: SHIP_READY", rc == 0 and "SHIP_READY wip/1-code" in out, out)
    check("...picks the build+restart pair (care)", "deploy pair: care" in out, out)
    check("...lists the screenshot it downloaded", "map.png" in out, out)
    check("...prints the merge command but did not merge",
          "merge --ff-only wip/1-code" in out and sh(work, "git", "rev-parse", "master") == sh(work, "git", "rev-parse", "origin/master"), out)
    rc, out = ship(tmp, "wip/2-assets", ok_state)
    check("assets-only change: ready with the no-restart pair (jbshell)", rc == 0 and "deploy pair: jbshell" in out, out)
    rc, out = ship(tmp, "wip/3-docs", {"runs": [None], "log": "", "pngs": False})
    check("docs-only change with no CI run: ready, no deploy needed",
          rc == 0 and "no deploy needed" in out and "SHIP_READY" in out, out)
    rc, out = ship(tmp, "wip/1-code", {"runs": [{"databaseId": 7, "status": "in_progress", "conclusion": ""}, GREEN],
                                       "log": LOG_OK, "pngs": True})
    check("a run in progress is waited for, then counts", rc == 0 and "waiting" in out and "SHIP_READY" in out, out)

    rc, out = ship(tmp, "wip/1-code", dict(ok_state, runs=[dict(GREEN, conclusion="failure")]))
    check("red CI run: not ready", rc == 1 and "SHIP_NOT_READY" in out and "CI failure" in out, out)
    rc, out = ship(tmp, "wip/1-code", dict(ok_state, log="VERDICT ci: FAILS: 4c tap"))
    check("VERDICT not ALL-PASS: not ready", rc == 1 and "phone VERDICT not ALL-PASS" in out, out)
    rc, out = ship(tmp, "wip/1-code", dict(ok_state, pngs=False))
    check("no screenshots for a site change: not ready", rc == 1 and "no phone screenshots" in out, out)
    rc, out = ship(tmp, "wip/4-local", ok_state)
    check("unpushed branch: not ready", rc == 1 and "not pushed" in out, out)
    rc, out = ship(tmp, "wip/1-code", {"runs": [None], "log": "", "pngs": False})
    check("site change with no CI run: not ready", rc == 1 and "no CI run" in out, out)
    rc, out = ship(tmp, "wip/1-code", dict(ok_state, broken=True))
    check("gh failing: not ready (fails closed)", rc == 1 and "SHIP_NOT_READY" in out and "network down" in out, out)
    rc, out = ship(tmp, "wip/1-code", {"runs": [{"databaseId": 7, "status": "in_progress", "conclusion": ""}],
                                       "log": LOG_OK, "pngs": True}, wait="0")
    check("still running past --wait: not ready", rc == 1 and "still running" in out, out)

    # ---- merge: only a READY branch moves origin/master, and only by fast-forward
    def merge(branch, state):
        sp = os.path.join(tmp, "gh_state.json")
        json.dump(state, open(sp, "w", encoding="utf-8"))
        env = dict(ENV, SHIP_REPO=work, SHIP_NO_FETCH="1", SHIP_POLL_SECONDS="0", FAKE_GH_STATE=sp,
                   SHIP_GH=json.dumps([sys.executable, os.path.join(tmp, "fake_gh.py")]))
        p = subprocess.run([sys.executable, SHIP, "merge", branch, "--wait", "0", "--out", os.path.join(tmp, "out", "m")],
                           capture_output=True, text=True, env=env, timeout=120)
        return p.returncode, p.stdout + p.stderr
    om = lambda: sh(origin, "git", "rev-parse", "master")
    before = om()
    rc, out = merge("wip/1-code", dict(ok_state, runs=[dict(GREEN, conclusion="failure")]))
    check("merge of a NOT ready branch refuses and leaves origin/master alone",
          rc == 1 and "SHIP_NOT_READY" in out and om() == before, out)
    rc, out = merge("wip/1-code", ok_state)
    check("merge of a ready branch moves origin/master to exactly the branch head",
          rc == 0 and om() == sh(work, "git", "rev-parse", "wip/1-code") and "SHIP_MERGED" in out, out)
    sh(work, "git", "fetch", "-q", "origin")
    rc, out = merge("wip/2-assets", ok_state)
    check("merge of a branch that is no longer a fast-forward refuses (no force push)",
          rc == 1 and om() == sh(work, "git", "rev-parse", "wip/1-code"), out)

    sh(work, "git", "fetch", "-q", "origin"); sh(work, "git", "merge", "-q", "--ff-only", "origin/master")
    open(os.path.join(work, "README.md"), "w").write("moved on\n")
    sh(work, "git", "commit", "-qam", "master moves"); sh(work, "git", "push", "-q", "origin", "master")
    sh(work, "git", "fetch", "-q", "origin")
    rc, out = ship(tmp, "wip/1-code", ok_state)
    check("master moved on (not a fast-forward): not ready", rc == 1 and "not a fast-forward" in out, out)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
