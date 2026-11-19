"""next_port_check.py: scripts/next_port.py finds the next free e2e port and widens a full range safely (2026-11-08).
Runs on a throwaway copy (naming.toml, naming_check.py, next_port.py, fake e2e scripts); never edits the real repo.
Usage: python tests/repo/next_port_check.py <repo root>   (exit 0 only when every case passed)"""
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


tmp = Path(tempfile.mkdtemp(prefix="next-port-check-"))
try:
    (tmp / "scripts").mkdir()
    (tmp / "tests" / "repo").mkdir(parents=True)
    e2e = tmp / "liquidgov-platform" / "tests" / "e2e"
    e2e.mkdir(parents=True)
    shutil.copy2(ROOT / "scripts" / "next_port.py", tmp / "scripts")
    shutil.copy2(ROOT / "tests" / "repo" / "naming_check.py", tmp / "tests" / "repo")
    text = (ROOT / "naming.toml").read_bytes().decode("utf-8")  # raw: line endings are part of what is checked
    lo = int(text.split("port_min = ")[1].split()[0])
    hi = int(text.split("port_max = ")[1].split()[0])
    check("precondition: the real naming.toml has a port range", lo < hi, (lo, hi))
    (tmp / "naming.toml").write_bytes(text.encode("utf-8"))

    def run(*a):
        p = subprocess.run([sys.executable, str(tmp / "scripts" / "next_port.py")] + list(a), capture_output=True, text=True,
                           timeout=60)
        return p.returncode, p.stdout.strip(), p.stderr.strip()

    for port in (lo, lo + 1, lo + 3):
        (e2e / ("s_%d.sh" % port)).write_text("PORT=%d; URL=x\n" % port)
    rc, out, err = run()
    check("the first gap is offered (lo+2)", rc == 0 and out == str(lo + 2), (rc, out, err))
    # 2026-11-19: it offered 18143 while tests/rollback/portraits_undo.sh already used it (only e2e/ was read)
    rb = tmp / "liquidgov-platform" / "tests" / "rollback"
    rb.mkdir()
    (rb / "r.sh").write_text("PORT=%d; URL=x\n" % (lo + 2))
    rc, out, err = run()
    check("a port a suite outside e2e/ uses (tests/rollback) is skipped (lo+4)", rc == 0 and out == str(lo + 4), (rc, out, err))
    for port in range(lo, hi + 1):
        (e2e / ("s_%d.sh" % port)).write_text("PORT=%d; URL=x\n" % port)
    before = (tmp / "naming.toml").read_bytes().decode("utf-8")
    rc, out, err = run()
    check("a full range exits 3 and changes nothing", rc == 3 and (tmp / "naming.toml").read_bytes().decode("utf-8") == before, (rc, err))
    rc, out, err = run("--widen")
    after = (tmp / "naming.toml").read_bytes().decode("utf-8")
    check("--widen keeps the file's line endings byte for byte", after.count("\r\n") == before.count("\r\n"),
          (before.count("\r\n"), after.count("\r\n")))
    check("--widen offers hi+1", rc == 0 and out.splitlines()[0] == str(hi + 1), (rc, out, err))
    check("--widen changed exactly the port_max line", after == before.replace("port_max = %d" % hi, "port_max = %d" % (hi + 20))
          and after != before, after[:300])
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("next_port_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
