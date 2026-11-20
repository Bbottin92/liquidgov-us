"""next_port_fake_check.py: scripts/next_port.py also skips the OTHER ports a suite binds, not only its PORT= line
(2026-11-20: it offered 18152, which outside_wikipedia.sh already uses for its stand-in server, FAKE=18152).
Runs on a throwaway copy (naming.toml, naming_check.py, next_port.py, fake suites); never edits the real repo.
A new file rather than a new case in next_port_check.py (committed checks are kept as they are).
Usage: python tests/repo/next_port_fake_check.py <repo root>   (exit 0 only when every case passed)"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent.parent).resolve()
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


tmp = Path(tempfile.mkdtemp(prefix="next-port-fake-check-"))
try:
    (tmp / "scripts").mkdir()
    (tmp / "tests" / "repo").mkdir(parents=True)
    e2e = tmp / "liquidgov-platform" / "tests" / "e2e"
    e2e.mkdir(parents=True)
    shutil.copy2(ROOT / "scripts" / "next_port.py", tmp / "scripts")
    shutil.copy2(ROOT / "tests" / "repo" / "naming_check.py", tmp / "tests" / "repo")
    text = (ROOT / "naming.toml").read_bytes().decode("utf-8")
    lo = int(text.split("port_min = ")[1].split()[0])
    hi = int(text.split("port_max = ")[1].split()[0])
    check("precondition: the real naming.toml has room for the cases", hi - lo >= 6, (lo, hi))
    (tmp / "naming.toml").write_bytes(text.encode("utf-8"))

    def run():
        p = subprocess.run([sys.executable, str(tmp / "scripts" / "next_port.py")], capture_output=True, text=True, timeout=60)
        return p.returncode, p.stdout.strip(), p.stderr.strip()

    (e2e / "a_one.sh").write_text("PORT=%d; URL=x\n" % lo)
    rc, out, err = run()
    check("control: the port after a plain suite's PORT is offered (lo+1)", rc == 0 and out == str(lo + 1), (rc, out, err))
    # outside_wikipedia.sh's shape: "PORT=18151; FAKE=18152; URL=..."
    (e2e / "b_two.sh").write_text("PORT=%d; FAKE=%d; URL=http://127.0.0.1:$PORT\n" % (lo + 1, lo + 2))
    rc, out, err = run()
    check("a stand-in port on the same line (FAKE=) is skipped too (lo+3)", rc == 0 and out == str(lo + 3), (rc, out, err))
    (e2e / "c_three.sh").write_text("set -u\nPORT=%d\nSTUB_PORT=%d\n" % (lo + 3, lo + 4))
    rc, out, err = run()
    check("a second port on its own line (STUB_PORT=) is skipped too (lo+5)", rc == 0 and out == str(lo + 5), (rc, out, err))
    (e2e / "d_four.sh").write_text("PORT=%d\nCOUNT=%d\n" % (lo + 6, hi + 500))
    rc, out, err = run()
    check("a number outside the port range is not counted as a port (still lo+5)", rc == 0 and out == str(lo + 5), (rc, out, err))
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("next_port_fake_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
