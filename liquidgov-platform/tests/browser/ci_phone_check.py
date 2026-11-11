"""The phone browser check on Linux (GitHub Actions; Brandon 2026-11-11 'ci': screenshots must stop needing the laptop's
RAM). Same steps as run_phone_check.py without the WSL hops: start phone_serve.sh (throwaway store, port 18140), read its
READY line, take Bbottin's session, run phone_check.run() against the lgmap.js on disk, stop the server.
Usage: LG_BROWSER=$(command -v google-chrome) python3 ci_phone_check.py <build dir> <liquidgov-platform dir> <label> <out dir>
Exit 0 only when every check passes; screenshots and phone-<label>.json land in <out dir> either way."""
import os
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import phone_check  # noqa: E402


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 2
    build, platform_dir, label, out = argv
    os.makedirs(out, exist_ok=True)
    browser = os.environ.get("LG_BROWSER", "")
    if not browser or not os.path.exists(browser):
        print("RUN_FAIL set LG_BROWSER to a Chromium binary (got %r)" % browser)
        return 2
    server = subprocess.Popen(["bash", os.path.join(HERE, "phone_serve.sh"), os.path.join(build, "liquidgov_http"),
                               os.path.join(build, "liquidgov_e2e_seed"), platform_dir],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, start_new_session=True)
    try:
        ready, deadline = None, time.time() + 120
        while time.time() < deadline:
            line = server.stdout.readline()
            if not line:
                if server.poll() is not None:
                    break
                continue
            print("serve: " + line.rstrip(), flush=True)
            if line.startswith("READY "):
                parts = line.split()
                ready = {"url": parts[1], "work": parts[2]}
                break
        if not ready:
            print("RUN_FAIL phone_serve.sh never printed READY (exit %s)" % server.poll())
            return 1
        sid = None
        with open(os.path.join(ready["work"], "sessions.txt"), encoding="utf-8") as f:
            for row in f:
                p = row.split()
                if len(p) >= 2 and p[0] == "Bbottin":
                    sid = p[1]
        if not sid:
            print("RUN_FAIL no Bbottin session in %s/sessions.txt" % ready["work"])
            return 1
        fails = phone_check.run(ready["url"], sid, os.path.join(platform_dir, "assets", "lgmap.js"), label, out)
        return 0 if not fails else 1
    finally:
        try:
            os.killpg(server.pid, signal.SIGTERM)
            server.wait(timeout=15)
        except (OSError, subprocess.SubprocessError) as e:
            print("ci_phone_check: could not stop the test server %d: %s" % (server.pid, e), file=sys.stderr)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
