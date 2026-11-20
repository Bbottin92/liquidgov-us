"""backup_retry_check.py: a deploy whose first `go` failed after the backup step can be run again (q56).
The backup step copies the running program to liquidgov_http.pre-<name> and refused outright when that copy already
existed. A first attempt that stopped after the backup (the rollback guard on 2026-11-17, assets1) left the copy, so the
retry was refused and the copy had to be checked and moved aside by hand. Now:
  - no copy yet: backup as before;
  - a copy byte-identical to the running program: it is this deploy's own copy from the earlier attempt (the program
    has not changed since), so backup goes on and says PRE_<NAME>_REUSED;
  - a copy that differs: the program changed after the copy was made (a build already ran, or the copy is from
    something else), so backup still refuses with PRE_<NAME>_ALREADY and changes nothing.
Runs make-go.py in a throwaway copy for the three template pairs (care, jbshell, prebuilt), then runs the generated
remote script's backup phase against a fake /opt/liquidgov in a temp folder (no network, no server).
Usage: python backup_retry_check.py <root>"""
import glob
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
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))
    passes += bool(ok)
    fails += not ok


def fake_opt(base, keep_name, keep_bytes):
    """A fake /opt/liquidgov: store, running program, the two older rollback copies, and maybe a leftover KEEP."""
    opt = os.path.join(base, "opt")
    os.makedirs(os.path.join(opt, "build"))
    open(os.path.join(opt, "liquidgov_accounts.lgp"), "w").write("LGP1\nacct\t1\n")
    binp = os.path.join(opt, "build", "liquidgov_http")
    open(binp, "wb").write(b"running program v1\n")
    os.chmod(binp, 0o755)
    for old in ("pre-flow", "pre-city"):
        open(os.path.join(opt, "liquidgov_http." + old), "wb").write(b"older program\n")
    if keep_bytes is not None:
        open(os.path.join(opt, "liquidgov_http." + keep_name), "wb").write(keep_bytes)
    return opt


tmp = tempfile.mkdtemp(prefix="backup-retry-")
try:
    os.makedirs(os.path.join(tmp, "deploy"))
    shutil.copy2(os.path.join(ROOT, "deploy", "make-go.py"), os.path.join(tmp, "deploy", "make-go.py"))
    shutil.copy2(os.path.join(ROOT, "naming.toml"), os.path.join(tmp, "naming.toml"))
    for pair in ("care", "jbshell", "prebuilt"):
        for half in ("go", "remote"):
            src = os.path.join(ROOT, "deploy", "%s-%s.sh" % (pair, half))
            if os.path.exists(src):
                shutil.copy2(src, os.path.join(tmp, "deploy", "%s-%s.sh" % (pair, half)))
    base_env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and k != "LIQUIDGOV_PREBUILT"}
    for name, f, extra in (("brrestart", "src/http_server.cpp", {}), ("brassets", "assets/lgmap.css", {}),
                           ("brprebuilt", "src/http_server.cpp", {"LIQUIDGOV_PREBUILT": "1"})):
        p = subprocess.run([sys.executable, os.path.join(tmp, "deploy", "make-go.py"), "auto", name, "Backup retry test", f],
                           capture_output=True, text=True, cwd=tmp, env=dict(base_env, **extra), timeout=120)
        check("%s: make-go.py made the pair" % name, p.returncode == 0, p.stdout[-300:] + p.stderr[-300:])
        if p.returncode != 0:
            continue
        remote = open(os.path.join(tmp, "deploy", "%s-remote.sh" % name), encoding="utf-8").read()
        keep = "pre-" + name
        up = name.upper()
        for case, keep_bytes, want_rc, want_line in (
                ("no leftover copy: backup runs", None, 0, "BACKUP_DONE"),
                ("a leftover copy identical to the running program is reused", b"running program v1\n", 0, "PRE_%s_REUSED" % up),
                ("a leftover copy that differs from the running program still refuses", b"some other program\n", 1, "PRE_%s_ALREADY" % up)):
            d = tempfile.mkdtemp(dir=tmp)
            opt = fake_opt(d, keep, keep_bytes)
            script = os.path.join(d, "remote.sh")
            open(script, "w", encoding="utf-8").write(remote.replace("/opt/liquidgov", opt))
            before = sorted(os.listdir(opt))
            r = subprocess.run([BASH, script, "backup"], capture_output=True, text=True, timeout=60)
            lines = r.stdout.split()
            keep_path = os.path.join(opt, "liquidgov_http." + keep)
            backups = glob.glob(os.path.join(opt, "liquidgov_accounts.lgp.pre-%s-*" % name))
            if want_rc == 0:
                ok = (r.returncode == 0 and want_line in lines and "BACKUP_DONE" in lines and len(backups) == 1
                      and open(keep_path, "rb").read() == b"running program v1\n")
            else:
                ok = (r.returncode != 0 and want_line in lines and "BACKUP_DONE" not in lines and not backups
                      and sorted(os.listdir(opt)) == before and open(keep_path, "rb").read() == keep_bytes)
            check("%s: %s" % (name, case), ok, "rc=%d out=%s err=%s" % (r.returncode, r.stdout[-200:], r.stderr[-200:]))
        # the go script's preflight gate, fed what the remote preflight prints for each leftover-copy state
        go = open(os.path.join(tmp, "deploy", "%s-go.sh" % name), encoding="utf-8").read()
        gate = [l for l in go.splitlines() if "PRE_%s_" % up in l and "PREFLIGHT_REFUSED" in l]
        pre = [l for l in remote.splitlines() if '"$KEEP"' in l and "PRE_%s_" % up in l and "echo" in l and "exit" not in l]
        check("%s: precondition: one preflight gate line in the go script, two KEEP lines in remote preflight" % name,
              len(gate) == 1 and len(pre) == 2, (gate, pre))
        if len(gate) != 1 or len(pre) != 2:
            continue
        for case, keep_bytes, want_pass in (("no leftover copy: preflight passes", None, True),
                                            ("leftover copy identical to the running program: preflight passes", b"running program v1\n", True),
                                            ("leftover copy that differs: preflight refuses", b"some other program\n", False)):
            d = tempfile.mkdtemp(dir=tmp)
            opt = fake_opt(d, keep, keep_bytes)
            prelog = os.path.join(d, "prelog")
            body = ('BIN="%s/build/liquidgov_http"; KEEP="%s/liquidgov_http.%s"\n' % (opt, opt, keep)
                    + "\n".join(l.strip() for l in pre) + "\n")
            r1 = subprocess.run([BASH, "-c", body], capture_output=True, text=True, timeout=30)
            open(prelog, "w").write(r1.stdout)
            r2 = subprocess.run([BASH, "-c", 'PRELOG="%s"\n%s\necho GATE_PASSED\n' % (prelog, gate[0])],
                                capture_output=True, text=True, timeout=30)
            passed = r2.returncode == 0 and "GATE_PASSED" in r2.stdout
            check("%s: %s" % (name, case), passed == want_pass and (want_pass or "PREFLIGHT_REFUSED" in r2.stdout),
                  "preflight=%r gate rc=%d out=%r" % (r1.stdout, r2.returncode, r2.stdout))
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
