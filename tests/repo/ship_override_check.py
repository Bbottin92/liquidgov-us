"""ship_override_check.py: an override Brandon OK'd in chat goes through ship.py instead of hand-run steps (q274).
2026-11-20 nov20: the rollback guard refused a new record type (fmix); Brandon OK'd LIQUIDGOV_NO_ROLLBACK_OK, but ship
refuses every override variable (review M3), so the sync dry run, pair commit, push, go dry and go were run by hand.
And ship's refusal message was the run's last 300 characters, which cut the guard's record list off.
  O1 deploy with no override: refused, and the message holds the guard's record line in full plus the log path
  O2 deploy --override-file: SHIP_DRY_DONE; the sync dry run and the go script's dry run both get the variable (through
     an empty-env fake WSL, so only the command text can carry it), with Brandon's quoted words; the log names it
  O3 an override file for another deploy, a variable outside the allowed list, or a missing field: refused, nothing ran
  O4 go --override-file after the merge: SHIP_LIVE, and the go script got the variable
  O5 an override variable in the caller's own environment is still refused (M3 unchanged)
Usage: python tests/repo/ship_override_check.py [repo root] -> last line passes=N fails=M; exit 1 on any FAIL."""
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
VAR = "LIQUIDGOV_NO_ROLLBACK_OK"
passes = fails = 0

FAKE_WSL = '#!/usr/bin/env bash\nexec env -i PATH="$PATH" HOME="$HOME" FAKE_DIR="$FAKE_DIR" bash -c "$1"\n'
# Like the real sync-to-droplet.sh + rollback_guard.py: refuses without the variable, with the record list, then more
# lines than a 300-character tail can hold (the real message's last three lines were longer than that).
FAKE_SYNC = r'''#!/usr/bin/env bash
echo "SYNC_OVR=${LIQUIDGOV_NO_ROLLBACK_OK:-unset}" >> "$FAKE_DIR/seen.txt"
echo "Deploying commit abc1234"
if [ -z "${LIQUIDGOV_NO_ROLLBACK_OK:-}" ]; then
  echo "ROLLBACK_GUARD_REFUSED live=1234567: restoring the live program after this deploy would fail to open the store:"
  echo "  fmix: new record type: the live loader throws 'unknown record type'"
  echo "A rollback would first have to strip or rewrite those records (as deploy/city-undo.py does for the city types)."
  echo "Deploy anyway only with Brandon's OK and an undo plan: LIQUIDGOV_NO_ROLLBACK_OK=\"<reason + undo plan>\"."
  echo "Refusing to sync: the no-rollback guard (deploy/rollback_guard.py) did not pass against 1234567. Nothing was sent."
  exit 5
fi
echo "ROLLBACK_GUARD_OVERRIDE: $LIQUIDGOV_NO_ROLLBACK_OK live=1234567"
echo "  fmix: new record type: the live loader throws 'unknown record type'"
echo ">f.st...... assets/a.css"
echo "Sync done."
'''
FAKE_MAKE_GO = r'''import os, sys
name = sys.argv[2]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(root, "deploy", name + "-go.sh"), "w", newline="\n").write("""#!/usr/bin/env bash
echo "GO_$1_OVR=${LIQUIDGOV_NO_ROLLBACK_OK:-unset}" >> "$FAKE_DIR/seen.txt"
if [ "$1" = dry ]; then echo "CI_GATE_OK tree"; echo DRY_OK; echo "DRY_ONLY_DONE: nothing changed."; exit 0; fi
echo BACKUP_DONE; echo GO_DONE; echo CHECK_DONE
echo "RECORD ctx log: python ctx.py log <who> deploy x --result ok --ref abc1234"
echo "RECORD notes.md: - x LIVE (commit abc1234, PID 42, public checks FAILS 0)"
""")
open(os.path.join(root, "deploy", name + "-remote.sh"), "w", newline="\n").write("#!/usr/bin/env bash\n")
print("auto: jbshell pair (assets only)")
'''


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-700:]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    return subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=ENV, check=True).stdout.strip()


def override_file(path, **fields):
    base = {"variable": VAR, "deploy": "thing1", "said": "Yes, rerun with override",
            "reason": "new fmix record", "undo": "stop site, strip fmix, restore old binary"}
    base.update(fields)
    with open(path, "w", encoding="utf-8") as f:
        f.write("# test override\n" + "".join("%s: %s\n" % (k, v) for k, v in base.items() if v is not None))
    return path


tmp = tempfile.mkdtemp(prefix="ship-override-")
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
    sh(work, "git", "add", "."); sh(work, "git", "commit", "-q", "-m", "base")
    sh(work, "git", "remote", "add", "origin", origin); sh(work, "git", "push", "-q", "-u", "origin", "master")
    seen_path = os.path.join(tmp, "seen.txt")

    def seen():
        return open(seen_path, encoding="utf-8").read() if os.path.exists(seen_path) else ""

    def last(prefix):
        rows = [l for l in seen().splitlines() if l.startswith(prefix)]
        return rows[-1][len(prefix):] if rows else "(none)"

    def ship(*args, extra=None):
        env = dict(ENV, SHIP_MAIN=work, SHIP_REPO=work, SHIP_SH=json.dumps([BASH, os.path.join(tmp, "fake_wsl.sh")]),
                   FAKE_DIR=tmp, SHIP_LOG_DIR=os.path.join(tmp, "logs"), SHIP_POLL_SECONDS="0", SHIP_NO_FETCH="1")
        env.update(extra or {})
        p = subprocess.run([sys.executable, SHIP] + list(args), capture_output=True, text=True, env=env, timeout=300)
        return p.returncode, p.stdout + p.stderr

    # ---- O1
    rc, out = ship("deploy", "thing1", "a thing")
    check("O1: deploy with no override is refused", rc == 1 and "SHIP_NOT_READY thing1" in out, out)
    check("O1: the refusal shows the guard's record line in full",
          "fmix: new record type: the live loader throws 'unknown record type'" in out, out)
    check("O1: the refusal names the sync dry run's log", "full log:" in out and "thing1-sync-dry.log" in out, out)

    # ---- O3 (before any good run, so nothing in seen.txt can come from it)
    for case, path in (
            ("another deploy's file", override_file(os.path.join(tmp, "o_other.txt"), deploy="thing2")),
            ("a variable outside the allowed list", override_file(os.path.join(tmp, "o_ci.txt"),
                                                                  variable="LIQUIDGOV_CI_OVERRIDE")),
            ("a missing undo field", override_file(os.path.join(tmp, "o_noundo.txt"), undo=None))):
        before = seen()
        rc, out = ship("deploy", "thing1", "a thing", "--override-file", path)
        check("O3: %s is refused and nothing ran" % case, rc == 1 and "SHIP_NOT_READY" in out and seen() == before, out)

    # ---- O2
    good = override_file(os.path.join(tmp, "override.txt"))
    rc, out = ship("deploy", "thing1", "a thing", "--override-file", good)
    check("O2: deploy --override-file reaches SHIP_DRY_DONE", rc == 0 and "SHIP_DRY_DONE thing1" in out, out)
    check("O2: the sync dry run got the variable with Brandon's quoted words",
          "Brandon said: Yes, rerun with override" in last("SYNC_OVR="), seen())
    check("O2: the go script's dry run got it too", "Brandon said:" in last("GO_dry_OVR="), seen())
    log = os.path.join(tmp, "logs", "thing1-sync-dry.log")
    check("O2: the sync log names the override and its source file",
          os.path.exists(log) and ("OVERRIDE %s for thing1" % VAR) in open(log, encoding="utf-8").read(), log)
    check("O2: the next step printed carries the same override file into go", ("--override-file " + good) in out, out)

    # ---- O5
    rc, out = ship("go", "thing1", "--override-file", good, extra={VAR: "from my shell"})
    check("O5: an override variable in the caller's environment is still refused",
          rc == 1 and VAR in out and "set in this shell" in out, out)

    # ---- O4 (after the pair is merged, as Brandon's "merge" would do)
    bname = sh(origin, "git", "branch", "--list", "wip/*-deploy-thing1").strip().lstrip("* ").strip()
    sh(work, "git", "fetch", "-q", "origin"); sh(work, "git", "merge", "-q", "--ff-only", "origin/" + bname)
    sh(work, "git", "push", "-q", "origin", "master")
    rc, out = ship("go", "thing1", "--override-file", good)
    check("O4: go --override-file is SHIP_LIVE", rc == 0 and "SHIP_LIVE thing1" in out, out)
    check("O4: the go script got the variable", "Brandon said:" in last("GO_go_OVR="), seen())
    rc, out = ship("go", "thing1")
    check("O4: go without the file does not pass it (each run states its own override)",
          last("GO_go_OVR=") == "unset", seen())
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
