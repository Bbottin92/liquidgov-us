"""The "Alerts blocked in iPhone Settings" strip shows only on an iPhone/iPad (Brandon 2026-11-11 4:37 PM, Brief 4 item
10; site-look2 FINDINGS: it appeared in desktop Chrome). Real headless Chromium with the real assets/push.js on a small
stdlib-served page; NO C++ server. Notification.permission is pinned before the page's scripts run.

  P1  desktop Chrome UA, alerts denied: no strip saying iPhone (no strip at all)
  P2  Android Chrome UA, alerts denied: no iPhone strip
  P3  iPhone UA (a Home Screen app has push), alerts denied: the strip "Alerts blocked in iPhone Settings" shows
  P4  iPadOS UA (reports as a Mac with touch), alerts denied: the iPhone strip shows
  P5  desktop, a "Turn on alerts" tap that the person denies: no iPhone strip afterwards
  P6  control: desktop, permission not yet asked: "Turn on alerts" still shows
Usage: LG_BROWSER=$(command -v google-chrome) python3 push_banner_check.py [<liquidgov-platform dir>]
Prints "VERDICT push-banner: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed checks unchanged."""
import http.server
import json
import os
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
if os.name != "nt" and not os.environ.get("LG_BROWSER"):
    for _c in ("/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser"):
        if os.path.exists(_c):
            os.environ["LG_BROWSER"] = _c
            break
import cdp  # noqa: E402

UA = {
    "desktop": ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36",
                "Win32", 0),
    "android": ("Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Mobile Safari/537.36",
                "Linux armv8l", 5),
    "iphone": ("Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Mobile/15E148",
               "iPhone", 5),
    "ipad": ("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Safari/605.1.15",
             "MacIntel", 5),
}


def make_handler(platform):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            path = self.path.split("?")[0]
            if path == "/assets/push.js":
                body, ctype = open(os.path.join(platform, "assets", "push.js"), "rb").read(), "application/javascript; charset=utf-8"
            elif path == "/sw.js":
                body, ctype = b"", "application/javascript; charset=utf-8"
            else:
                body, ctype = b"<!doctype html><html><body><main><h1>page</h1></main><script src=/assets/push.js></script></body></html>", "text/html"
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

    return H


# pins Notification.permission (and what requestPermission answers) before push.js runs
PIN = """(() => { const st = {p: %s, ask: %s};
  Object.defineProperty(Notification, 'permission', {get: () => st.p, configurable: true});
  Notification.requestPermission = () => { st.p = st.ask; return Promise.resolve(st.ask); }; })();"""


class Run:
    def __init__(self, base):
        self.base, self.fails, self.n, self.script = base, [], 0, None

    def check(self, name, ok, detail):
        self.n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:400]), flush=True)
        if not ok:
            self.fails.append(name)

    def strip(self):
        v = cdp.js(self.ws, "(() => { const e = document.getElementById('lgpushask'); return e ? e.textContent : ''; })()")
        return v or ""

    def open(self, who, perm, ask="denied"):
        ua, plat, touch = UA[who]
        self.ws.call("Emulation.setUserAgentOverride", userAgent=ua, platform=plat)
        self.ws.call("Emulation.setTouchEmulationEnabled", enabled=touch > 0, maxTouchPoints=max(touch, 1))
        if self.script:
            self.ws.call("Page.removeScriptToEvaluateOnNewDocument", identifier=self.script)
        self.script = self.ws.call("Page.addScriptToEvaluateOnNewDocument",
                                   source=PIN % (json.dumps(perm), json.dumps(ask)))["identifier"]
        self.ws.call("Page.navigate", url=self.base + "/?" + who + perm)
        time.sleep(1.2)
        return self.strip()

    def cases(self):
        s = self.open("desktop", "denied")
        self.check("P1 desktop, denied: no iPhone strip", "iPhone" not in s, s)
        s = self.open("android", "denied")
        self.check("P2 Android, denied: no iPhone strip", "iPhone" not in s, s)
        s = self.open("iphone", "denied")
        self.check("P3 iPhone, denied: the iPhone Settings strip shows", "Alerts blocked in iPhone Settings" in s, s)
        s = self.open("ipad", "denied")
        self.check("P4 iPad (Mac UA with touch), denied: the iPhone Settings strip shows", "Alerts blocked in iPhone Settings" in s, s)
        s = self.open("desktop", "default", "denied")
        self.check("P6 control: desktop, not asked yet: Turn on alerts shows", "Turn on alerts" in s, s)
        cdp.js(self.ws, "(() => { const b = document.querySelector('#lgpushask button'); if (b) b.click(); return 1; })()")
        time.sleep(0.6)
        s = self.strip()
        self.check("P5 desktop, a Turn on alerts tap that is denied: no iPhone strip", "iPhone" not in s, s)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(platform))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1])
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-push-banner-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT push-banner: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
