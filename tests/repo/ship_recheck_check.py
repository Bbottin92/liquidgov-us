"""ship_recheck_check.py: scripts/ship.py check behaves when it is run again, and when it is run right after a push
(both met on 2026-11-14, Brandon's spatial1 / bubble1 deploys from PowerShell).
  again:  the real gh will not write a downloaded file over one that is already there, so the second check of a branch
          (ship.py merge runs one after a check) printed "screenshots: NONE ... The file exists" and, for a site
          change, refused with "no phone screenshots". The fake gh here refuses the same way.
  early:  GitHub lists a run a few seconds after the push; a check started at once saw no run and, for a change with
          no site files, said SHIP_READY with no verdicts. ship.py now looks again a few times before believing it.
Throwaway repo with a real bare origin and a fake gh, as ship_check.py (not edited).
Usage: python tests/repo/ship_recheck_check.py [repo root]   -> last line passes=N fails=M; exit 1 on any FAIL."""
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
if a[:2] == ["run", "list"]:
    if "--workflow" in a:
        runs = s["runs"]
        cur = runs.pop(0) if len(runs) > 1 else runs[0]
        s["looks"] = s.get("looks", 0) + 1
        json.dump(s, open(state_path, "w", encoding="utf-8"))
        print(json.dumps([dict(cur, workflowName="platform")] if cur else []))
    else:
        print("[]")
elif a[:2] == ["run", "view"]:
    print(s.get("log", ""))
elif a[:2] == ["run", "download"]:
    d = a[a.index("-D") + 1]
    dst = os.path.join(d, "phone", "map.png")
    if os.path.exists(dst):   # as the real gh: it never writes over a file
        sys.stderr.write("error extracting zip archive: open %s: The file exists.\n" % dst); sys.exit(1)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, "w", encoding="utf-8").write(s.get("picture", "png"))
else:
    sys.stderr.write("fake gh: unexpected %r\n" % a); sys.exit(3)
'''
GREEN = {"databaseId": 7, "status": "completed", "conclusion": "success"}
LOG_OK = "phone\t2026-11-14T00:00:00Z VERDICT ci: ALL-PASS  (9 checks; report r.json)"


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-600:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


def ship(tmp, branch, state, wait="20"):
    sp = os.path.join(tmp, "gh_state.json")
    json.dump(state, open(sp, "w", encoding="utf-8"))
    env = dict(ENV, SHIP_REPO=os.path.join(tmp, "work"), SHIP_NO_FETCH="1", SHIP_POLL_SECONDS="0",
               SHIP_GH=json.dumps([sys.executable, os.path.join(tmp, "fake_gh.py")]), FAKE_GH_STATE=sp)
    p = subprocess.run([sys.executable, SHIP, "check", branch, "--wait", wait, "--out", os.path.join(tmp, "out", branch)],
                       capture_output=True, text=True, env=env, timeout=120)
    return p.returncode, p.stdout + p.stderr, json.load(open(sp, encoding="utf-8")).get("looks", 0)


def branch_with(work, name, path):
    sh(work, "git", "checkout", "-q", "-b", name, "master")
    full = os.path.join(work, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    open(full, "w", encoding="utf-8").write(name + "\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", name)
    sh(work, "git", "push", "-q", "origin", name)
    sh(work, "git", "checkout", "-q", "master")


tmp = tempfile.mkdtemp(prefix="ship-recheck-")
try:
    open(os.path.join(tmp, "fake_gh.py"), "w", encoding="utf-8").write(FAKE_GH)
    origin, work = os.path.join(tmp, "origin.git"), os.path.join(tmp, "work")
    sh(tmp, "git", "init", "-q", "--bare", origin)
    sh(tmp, "git", "init", "-q", "-b", "master", work)
    sh(work, "git", "config", "user.email", "t@t"); sh(work, "git", "config", "user.name", "t")
    open(os.path.join(work, "README.md"), "w").write("x\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin); sh(work, "git", "push", "-q", "origin", "master")
    branch_with(work, "wip/1-site", "liquidgov-platform/assets/a.css")
    branch_with(work, "wip/2-ci", ".github/workflows/platform.yml")
    sh(work, "git", "fetch", "-q", "origin")
    shot = os.path.join(tmp, "out", "wip/1-site", "phone", "map.png")

    # ---- again: a second check of the same branch
    rc, out, _ = ship(tmp, "wip/1-site", {"runs": [GREEN], "log": LOG_OK, "picture": "first"})
    check("A0 precondition: the first check of a site change is ready and has its screenshot",
          rc == 0 and "SHIP_READY wip/1-site" in out and os.path.exists(shot), out)
    rc, out, _ = ship(tmp, "wip/1-site", {"runs": [GREEN], "log": LOG_OK, "picture": "second"})
    check("A1 the second check (gh will not overwrite) is ready too", rc == 0 and "SHIP_READY wip/1-site" in out, out)
    check("A1 ...it lists the screenshot, and says nothing of a failed download",
          "map.png" in out and "screenshots: NONE" not in out and "no phone screenshots" not in out, out)
    check("A2 the picture on disk is the one from the second run, not a stale one",
          os.path.exists(shot) and open(shot, encoding="utf-8").read() == "second",
          os.path.exists(shot) and open(shot, encoding="utf-8").read())
    left = [n for n in os.listdir(os.path.dirname(os.path.join(tmp, "out", "wip/1-site"))) if n.startswith(".ship-shots-")]
    check("A3 no download folder is left behind", not left, left)
    rc, out, _ = ship(tmp, "wip/1-site", {"runs": [dict(GREEN, databaseId=8)], "log": LOG_OK, "picture": ""})
    # control: when the download really has nothing, a site change is still refused (fails closed)
    open(os.path.join(tmp, "fake_gh.py"), "w", encoding="utf-8").write(
        FAKE_GH.replace('if os.path.exists(dst):', 'if True:'))
    rc, out, _ = ship(tmp, "wip/1-site", {"runs": [GREEN], "log": LOG_OK})
    check("A4 control: a download that fails still refuses a site change", rc == 1 and "no phone screenshots" in out, out)
    open(os.path.join(tmp, "fake_gh.py"), "w", encoding="utf-8").write(FAKE_GH)

    # ---- early: the run is not listed yet when the check starts
    rc, out, looks = ship(tmp, "wip/2-ci", {"runs": [None, None, GREEN], "log": LOG_OK})
    check("E1 no site files, run listed on the third look: the check looks again and reads that run",
          rc == 0 and "looking again" in out and "CI run 7: completed/success" in out and "VERDICT ci: ALL-PASS" in out
          and "no platform run (" not in out, out)
    rc, out, looks = ship(tmp, "wip/1-site", {"runs": [None, GREEN], "log": LOG_OK, "picture": "third"})
    check("E2 a site change, run listed on the second look: ready (was: no CI run for the head commit)",
          rc == 0 and "SHIP_READY wip/1-site" in out, out)
    rc, out, looks = ship(tmp, "wip/2-ci", {"runs": [None], "log": ""})
    check("E3 control: no run ever, no site files: it gives up after a few looks and is ready as before",
          rc == 0 and "SHIP_READY wip/2-ci" in out and 2 <= looks <= 6, (looks, out))
    rc, out, looks = ship(tmp, "wip/1-site", {"runs": [None], "log": ""})
    check("E4 control: no run ever, site change: still not ready", rc == 1 and "no CI run for the head commit" in out, out)
    rc, out, looks = ship(tmp, "wip/2-ci", {"runs": [None, GREEN], "log": LOG_OK}, wait="0")
    check("E5 --wait 0 does not look again", looks == 1 and "looking again" not in out, (looks, out))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
