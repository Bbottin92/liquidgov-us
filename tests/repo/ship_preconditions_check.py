"""ship_preconditions_check.py: ship.py check/merge look at the real local and origin branches
before acting, and read GitHub's origin/master after every push attempt (Slice 1 of the
reliability handoff; also the 2026-11-23 merge misses: a branch that existed only on origin
or in another clone, and a push that landed but was reported as failed).

New file; does not edit existing ship tests. Usage: python tests/repo/ship_preconditions_check.py [repo root]
Last line: passes=N fails=M; exit 1 on any FAIL. Expected checks are pinned."""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SHIP = os.path.join(ROOT, "scripts", "ship.py")
sys.path.insert(0, os.path.join(ROOT, "scripts"))
from hook_env import clean_env  # noqa: E402  (scripts/hook_env.py)

ENV = clean_env()
ENV.pop("LIQUIDGOV_CI_GATE_MODE", None)
passes = fails = ran = 0
EXPECTED = 27

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

GREEN = {"databaseId": 7, "status": "completed", "conclusion": "success"}
LOG_OK = "phone\t2026-11-23T00:00:00Z VERDICT ci: ALL-PASS  (9 checks; report r.json)"


def check(name, ok, detail=""):
    global passes, fails, ran
    ran += 1
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-700:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


def try_sha(cwd, ref):
    p = subprocess.run(["git", "rev-parse", "--verify", ref], cwd=cwd, capture_output=True, text=True, env=ENV)
    return p.stdout.strip() if p.returncode == 0 else ""


def run_ship(tmp, work, action, branch, state, extra_env=None):
    sp = os.path.join(tmp, "gh_state.json")
    json.dump(state, open(sp, "w", encoding="utf-8"))
    out = os.path.join(tmp, "out", action)
    os.makedirs(out, exist_ok=True)
    env = dict(ENV, SHIP_REPO=work, SHIP_NO_FETCH="1", SHIP_POLL_SECONDS="0", FAKE_GH_STATE=sp,
               SHIP_GH=json.dumps([sys.executable, os.path.join(tmp, "fake_gh.py")]))
    env.update(extra_env or {})
    p = subprocess.run([sys.executable, SHIP, action, branch, "--wait", "0", "--out", out],
                       capture_output=True, text=True, env=env, timeout=120)
    return p.returncode, p.stdout + p.stderr, out


check("precondition: scripts/ship.py exists", os.path.isfile(SHIP), SHIP)

tmp = tempfile.mkdtemp(prefix="ship-preconditions-")
try:
    open(os.path.join(tmp, "fake_gh.py"), "w", encoding="utf-8").write(FAKE_GH)
    origin, work = os.path.join(tmp, "origin.git"), os.path.join(tmp, "work")
    sh(tmp, "git", "init", "-q", "--bare", origin)
    sh(tmp, "git", "init", "-q", "-b", "master", work)
    sh(work, "git", "config", "user.email", "t@t")
    sh(work, "git", "config", "user.name", "t")
    open(os.path.join(work, "README.md"), "w", encoding="utf-8").write("x\n")
    sh(work, "git", "add", ".")
    sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin)
    sh(work, "git", "push", "-q", "origin", "master")

    def make_branch(name, path="liquidgov-platform/src/a.cpp", text=None):
        sh(work, "git", "checkout", "-q", "-b", name, "master")
        full = os.path.join(work, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        open(full, "w", encoding="utf-8").write(text or (name + "\n"))
        sh(work, "git", "add", ".")
        sh(work, "git", "commit", "-q", "-m", name)
        sh(work, "git", "push", "-q", "origin", name)
        sh(work, "git", "checkout", "-q", "master")
        sh(work, "git", "fetch", "-q", "origin")

    make_branch("wip/1-code")
    ok_state = {"runs": [GREEN], "log": LOG_OK, "pngs": True}
    om = lambda: sh(origin, "git", "rev-parse", "master")
    before = om()
    branch_sha = sh(work, "git", "rev-parse", "wip/1-code")

    def reset_master():
        sh(origin, "git", "update-ref", "refs/heads/master", before)
        sh(work, "git", "fetch", "-q", "origin")
        sh(work, "git", "checkout", "-q", "-B", "master", "origin/master")

    def ensure_branch(sha):
        sh(work, "git", "update-ref", "refs/heads/wip/1-code", sha)
        if try_sha(work, "HEAD") == "" or sh(work, "git", "rev-parse", "--abbrev-ref", "HEAD") != "master":
            sh(work, "git", "checkout", "-q", "-B", "master", "origin/master")

    # ---- missing entirely (not local, not on origin)
    rc, out, outdir = run_ship(tmp, work, "check", "wip/does-not-exist", ok_state)
    check("missing branch: check refuses with a plain reason naming the branch",
          rc != 0 and "wip/does-not-exist" in out
          and "does not exist locally or on origin" in out
          and "SHIP_READY" not in out, out)
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/does-not-exist", ok_state)
    check("missing branch: merge refuses, leaves origin/master alone, does not print SHIP_MERGED",
          rc != 0 and "SHIP_MERGED" not in out and om() == before
          and "does not exist locally or on origin" in out, out)

    # ---- local missing, origin has the branch (the other-clone case)
    sh(work, "git", "branch", "-D", "wip/1-code")
    check("precondition: wip/1-code is gone locally but still on origin",
          "wip/1-code" not in sh(work, "git", "branch", "--list", "wip/1-code")
          and sh(work, "git", "rev-parse", "origin/wip/1-code") == branch_sha)
    rc, out, outdir = run_ship(tmp, work, "check", "wip/1-code", ok_state)
    local_now = try_sha(work, "refs/heads/wip/1-code")
    check("local missing, origin has it: check creates the local branch at the origin sha and is ready",
          rc == 0 and "SHIP_READY wip/1-code" in out and "created local branch" in out
          and local_now == branch_sha, out)
    if try_sha(work, "refs/heads/wip/1-code"):
        sh(work, "git", "branch", "-D", "wip/1-code")
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state)
    check("local missing, origin has it: merge creates the branch and lands that origin sha on master",
          rc == 0 and "SHIP_MERGED" in out and "created local branch" in out
          and om() == branch_sha, out)
    reset_master()
    ensure_branch(branch_sha)
    check("precondition: origin/master and local wip/1-code reset after the create-from-origin merge",
          om() == before and try_sha(work, "wip/1-code") == branch_sha)

    # ---- local strictly behind origin: advance (fast-forward), do not merge the stale sha
    sh(work, "git", "checkout", "-q", "wip/1-code")
    open(os.path.join(work, "liquidgov-platform", "src", "a.cpp"), "w", encoding="utf-8").write("newer\n")
    sh(work, "git", "add", ".")
    sh(work, "git", "commit", "-q", "-m", "newer-on-origin")
    sh(work, "git", "push", "-q", "origin", "wip/1-code")
    newer = sh(work, "git", "rev-parse", "HEAD")
    sh(work, "git", "reset", "-q", "--hard", branch_sha)
    sh(work, "git", "checkout", "-q", "master")
    sh(work, "git", "fetch", "-q", "origin")
    check("precondition: local wip/1-code is strictly behind origin/wip/1-code",
          sh(work, "git", "rev-parse", "wip/1-code") == branch_sha
          and sh(work, "git", "rev-parse", "origin/wip/1-code") == newer
          and branch_sha != newer)
    rc, out, outdir = run_ship(tmp, work, "check", "wip/1-code", ok_state)
    check("local behind origin: check fast-forwards the local branch to the origin sha and is ready",
          rc == 0 and "SHIP_READY wip/1-code" in out and "fast-forwarded local branch" in out
          and sh(work, "git", "rev-parse", "wip/1-code") == newer, out)
    # put local behind again for merge
    sh(work, "git", "branch", "-f", "wip/1-code", branch_sha)
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state)
    check("local behind origin: merge fast-forwards, then lands the NEWER origin sha, not the stale local one",
          rc == 0 and "SHIP_MERGED" in out and "fast-forwarded local branch" in out
          and om() == newer and om() != branch_sha, out)
    reset_master()
    ensure_branch(newer)
    check("precondition: origin/master reset after the behind-branch merge", om() == before)

    # ---- local ahead of origin (unpushed extra commit; origin still has the previous sha)
    sh(work, "git", "checkout", "-q", "wip/1-code")
    open(os.path.join(work, "liquidgov-platform", "src", "a.cpp"), "w", encoding="utf-8").write("ahead\n")
    sh(work, "git", "add", ".")
    sh(work, "git", "commit", "-q", "-m", "ahead-local")
    ahead = sh(work, "git", "rev-parse", "HEAD")
    sh(work, "git", "checkout", "-q", "master")
    check("precondition: local wip/1-code is ahead of origin/wip/1-code",
          sh(work, "git", "rev-parse", "wip/1-code") == ahead
          and sh(work, "git", "rev-parse", "origin/wip/1-code") == newer
          and ahead != newer)
    rc, out, outdir = run_ship(tmp, work, "check", "wip/1-code", ok_state)
    check("local ahead of origin: check refuses with both shas and does not move the local branch",
          rc != 0 and "ahead of origin/" in out and ahead[:7] in out and newer[:7] in out
          and "SHIP_READY" not in out
          and sh(work, "git", "rev-parse", "wip/1-code") == ahead, out)
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state)
    check("local ahead of origin: merge refuses, leaves origin/master and the local branch alone",
          rc != 0 and "SHIP_MERGED" not in out and om() == before
          and "ahead of origin/" in out
          and sh(work, "git", "rev-parse", "wip/1-code") == ahead, out)
    # restore local to the pushed sha for later cases
    sh(work, "git", "branch", "-f", "wip/1-code", newer)

    # ---- local diverged from origin
    # second work clone so we can push a competing commit to origin without touching local
    other = os.path.join(tmp, "other")
    sh(tmp, "git", "clone", "-q", origin, other)
    sh(other, "git", "config", "user.email", "t@t")
    sh(other, "git", "config", "user.name", "t")
    sh(other, "git", "checkout", "-q", "wip/1-code")
    open(os.path.join(other, "liquidgov-platform", "src", "a.cpp"), "w", encoding="utf-8").write("origin-side\n")
    sh(other, "git", "add", ".")
    sh(other, "git", "commit", "-q", "-m", "origin-side")
    sh(other, "git", "push", "-q", "--force", "origin", "wip/1-code")  # origin moved to a sibling of `newer`
    origin_side = sh(other, "git", "rev-parse", "HEAD")
    sh(work, "git", "fetch", "-q", "origin")
    # give local its own sibling commit (not an ancestor of origin_side, and origin_side not an ancestor of it)
    sh(work, "git", "checkout", "-q", "wip/1-code")
    # wip/1-code is still at `newer`; origin is at origin_side (also based on newer? wait:
    # other cloned after we reset local to `newer` and... other checked out wip/1-code from origin,
    # which was at `newer`, then committed origin-side. So origin_side is a child of newer.
    # If local stays at newer, local is BEHIND, not diverged. We need a sibling on local too.
    open(os.path.join(work, "liquidgov-platform", "src", "a.cpp"), "w", encoding="utf-8").write("local-side\n")
    sh(work, "git", "add", ".")
    sh(work, "git", "commit", "-q", "-m", "local-side")
    local_side = sh(work, "git", "rev-parse", "HEAD")
    sh(work, "git", "checkout", "-q", "master")
    check("precondition: local and origin/wip/1-code have diverged",
          local_side != origin_side
          and sh(work, "git", "rev-parse", "wip/1-code") == local_side
          and sh(work, "git", "rev-parse", "origin/wip/1-code") == origin_side)
    rc, out, outdir = run_ship(tmp, work, "check", "wip/1-code", ok_state)
    check("local diverged from origin: check refuses with both shas, does not merge the stale local commit",
          rc != 0 and "diverged from origin/" in out and local_side[:7] in out and origin_side[:7] in out
          and "SHIP_READY" not in out
          and sh(work, "git", "rev-parse", "wip/1-code") == local_side, out)
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state)
    check("local diverged from origin: merge refuses, origin/master unchanged, no SHIP_MERGED",
          rc != 0 and "SHIP_MERGED" not in out and om() == before
          and "diverged from origin/" in out
          and local_side[:7] in out and origin_side[:7] in out, out)

    # restore a clean, in-sync feature branch at origin_side for the push-readback cases
    sh(work, "git", "branch", "-f", "wip/1-code", origin_side)
    sh(work, "git", "checkout", "-q", "master")
    check("precondition: local wip/1-code matches origin again for push-readback cases",
          sh(work, "git", "rev-parse", "wip/1-code") == origin_side
          and sh(work, "git", "rev-parse", "origin/wip/1-code") == origin_side
          and om() == before)
    landed_sha = origin_side

    # ---- post-push read-back of the real origin/master
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state,
                               extra_env={"SHIP_PUSH_SIMULATE": "fail-after"})
    check("push reported failure but origin/master moved to the pushed sha: reports it landed on GitHub",
          rc == 0 and "SHIP_MERGED" in out and "landed on GitHub" in out
          and "SHIP_NOT_READY" not in out and om() == landed_sha
          and landed_sha[:7] in out, out)

    reset_master()
    ensure_branch(landed_sha)
    check("precondition: origin/master reset for the failed-and-not-landed case", om() == before)

    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state,
                               extra_env={"SHIP_PUSH_SIMULATE": "fail"})
    check("push reported failure and origin/master did not move: refuses, names the looked sha and push.log",
          rc != 0 and "SHIP_MERGED" not in out and om() == before
          and before[:7] in out and "push.log" in out, out)
    reset_master()
    ensure_branch(landed_sha)

    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state,
                               extra_env={"SHIP_PUSH_SIMULATE": "ok-no-push"})
    check("push reported success but origin/master is not the pushed sha: refuses to say landed, reports the mismatch",
          rc != 0 and "SHIP_MERGED" not in out and om() == before
          and "push reported success" in out and landed_sha[:7] in out and before[:7] in out, out)
    reset_master()
    ensure_branch(landed_sha)

    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state)
    check("push succeeded and origin/master matches the pushed sha: SHIP_MERGED with the verified remote sha",
          rc == 0 and "SHIP_MERGED" in out and om() == landed_sha
          and landed_sha[:7] in out, out)

    # cannot read origin/master after a push that actually landed: fail closed, do not claim landed
    reset_master()
    ensure_branch(landed_sha)
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state,
                               extra_env={"SHIP_FETCH_FAIL": "1", "SHIP_PUSH_SIMULATE": "fail-after"})
    check("cannot read origin/master after a push attempt: fail closed, does not print SHIP_MERGED",
          rc != 0 and "SHIP_MERGED" not in out and "could not read origin/master" in out, out)

    # reset again (the fail-after may have moved origin/master even though we did not claim it)
    reset_master()
    ensure_branch(landed_sha)

    # ---- corrupt / unreadable local ref
    ref_path = os.path.join(work, ".git", "refs", "heads", "wip", "1-code")
    open(ref_path, "w", encoding="utf-8").write("not-a-sha\n")
    rc, out, outdir = run_ship(tmp, work, "check", "wip/1-code", ok_state)
    check("corrupt local ref: check fails loudly with a plain reason, never silently ready",
          rc != 0 and "SHIP_READY" not in out
          and ("could not read" in out or "unreadable" in out or "corrupt" in out), out)
    rc, out, outdir = run_ship(tmp, work, "merge", "wip/1-code", ok_state)
    check("corrupt local ref: merge fails loudly, leaves origin/master alone, no SHIP_MERGED",
          rc != 0 and "SHIP_MERGED" not in out and om() == before
          and ("could not read" in out or "unreadable" in out or "corrupt" in out), out)

finally:
    shutil.rmtree(tmp, ignore_errors=True)

check("pinned check count", ran + 1 == EXPECTED, "would be %d, expected %d" % (ran + 1, EXPECTED))
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
