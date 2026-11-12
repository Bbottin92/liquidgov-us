"""/me saves itself: no "Publish revision" bar (Brandon 2026-11-11 4:50 PM, Brief 4 item 11). Each form[data-draft]
saves on change, debounced about 800 ms, to its own endpoint with fetch (no page load, so no jump to the top), with a
small inline status: "Saving…", "Saved", or "Couldn't save" with a Retry button.

Real headless Chromium, NO C++ server: a stdlib server here serves a page built from the real code (the GET /me
handler's markup is checked for the bar; the draft script is draft_publish_script()'s literal text, cut from
src/http_server.cpp) around a form[data-draft] like Profile CSS, and a fake POST /me/css that answers like the real one
(303 to /me?ok=css, or /me?err=css on a refused value; 500 when told to fail).

  A1  the GET /me handler no longer renders #medraftbar, #mepublish ("Publish revision") or #mediscard
  A2  typing a burst of characters sends ONE save, with the new value, 0.5-2.5 s after the last key
  A3  the inline status by the form reads Saving… while it is sent, then Saved
  A4  no page load and no scroll: a marker on window survives and scrollY is unchanged
  A5  a refused value (server redirects with err=) shows "Couldn't save" and a Retry button
  A5b Retry sends again; once the server accepts, Saved
  A6  a server error (500) shows "Couldn't save" too
Usage: LG_BROWSER=$(command -v google-chrome) python3 me_autosave_check.py [<liquidgov-platform dir>]
Prints "VERDICT me-autosave: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed checks unchanged."""
import http.server
import json
import os
import re
import sys
import threading
import time
import traceback
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
if os.name != "nt" and not os.environ.get("LG_BROWSER"):
    for _c in ("/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser"):
        if os.path.exists(_c):
            os.environ["LG_BROWSER"] = _c
            break
import cdp  # noqa: E402


def literals(src, start, end_marker="\n}\n"):
    a = src.index(start)
    b = src.index(end_marker, a)
    return "".join(p.encode().decode("unicode_escape") for p in re.findall(r'"((?:[^"\\]|\\.)*)"', src[a:b]))


def me_handler_markup(src):
    a = src.index('if(method=="GET"&&path=="/me")')
    b = src.index('return pg("Profile"', a)
    return "".join(p.encode().decode("unicode_escape") for p in re.findall(r'"((?:[^"\\]|\\.)*)"', src[a:b]))


STATE = {"mode": "ok", "posts": []}


def make_handler(script):
    page = ("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width'></head>"
            "<body style='font-family:sans-serif'><div style='height:1500px'>top of /me</div>"
            "<div class=card><h2>Profile CSS</h2><form method=POST action=/me/css data-draft=1>"
            "<textarea id=css name=css rows=8></textarea></form></div><div style='height:1500px'>below</div>"
            "%s</body></html>" % script)

    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def send(self, code, body=b"", loc=None):
            self.send_response(code)
            if loc:
                self.send_header("Location", loc)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            STATE.setdefault("gets", []).append(self.path)
            return self.send(200, page.encode())

        def do_POST(self):
            n = int(self.headers.get("Content-Length") or 0)
            body = urllib.parse.parse_qs(self.rfile.read(n).decode())
            STATE["posts"].append({"t": time.time(), "path": self.path, "css": body.get("css", [""])[0]})
            if STATE["mode"] == "500":
                return self.send(500, b"no")
            if STATE["mode"] == "refuse":
                return self.send(303, loc="/me?err=css&reason=import")
            return self.send(303, loc="/me?ok=css")

    return H


STATUS = r"""(() => { const f = document.querySelector('form[data-draft]'); const c = f.closest('.card') || f.parentNode;
  const s = c.querySelector('[data-draft-status]'); const r = c.querySelector('[data-draft-retry]');
  return {status: s ? s.textContent.trim() : null, retry: !!(r && !r.hidden && r.getBoundingClientRect().height > 0),
          marker: window.__marker === 1, y: Math.round(scrollY), bar: !!document.getElementById('medraftbar')}; })()"""


class Run:
    def __init__(self, base):
        self.base, self.fails, self.n = base, [], 0

    def check(self, name, ok, detail):
        self.n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:500]), flush=True)
        if not ok:
            self.fails.append(name)

    def j(self, expr):
        v = cdp.js(self.ws, "JSON.stringify(%s)" % expr)
        try:
            return json.loads(v)
        except (TypeError, ValueError):  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def type(self, text):
        cdp.js(self.ws, "document.getElementById('css').focus({preventScroll: true}), 1")
        for ch in text:
            self.ws.call("Input.insertText", text=ch)
            time.sleep(0.06)

    def watch(self, secs):
        """Status texts seen over secs (sampled), and the last state."""
        seen, s = [], None
        for _ in range(int(secs / 0.05)):
            s = self.j(STATUS)
            if isinstance(s, dict) and s.get("status") and (not seen or seen[-1] != s["status"]):
                seen.append(s["status"])
            time.sleep(0.05)
        return seen, s

    def cases(self, markup):
        bad = [k for k in ("medraftbar", "mepublish", "mediscard", "Publish revision") if k in markup]
        self.check("A1 the GET /me handler renders no Publish/Discard bar", bad == [], bad)
        self.ws.call("Emulation.setDeviceMetricsOverride", width=390, height=844, deviceScaleFactor=2, mobile=True)
        self.ws.call("Page.navigate", url=self.base + "/me")
        time.sleep(1.5)
        cdp.js(self.ws, "window.__marker = 1; document.querySelector('form[data-draft]').scrollIntoView({block: 'center'}); 1")
        time.sleep(0.3)
        y0 = self.j("Math.round(scrollY)")
        STATE["mode"], STATE["posts"] = "ok", []
        t_last = time.time()
        self.type("h1{}")
        t_last = time.time()
        seen, s = self.watch(3.0)
        posts = list(STATE["posts"])
        dt = posts[0]["t"] - t_last if posts else None
        self.check("A2 a burst of typing sends ONE save, with the new value, 0.5-2.5 s after the last key",
                   len(posts) == 1 and posts[0]["path"] == "/me/css" and posts[0]["css"] == "h1{}" and dt is not None
                   and 0.5 <= dt <= 2.5, {"posts": posts, "dt": dt})
        self.check("A3 the inline status shows Saving… and then Saved", "Saved" in seen and
                   any(x.startswith("Saving") for x in seen) and (s or {}).get("status") == "Saved", {"seen": seen, "s": s})
        self.check("A4 no page load and no scroll", isinstance(s, dict) and s.get("marker") and abs(s.get("y", -999) - y0) <= 2,
                   {"s": s, "y0": y0})
        STATE["mode"], STATE["posts"] = "refuse", []
        self.type("@import")
        seen, s = self.watch(3.0)
        self.check("A5 a refused value shows Couldn't save with a Retry button", isinstance(s, dict)
                   and (s.get("status") or "").startswith("Couldn") and s.get("retry"), {"seen": seen, "s": s})
        STATE["mode"], STATE["posts"] = "ok", []
        cdp.js(self.ws, "(() => { const r = document.querySelector('[data-draft-retry]'); if (r) r.click(); return 1; })()")
        seen, s = self.watch(2.0)
        self.check("A5b Retry sends again; once accepted, Saved", len(STATE["posts"]) == 1 and (s or {}).get("status") == "Saved"
                   and not (s or {}).get("retry"), {"posts": STATE["posts"], "seen": seen, "s": s})
        STATE["mode"], STATE["posts"] = "500", []
        self.type("x")
        seen, s = self.watch(3.0)
        self.check("A6 a server error shows Couldn't save", isinstance(s, dict) and (s.get("status") or "").startswith("Couldn")
                   and s.get("retry"), {"seen": seen, "s": s})
        self.check("A4b still no page load and no bar after all that", isinstance(s, dict) and s.get("marker") and not s.get("bar"), s)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    src = __import__("server_source").read(platform)  # whole server source (http_server.cpp + its src/site pieces)
    script = literals(src, "std::string draft_publish_script()")
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(script))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1])
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-me-autosave-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases(me_handler_markup(src))
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT me-autosave: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
