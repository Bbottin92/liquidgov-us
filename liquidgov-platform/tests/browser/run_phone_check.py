"""Run the phone browser check end to end from Windows: start the WSL test server, run phone_check.py, stop the server.

Usage (Windows Python, from anywhere):
  python liquidgov-platform/tests/browser/run_phone_check.py <WSL build dir> [--assets <liquidgov-platform dir>]
         [--label new] [--out <dir>]
  <WSL build dir> holds liquidgov_http and liquidgov_e2e_seed, e.g. '~/lg-sweep-check' (quoted; ~ is expanded in WSL),
  as scripts/lgcheck.sh builds it. --assets serves the page files of another checkout (a Windows path), for the old-build comparison:
  a new case must FAIL there. Exit 0 all pass, 1 a check failed, 2 the run could not start.

Run it before and after any change to assets/lgmap.js, assets/lgmap.css or the map page. It needs Edge, so CI does
not run it (2026-11-09: CI and the e2e suites only see HTML, not how the map reacts to a finger).

The server's whole lifecycle is here on purpose (2026-11-09 lessons): stopping WSL test servers by hand failed three
times (an inline awk lost its quotes twice; pkill -f killed its own shell and left the server running). This stops
it by the PID file the server writes, then proves the port is closed. Free RAM is checked first (>= 2.5 GB, the
resource guard's line) instead of being polled by hand. The served script's CRs are stripped before WSL runs it.
"""
import argparse
import ctypes
import os
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import phone_check  # noqa: E402

PORT = 18140  # phone_serve.sh's port
MIN_FREE_GB = 2.5


def wsl_path(p):
    p = os.path.abspath(p)
    return "/mnt/%s%s" % (p[0].lower(), p[2:].replace("\\", "/"))


def free_gb():
    class MS(ctypes.Structure):
        _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong), ("ullTotalPhys", ctypes.c_ulonglong),
                    ("ullAvailPhys", ctypes.c_ulonglong), ("ullTotalPageFile", ctypes.c_ulonglong),
                    ("ullAvailPageFile", ctypes.c_ulonglong), ("ullTotalVirtual", ctypes.c_ulonglong),
                    ("ullAvailVirtual", ctypes.c_ulonglong), ("ullAvailExtendedVirtual", ctypes.c_ulonglong)]
    m = MS()
    m.dwLength = ctypes.sizeof(MS)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m)):
        return None
    return m.ullAvailPhys / 2 ** 30


def answers(url):
    try:
        urllib.request.urlopen(url, timeout=2)
        return True
    except urllib.error.HTTPError:
        return True  # it answered, just not 200
    except OSError:
        return False  # err-ok: nothing listening


def wsl(cmd, timeout=30):
    return subprocess.run(["wsl", "bash", "-c", cmd], capture_output=True, text=True, timeout=timeout)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("build", help="WSL dir with liquidgov_http and liquidgov_e2e_seed")
    ap.add_argument("--assets", default=os.path.dirname(os.path.dirname(HERE)), help="liquidgov-platform dir to serve")
    ap.add_argument("--label", default="new")
    ap.add_argument("--out", default=os.path.join(tempfile.gettempdir(), "lg-phone-check"))
    a = ap.parse_args(argv)
    url = "http://127.0.0.1:%d" % PORT
    js_file = os.path.join(a.assets, "assets", "lgmap.js")
    if not os.path.isfile(js_file):
        print("RUN_FAIL no assets/lgmap.js under %s" % a.assets)
        return 2
    g = free_gb()
    if g is None or g < MIN_FREE_GB:
        print("RUN_FAIL free RAM %s GB < %.1f GB: close something (or wsl --shutdown) and rerun" % (
            "unknown" if g is None else "%.2f" % g, MIN_FREE_GB))
        return 2
    if answers(url + "/"):
        print("RUN_FAIL port %d already answers: a leftover server? refusing" % PORT)
        return 2

    # LF copy of the server script (a Windows checkout may hold CRLF), run by WSL bash
    tmp = tempfile.mkdtemp(prefix="lg-phone-serve-")
    serve = os.path.join(tmp, "phone_serve.sh")
    with open(os.path.join(HERE, "phone_serve.sh"), "rb") as f:
        body = f.read().replace(b"\r\n", b"\n")
    with open(serve, "wb") as f:
        f.write(body)
    b = a.build.rstrip("/")
    # Git Bash rewrites a /home/... argument to C:/Program Files/Git/home/... before Python sees it (2026-11-09).
    if ":" in b or "\\" in b:
        print("RUN_FAIL build dir %r is not a WSL path (Git Bash rewrites /home/...): pass '~/<dir>' in quotes, "
              "or set MSYS_NO_PATHCONV=1" % a.build)
        return 2
    if b.startswith("~/"):
        home = wsl("echo $HOME").stdout.strip()
        if not home.startswith("/"):
            print("RUN_FAIL could not read the WSL home directory")
            return 2
        b = home + b[1:]
    proc = subprocess.Popen(["wsl", "bash", wsl_path(serve), b + "/liquidgov_http", b + "/liquidgov_e2e_seed", wsl_path(a.assets)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
    lines, ready = [], {}

    def reader():
        for line in proc.stdout:
            line = line.rstrip("\n")
            lines.append(line)
            print("serve: " + line, flush=True)
            if line.startswith("READY "):
                parts = line.split()
                ready["url"], ready["work"] = parts[1], parts[2]
    threading.Thread(target=reader, daemon=True).start()

    work = "/tmp/lg-phone-check"  # phone_serve.sh's work dir; READY repeats it

    def body():
        for _ in range(240):  # up to 120 s for seed + sample data
            if ready or proc.poll() is not None:
                break
            time.sleep(0.5)
        if not ready:
            print("RUN_FAIL the test server did not get ready (exit %s); its output is above" % proc.poll())
            return 2
        sess = wsl("cat %s/sessions.txt" % ready["work"])
        sid = dict(l.split()[:2] for l in sess.stdout.splitlines() if len(l.split()) >= 2).get("Bbottin")
        if not sid:
            print("RUN_FAIL no Bbottin session in %s/sessions.txt (%s)" % (ready["work"], sess.stderr.strip()))
            return 2
        print("RUN server %s, assets %s, build %s, out %s" % (url, a.assets, b, a.out), flush=True)
        return 0 if not phone_check.run(url, sid, js_file, a.label, a.out) else 1

    rc = 2
    try:
        rc = body()
    finally:
        pid = wsl("cat %s/server.pid 2>/dev/null" % work).stdout.strip()
        if pid.isdigit():
            wsl("kill %s" % pid)
        try:
            proc.terminate()
            proc.wait(timeout=15)
        except (OSError, subprocess.SubprocessError) as e:
            print("RUN_WARN could not stop the wsl launcher: %s" % e)
        for _ in range(20):
            if not answers(url + "/"):
                break
            time.sleep(0.5)
        if answers(url + "/"):
            print("RUN_FAIL the test server still answers on %d after stopping (pid %s): stop it by hand" % (PORT, pid or "?"))
            rc = 2
        else:
            print("RUN server stopped (pid %s), port %d closed" % (pid or "?", PORT))
        print("RUN_EXIT=%d" % rc)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
