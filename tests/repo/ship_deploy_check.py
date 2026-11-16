"""ship_deploy_check.py: scripts/ship.py deploy / go (Brandon 2026-11-11 "yes build it").
Throwaway repo with a real bare origin; the server side is faked: deploy/sync-to-droplet.sh prints rsync itemize lines,
deploy/make-go.py writes a fake pair and records its arguments, the generated go script prints the real markers. The
shell is SHIP_SH (bash -c here, wsl.exe bash -lc for real). No network, no server.
  deploy: dry-run file list -> make-go auto <name> "<purpose>" <files> -> pair committed and pushed to origin/master ->
          go.sh dry -> SHIP_DRY_DONE. Refuses: dirty tree; master behind origin; nothing differs from live; a dry
          run whose CI gate would refuse; a bad name; an existing pair is reused (no second make-go, no new commit).
  go:     go.sh go -> CHECK_DONE + RECORD lines -> SHIP_LIVE and the RECORD lines printed. Refuses/fails: no pair;
          go exits non-zero; exit 0 without CHECK_DONE; RECORD_FAILED.
Usage: python tests/repo/ship_deploy_check.py [repo root]  -> last line passes=N fails=M; exit 1 on any FAIL."""
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
echo "Syncing liquidgov-platform/ -> x [dry run]"
if [ -f "$FAKE_DIR/no_changes" ]; then exit 0; fi
echo "cd+++++++++ assets/"
echo ".f....og... CMakeLists.txt"
echo ".d....og... ./"
echo ">f.st...... assets/a.css"
echo ">f+++++++++ src/b.cpp"
echo "Sync done."
'''

FAKE_MAKE_GO = r'''import os, sys, json
open(os.path.join(os.environ["FAKE_DIR"], "make_go_args.json"), "w").write(json.dumps(sys.argv[1:]))
name = sys.argv[2]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
go = os.path.join(root, "deploy", name + "-go.sh")
open(go, "w", newline="\n").write("""#!/usr/bin/env bash
mode="$(cat "$FAKE_DIR/go_mode" 2>/dev/null || echo ok)"
if [ "$1" = dry ]; then
  if [ "$mode" = gate_refuse ]; then echo "CI_GATE_WOULD_REFUSE red"
  elif [ "$mode" = gate_warn ]; then echo "CI_GATE_WARNING no run -- go would warn, not stop"
  else echo "CI_GATE_OK tree"; fi
  echo DRY_OK
  echo "DRY_ONLY_DONE: nothing on the server was changed. Run with go to deploy."
  exit 0
fi
case "$mode" in
  fail) echo "PREFLIGHT_FAILED"; exit 1 ;;
  nocheck) echo GO_DONE; exit 0 ;;
  recfail) echo GO_DONE; echo CHECK_DONE; echo "RECORD_FAILED: commit '' / PID ''"; exit 0 ;;
esac
echo BACKUP_DONE; echo GO_DONE; echo CHECK_DONE
echo "RECORD ctx log: python ctx.py log <who> deploy \\"thing (NAME)\\" --result ok --ref abc1234"
echo "RECORD notes.md: - **2026-11-11 09:00 PT -- thing (NAME) LIVE (commit abc1234, PID 42, public checks FAILS 0).**"
""".replace("NAME", name))
open(os.path.join(root, "deploy", name + "-remote.sh"), "w", newline="\n").write("#!/usr/bin/env bash\n")
print("auto: care pair (code changes)")
'''


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-700:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


def ship(tmp, work, *args):
    env = dict(ENV, SHIP_MAIN=work, SHIP_REPO=work, SHIP_SH=json.dumps([BASH, "-c"]), FAKE_DIR=tmp,
               SHIP_LOG_DIR=os.path.join(tmp, "logs"))
    p = subprocess.run([sys.executable, SHIP] + list(args), capture_output=True, text=True, env=env, timeout=180)
    return p.returncode, p.stdout + p.stderr


tmp = tempfile.mkdtemp(prefix="ship-deploy-")
try:
    check("precondition: scripts/ship.py exists", os.path.isfile(SHIP), SHIP)
    origin, work = os.path.join(tmp, "origin.git"), os.path.join(tmp, "work")
    sh(tmp, "git", "init", "-q", "--bare", origin)
    sh(tmp, "git", "init", "-q", "-b", "master", work)
    sh(work, "git", "config", "user.email", "t@t"); sh(work, "git", "config", "user.name", "t")
    os.makedirs(os.path.join(work, "deploy"))
    open(os.path.join(work, "deploy", "sync-to-droplet.sh"), "w", newline="\n").write(FAKE_SYNC)
    open(os.path.join(work, "deploy", "make-go.py"), "w", newline="\n").write(FAKE_MAKE_GO)
    open(os.path.join(work, "naming.toml"), "w").write("x = 1\n")
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin); sh(work, "git", "push", "-q", "-u", "origin", "master")
    om = lambda: sh(origin, "git", "rev-parse", "master")
    mode = lambda m: open(os.path.join(tmp, "go_mode"), "w").write(m)

    rc, out = ship(tmp, work, "deploy", "Bad-Name", "x")
    check("deploy refuses a name that is not lowercase letters/digits", rc == 2 or (rc == 1 and "name" in out), out)

    open(os.path.join(work, "stray.txt"), "w").write("x")
    rc, out = ship(tmp, work, "deploy", "thing1", "a thing")
    check("deploy refuses a dirty working copy", rc == 1 and "SHIP_NOT_READY" in out and "clean" in out, out)
    os.remove(os.path.join(work, "stray.txt"))

    open(os.path.join(tmp, "no_changes"), "w").write("1")
    rc, out = ship(tmp, work, "deploy", "thing1", "a thing")
    check("deploy refuses when nothing differs from live", rc == 1 and "nothing differs" in out, out)
    os.remove(os.path.join(tmp, "no_changes"))

    before = om()
    mode("ok")
    rc, out = ship(tmp, work, "deploy", "thing1", "a thing")
    check("deploy: dry-run files -> pair -> commit + push -> go.sh dry -> SHIP_DRY_DONE",
          rc == 0 and "SHIP_DRY_DONE thing1" in out, out)
    args = json.load(open(os.path.join(tmp, "make_go_args.json")))
    check("...make-go got auto + name + purpose + exactly the changed FILES (no directory lines, and no "
          "owner/group-only '.f....og...' lines: real dry run 2026-11-11 listed 149 of those)",
          args == ["auto", "thing1", "a thing", "assets/a.css", "src/b.cpp"], args)
    check("...the pair is on its own deploy branch and origin/master did NOT move (H2, 2026-11-11)",
          om() == before and "deploy/thing1-go.sh" in sh(origin, "git", "show", "--name-only", "--format=",
          sh(origin, "git", "branch", "--list", "wip/*-deploy-thing1").strip(" *")), out)
    check("...the dry run's own lines are shown", "DRY_ONLY_DONE" in out and "CI_GATE_OK" in out, out)

    os.remove(os.path.join(tmp, "make_go_args.json"))
    head = om()
    rc, out = ship(tmp, work, "deploy", "thing1", "a thing")
    check("deploy again with the pair already committed: reuses it (no make-go, no new commit)",
          rc == 0 and not os.path.exists(os.path.join(tmp, "make_go_args.json")) and om() == head, out)

    mode("gate_refuse")
    rc, out = ship(tmp, work, "deploy", "thing1", "a thing")
    check("a dry run whose CI gate would refuse: not done", rc == 1 and "CI_GATE_WOULD_REFUSE" in out
          and "SHIP_DRY_DONE" not in out, out)

    mode("gate_warn")
    rc, out = ship(tmp, work, "deploy", "thing1", "a thing")
    check("a dry run whose CI gate would only warn (warn mode): done, the warning shown", rc == 0 and "CI_GATE_WARNING" in out
          and "SHIP_DRY_DONE" in out, out)

    other = os.path.join(tmp, "other")
    sh(tmp, "git", "clone", "-q", "-b", "master", origin, other)
    sh(other, "git", "config", "user.email", "t@t"); sh(other, "git", "config", "user.name", "t")
    open(os.path.join(other, "README.md"), "w").write("y\n")
    sh(other, "git", "add", "."); sh(other, "git", "commit", "-q", "-m", "elsewhere")
    sh(other, "git", "push", "-q", "origin", "master")
    check("precondition: origin/master moved past local master", om() != sh(work, "git", "rev-parse", "master"))
    mode("ok")
    rc, out = ship(tmp, work, "deploy", "thing1", "a thing")
    check("deploy refuses when local master is behind origin/master", rc == 1 and "origin/master" in out, out)
    sh(work, "git", "pull", "-q", "--ff-only")
    dbr = sh(origin, "git", "branch", "--list", "wip/*-deploy-thing1").strip(" *")  # "merge" the pair (H2): go needs it
    sh(work, "git", "fetch", "-q", "origin"); sh(work, "git", "merge", "-q", "--no-edit", "origin/" + dbr)
    sh(work, "git", "push", "-q", "origin", "master")

    rc, out = ship(tmp, work, "go", "nosuchpair")
    check("go refuses a pair that does not exist", rc == 1 and "SHIP_GO_FAILED" in out, out)
    mode("ok")
    rc, out = ship(tmp, work, "go", "thing1")
    check("go: CHECK_DONE + RECORD lines -> SHIP_LIVE, records printed",
          rc == 0 and "SHIP_LIVE thing1" in out and "RECORD ctx log:" in out and "RECORD notes.md:" in out, out)
    mode("fail")
    rc, out = ship(tmp, work, "go", "thing1")
    check("go script exits non-zero: SHIP_GO_FAILED", rc == 1 and "SHIP_GO_FAILED" in out and "PREFLIGHT_FAILED" in out, out)
    mode("nocheck")
    rc, out = ship(tmp, work, "go", "thing1")
    check("exit 0 without CHECK_DONE: SHIP_GO_FAILED", rc == 1 and "SHIP_GO_FAILED" in out, out)
    mode("recfail")
    rc, out = ship(tmp, work, "go", "thing1")
    check("RECORD_FAILED: SHIP_GO_FAILED (records must be written by hand)", rc == 1 and "RECORD_FAILED" in out, out)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
