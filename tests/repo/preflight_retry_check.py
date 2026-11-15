"""preflight_retry_check.py: a generated *-go.sh retries its read-only preflight ONCE when WSL drops the ssh call
with "accept4 failed" (vsock 110), as it already does for rsync. On 2026-11-10 the first polish1 'go' died right
there, before any change on the server, and had to be started again by hand (Brandon: "yes add the preflight retry").
Any other preflight failure, or a second accept4, still stops the deploy.
Runs make-go.py in a throwaway copy for both template pairs (build+restart and no-restart), then runs only the
generated preflight block with a fake `remote` (no network). Usage: python preflight_retry_check.py <root>"""
import os
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"  # PATH's bash, never System32's WSL launcher (lesson 2026-11-08)
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


def preflight_block(go):
    """From the PRELOG line through the PREFLIGHT_DONE check: the whole preflight step."""
    start = go.find('PRELOG="')
    end = go.find("'^PREFLIGHT_DONE$'", start)
    if start < 0 or end < 0:
        return None
    return go[start:go.find("\n", end) + 1]


# fake remote: FAILS counts down; each failing call prints MODE's error to stderr and exits 1
FAKE = r'''
set -euo pipefail
TMPDIR="%s"
FAILS=%d
remote() {
  # counted in a file: in 'remote ... | tee' the function runs in a subshell, so a shell variable would reset
  echo call >> "$TMPDIR/calls"; n=$(wc -l < "$TMPDIR/calls")
  if [ "$n" -le "$FAILS" ]; then echo "%s" >&2; return 1; fi
  printf 'ACTIVE=active\nPID=4242\nPRE_%s_ABSENT\nPRE_FLOW_PRESENT\nPRE_CITY_PRESENT\nPREFLIGHT_DONE\n'
}
'''
WSL_ERR = "<3>WSL (433 - ) ERROR: UtilAcceptVsock:281: accept4 failed 110"
OTHER_ERR = "ssh: connect to host 143.198.159.58 port 22: Connection refused"

tmp = tempfile.mkdtemp(prefix="preflight-retry-")
try:
    os.makedirs(os.path.join(tmp, "deploy"))
    shutil.copy2(os.path.join(ROOT, "deploy", "make-go.py"), os.path.join(tmp, "deploy", "make-go.py"))
    shutil.copy2(os.path.join(ROOT, "naming.toml"), os.path.join(tmp, "naming.toml"))
    for pair in ("care", "jbshell"):
        for half in ("go", "remote"):
            shutil.copy2(os.path.join(ROOT, "deploy", "%s-%s.sh" % (pair, half)), os.path.join(tmp, "deploy", "%s-%s.sh" % (pair, half)))
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    for name, f in (("pfrestart", "src/http_server.cpp"), ("pfassets", "assets/lgmap.css")):
        p = subprocess.run([sys.executable, os.path.join(tmp, "deploy", "make-go.py"), "auto", name, "Preflight test", f],
                           capture_output=True, text=True, cwd=tmp, env=env, timeout=120)
        check("%s: make-go.py made the pair" % name, p.returncode == 0, p.stdout[-300:] + p.stderr[-300:])
        if p.returncode != 0:
            continue
        block = preflight_block(open(os.path.join(tmp, "deploy", "%s-go.sh" % name), encoding="utf-8").read())
        check("%s: precondition: found the preflight block" % name, block is not None)
        if block is None:
            continue
        for case, nfail, err, want_rc, want_calls in (
                ("a healthy preflight passes first time", 0, WSL_ERR, 0, 1),
                ("one WSL accept4 drop is retried and passes", 1, WSL_ERR, 0, 2),
                ("two accept4 drops in a row stop the deploy", 2, WSL_ERR, 1, 2),
                ("any other ssh failure stops at once (no retry)", 1, OTHER_ERR, 1, 1)):
            d = tempfile.mkdtemp(dir=tmp).replace("\\", "/")
            script = FAKE % (d, nfail, err, name.upper()) + block + "echo PREFLIGHT_STEP_OK\n"
            r = subprocess.run([BASH, "-c", script], capture_output=True, text=True, timeout=60)
            calls = len(open(os.path.join(d, "calls")).read().split("\n")) - 1 if os.path.exists(os.path.join(d, "calls")) else 0
            ok = (r.returncode == 0) == (want_rc == 0) and calls == want_calls \
                and (("PREFLIGHT_STEP_OK" in r.stdout) == (want_rc == 0))
            check("%s: %s" % (name, case), ok, "rc=%d calls=%d out=%s err=%s" % (r.returncode, calls, r.stdout[-150:], r.stderr[-150:]))
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
