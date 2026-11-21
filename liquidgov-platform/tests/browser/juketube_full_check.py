"""Juketube's Full button always gives a full view (Brandon 2026-11-21: "i cant fullscreen the juketube", desktop
browser). Full used to call requestFullscreen on the player and do nothing more: no full view when the browser has no
such call (iPhone Safari for a page element), refuses it, or never answers (the desktop app's browser pane left the
request pending). Now real full screen comes first and the player fills the window when that does not happen.
Checked in a real headless Chromium with the REAL assets/juketube.js and juketube.css (and site.css); a stdlib server
here serves them, a stand-in YT.Player (no network), and records POST /log. The page's ?mode= replaces the browser's
requestFullscreen: none (missing), hang (a promise that never settles), refuse (a rejected promise).
  F0 precondition: the widget loads with no page error, the panel opens, and the player is docked in it
  F1 none: one click on Full and the player fills the window at once (box = viewport), the x is shown, Full says Exit
  F2 none: the x closes it, and within a second the player is docked in the panel again
  F3 none: Esc closes it too
  F4 hang: not filled at 0.5 s (real full screen still has its chance), filled by 2 s
  F5 refuse: filled at once
  F6 while filled, the panel's 250 ms docking timer does not move it (still the viewport box 1 s later)
  F7 the reason reaches /log (ev=jb_full_fallback, d names it), so the next real sighting says what the browser did
Usage: LG_BROWSER=$(command -v google-chrome) python3 juketube_full_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT juketube-full: ALL-PASS" or the failed cases. Exit 1 on a FAIL."""
import http.server
import os
import sys
import threading
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
if os.name != "nt" and not os.environ.get("LG_BROWSER"):
    for _c in ("/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser"):
        if os.path.exists(_c):
            os.environ["LG_BROWSER"] = _c
            break
import cdp  # noqa: E402

PLATFORM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", ".."))
OUT = sys.argv[2] if len(sys.argv) > 2 else ""  # optional: where to save a screenshot of the filled player
W, H = 1280, 800
LOGS = []
results = []

MODES = {
    "none": "delete Element.prototype.requestFullscreen; delete Element.prototype.webkitRequestFullscreen;",
    "hang": "Element.prototype.requestFullscreen = function () { return new Promise(function () {}); };",
    "refuse": "Element.prototype.requestFullscreen = function () {"
              " return Promise.reject(new TypeError('Permissions check failed')); };",
}
# a stand-in YouTube player: the real one needs the network; every method the widget calls is a no-op
YT_STUB = """
window.__errors = [];
window.addEventListener('error', function (e) { window.__errors.push(String(e.message)); });
window.YT = {PlayerState: {UNSTARTED: -1, ENDED: 0, PLAYING: 1, PAUSED: 2, BUFFERING: 3, CUED: 5},
  Player: function (id, opts) {
    var el = document.getElementById(id), f = document.createElement('iframe');
    f.id = id; if (el) el.replaceWith(f);
    var me = new Proxy({}, {get: function (t, k) { return k in t ? t[k] : function () { return 0; }; }});
    setTimeout(function () { try { opts.events.onReady({target: me}); } catch (e) {} }, 0);
    return me;
  }};
"""


def page(mode):
    return ("<!doctype html><html><head><meta charset=utf-8>"
            "<link rel=stylesheet href=/assets/site.css><link rel=stylesheet href=/assets/juketube.css></head>"
            "<body><main class=wrap><h1>Feed</h1><p>page</p></main><div id=juketube-root></div>"
            "<script>" + YT_STUB + MODES.get(mode, "") + "</script>"
            "<script src=/assets/juketube.js></script></body></html>")


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        if u.path == "/":
            mode = urllib.parse.parse_qs(u.query).get("mode", [""])[0]
            return self.send(200, "text/html; charset=utf-8", page(mode).encode())
        if u.path in ("/assets/juketube.js", "/assets/juketube.css", "/assets/site.css"):
            with open(os.path.join(PLATFORM, u.path.lstrip("/")), "rb") as f:
                body = f.read()
            return self.send(200, "text/css" if u.path.endswith(".css") else "application/javascript", body)
        return self.send(404, "text/plain", b"no")

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n).decode("utf-8", "replace")
        if self.path == "/log":
            LOGS.append(urllib.parse.parse_qs(body))
        return self.send(204, "text/plain", b"")


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))


def click(ws, sel):
    box = cdp.js(ws, "(function(){var r=document.querySelector(%r).getBoundingClientRect();"
                     "return [r.left+r.width/2, r.top+r.height/2, r.width];})()" % sel)
    if not isinstance(box, list) or box[2] < 1:
        return False
    for t in ("mousePressed", "mouseReleased"):
        ws.call("Input.dispatchMouseEvent", type=t, x=box[0], y=box[1], button="left", clickCount=1)
    return True


STATE = ("(function(){var w=document.getElementById('jbtube-wrap'),r=w.getBoundingClientRect(),"
         "c=document.getElementById('jbfsclose');return {full:w.classList.contains('jb-full'),"
         "docked:w.classList.contains('jb-docked'),box:[Math.round(r.left),Math.round(r.top),Math.round(r.width),"
         "Math.round(r.height)],close:!!(c&&!c.hidden&&c.getBoundingClientRect().width>0),"
         "label:document.getElementById('jbfs').textContent,errors:window.__errors};})()")


def fills(s):
    return s and s["full"] and s["box"] == [0, 0, W, H]


def load(ws, port, mode):
    ws.call("Page.navigate", url="http://127.0.0.1:%d/?mode=%s" % (port, mode))
    for _ in range(40):
        if cdp.js(ws, "!!document.getElementById('jbfs') && document.readyState === 'complete'"):
            break
        time.sleep(0.25)
    click(ws, "#jbtoggle")
    time.sleep(0.6)


def main():
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    port = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    proc, ws = cdp.launch_edge(prefix="lg-jbfull-")
    try:
        ws.call("Emulation.setDeviceMetricsOverride", width=W, height=H, deviceScaleFactor=1, mobile=False)
        ws.call("Runtime.enable")
        load(ws, port, "none")
        s = cdp.js(ws, STATE)
        check("F0 precondition: the widget loads with no page error and the player is docked in the open panel",
              isinstance(s, dict) and not s["errors"] and s["docked"] and s["box"][2] > 50, s)
        click(ws, "#jbfs")
        time.sleep(0.3)
        s = cdp.js(ws, STATE)
        check("F1 none: one click on Full fills the window at once, with the x shown and Full saying Exit",
              fills(s) and s["close"] and s["label"] == "Exit", s)
        if OUT:  # a picture of the filled player, to look at (the stand-in player is a blank frame)
            import base64
            os.makedirs(OUT, exist_ok=True)
            shot = ws.call("Page.captureScreenshot", format="png")["data"]
            with open(os.path.join(OUT, "juketube-full.png"), "wb") as f:
                f.write(base64.b64decode(shot))
        time.sleep(1.0)
        s = cdp.js(ws, STATE)
        check("F6 while filled, the docking timer does not move it (1 s later)", fills(s), s)
        click(ws, "#jbfsclose")
        time.sleep(1.0)
        s = cdp.js(ws, STATE)
        check("F2 none: the x closes it and the player is docked in the panel again",
              s and not s["full"] and s["docked"] and s["box"][2] < W and s["label"] == "Full", s)
        click(ws, "#jbfs")
        time.sleep(0.3)
        for t in ("keyDown", "keyUp"):
            ws.call("Input.dispatchKeyEvent", type=t, key="Escape", code="Escape", windowsVirtualKeyCode=27)
        time.sleep(0.5)
        s = cdp.js(ws, STATE)
        check("F3 none: Esc closes it", s and not s["full"], s)
        reasons = [l.get("d", [""])[0] for l in LOGS if l.get("ev", [""])[0] == "jb_full_fallback"]
        check("F7 the reason reached /log (ev=jb_full_fallback, d names it)",
              any("no fullscreen api" in r for r in reasons), LOGS)

        load(ws, port, "hang")
        click(ws, "#jbfs")
        time.sleep(0.5)
        early = cdp.js(ws, STATE)
        time.sleep(1.5)
        late = cdp.js(ws, STATE)
        check("F4 hang: not filled at 0.5 s, filled by 2 s", early and not early["full"] and fills(late), (early, late))

        load(ws, port, "refuse")
        click(ws, "#jbfs")
        time.sleep(0.3)
        s = cdp.js(ws, STATE)
        check("F5 refuse: filled at once", fills(s), s)
    finally:
        cdp.stop_edge(proc, getattr(ws, "profile", None))
        srv.shutdown()
    ok = all(results)
    print("VERDICT juketube-full: %s" % ("ALL-PASS" if ok else "FAIL (%d of %d)" % (results.count(False), len(results))))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
