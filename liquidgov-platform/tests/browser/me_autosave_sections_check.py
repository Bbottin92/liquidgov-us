"""Every /me section saves itself, not only Profile CSS (Brandon 2026-11-11 5:49 PM, Brief 6 item 4): About you and
Play settings use the same mechanism as a892963 (form[data-draft], draft_publish_script()) and have no Save button.

Real headless Chromium, NO C++ server (like me_autosave_check.py): a stdlib server serves a page made of the real
About you and Play settings forms, cut from the GET /me handler's literal markup in src/http_server.cpp, plus the real
draft_publish_script(). Fake POST /me/profile and /me/jukebox/play-settings answer like the real ones (303 to /me, or
/me?ok=jplay; /me?err=profile for an empty display name). Deterministic: each save is HELD by the server until the
check releases it, so "Saving…" is asserted while the save is in flight, not sampled; every wait is a wait for a
condition with a time limit (a timeout is a FAIL), never a fixed sleep.

  S1  the GET /me handler's About you and Play settings forms carry data-draft and have no Save button
  S2  typing in Name sends ONE save to /me/profile with the new value and the other fields; Saving… while it
      is held, then Saved, in About you's own status
  S3  no page load and no scroll (a marker on window survives, scrollY unchanged)
  S4  unticking "Play my Juketube" sends ONE save to /me/jukebox/play-settings without autoplay; Saved in Play
      settings' own status (About you's status is untouched)
  S5  an empty display name (the server redirects with err=profile) shows Couldn't save and Retry
Usage: LG_BROWSER=$(command -v google-chrome) python3 me_autosave_sections_check.py [<liquidgov-platform dir>]
Prints "VERDICT me-autosave-sections: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed checks
unchanged."""
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
import me_autosave_check as m  # noqa: E402  (sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

ACTIONS = ("/me/profile", "/me/jukebox/play-settings")
STATE = {"posts": [], "mode": "ok", "release": threading.Event(), "held": 0}
LOCK = threading.Lock()


def section_forms(markup):
    """The <form>…</form> for each of ACTIONS in the handler's literal markup ({} if missing)."""
    out = {}
    for a in ACTIONS:
        f = re.search(r'<form method="POST" action="%s"[^>]*>.*?</form>' % re.escape(a), markup, re.S)
        if f:
            out[a] = f.group(0)
    return out


def make_handler(forms, script):
    cards = "".join("<div class=card><h2>%s</h2>%s</div>" % (a, forms.get(a, "")) for a in ACTIONS)
    page = ("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width'></head>"
            "<body style='font-family:sans-serif'><div style='height:1500px'>top of /me</div>%s"
            "<div style='height:1500px'>below</div>%s</body></html>" % (cards, script))

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
            return self.send(200, page.encode())

        def do_POST(self):
            n = int(self.headers.get("Content-Length") or 0)
            body = urllib.parse.parse_qs(self.rfile.read(n).decode(), keep_blank_values=True)
            with LOCK:
                STATE["posts"].append({"path": self.path, "body": {k: v[0] for k, v in body.items()}})
                STATE["held"] += 1
            released = STATE["release"].wait(10)
            with LOCK:
                STATE["held"] -= 1
            if not released:
                return self.send(500, b"never released")
            if self.path == "/me/profile":
                if STATE["mode"] == "refuse" or not body.get("display_name", [""])[0].strip():
                    return self.send(303, loc="/me?err=profile")
                return self.send(303, loc="/me")
            return self.send(303, loc="/me?ok=jplay")

    return H


STATUS = r"""(() => { const out = {marker: window.__marker === 1, y: Math.round(scrollY)};
  for (const a of %s) { const f = document.querySelector('form[action="' + a + '"]'); const c = f && f.closest('.card');
    const s = c && c.querySelector('[data-draft-status]'); const r = c && c.querySelector('[data-draft-retry]');
    const b = f && [...f.querySelectorAll('button')].filter(x => !x.hidden && x.getBoundingClientRect().height > 0);
    out[a] = {status: s ? s.textContent.trim() : null, retry: !!(r && !r.hidden && r.getBoundingClientRect().height > 0),
              buttons: b ? b.map(x => x.textContent.trim()) : null}; }
  return out; })()""" % json.dumps(list(ACTIONS))


class Run(m.Run):
    def until(self, what, cond, limit=8.0):
        """Wait for cond(state) to hold (polled every 50 ms, limit seconds); return (ok, last state)."""
        end, s = time.time() + limit, None
        while time.time() < end:
            s = self.j(STATUS)
            if isinstance(s, dict) and cond(s):
                return True, s
            time.sleep(0.05)
        print("  waited %.0f s for %s; last state %s" % (limit, what, json.dumps(s)[:300]), flush=True)
        return False, s

    def posts(self):
        with LOCK:
            return list(STATE["posts"])

    def held(self, k, limit=5.0):
        end = time.time() + limit
        while time.time() < end:
            with LOCK:
                if STATE["held"] >= k:
                    return True
            time.sleep(0.05)
        return False

    def cases(self, markup):
        forms = section_forms(markup)
        bad = {a: ("no form" if a not in forms else
                   "no data-draft" if "data-draft" not in forms[a].split(">", 1)[0] else
                   "Save button" if re.search(r"<button[^>]*>\s*Save", forms[a]) else "")
               for a in ACTIONS}
        self.check("S1 About you and Play settings forms carry data-draft and no Save button",
                   not any(bad.values()), bad)
        self.ws.call("Emulation.setDeviceMetricsOverride", width=390, height=844, deviceScaleFactor=2, mobile=True)
        self.ws.call("Page.navigate", url=self.base + "/me")
        ok, s = self.until("the page and its forms", lambda s: all(s.get(a) and s[a]["buttons"] is not None for a in ACTIONS))
        if not ok:
            self.check("page loaded with both forms", False, s)
            return
        cdp.js(self.ws, "window.__marker = 1; document.querySelector('form[action=\"/me/profile\"]')"
                        ".scrollIntoView({block: 'center'}); 1")
        _, s = self.until("the scroll to settle", lambda s: s["y"] > 0)
        y0 = s["y"]
        STATE["release"].clear()
        cdp.js(self.ws, "(() => { const i = document.querySelector('input[name=display_name]'); i.focus({preventScroll: true});"
                        " i.select(); return 1; })()")
        self.ws.call("Input.insertText", text="Bee")
        held = self.held(1)
        ok, s = self.until("Saving…", lambda s: (s["/me/profile"]["status"] or "").startswith("Saving"), 3)
        p = self.posts()
        self.check("S2 typing in Name sends a save to /me/profile with the new value and the other fields",
                   held and len(p) == 1 and p[0]["path"] == "/me/profile" and p[0]["body"].get("display_name") == "Bee"
                   and {"hometown", "living", "bio", "interests", "career", "school"} <= set(p[0]["body"]), p)
        self.check("S2 ... About you shows Saving… while the save is held", ok, s)
        STATE["release"].set()
        ok, s = self.until("Saved", lambda s: s["/me/profile"]["status"] == "Saved")
        self.check("S2 ... then Saved, and only ONE save was sent", ok and len(self.posts()) == 1,
                   {"s": s, "posts": self.posts()})
        self.check("S3 no page load and no scroll", isinstance(s, dict) and s.get("marker") and abs(s["y"] - y0) <= 2,
                   {"s": s, "y0": y0})
        with LOCK:
            STATE["posts"] = []
        cdp.js(self.ws, "document.querySelector('input[name=autoplay]').click(), 1")
        ok, s = self.until("Play settings: Saved", lambda s: s["/me/jukebox/play-settings"]["status"] == "Saved")
        p = self.posts()
        self.check("S4 unticking Play my Juketube sends ONE save without autoplay; Saved in Play settings",
                   ok and len(p) == 1 and p[0]["path"] == "/me/jukebox/play-settings" and "autoplay" not in p[0]["body"]
                   and "relay" in p[0]["body"], {"s": s, "posts": p})
        self.check("S4 ... About you's own status is untouched (still Saved)", isinstance(s, dict)
                   and s["/me/profile"]["status"] == "Saved", s)
        with LOCK:
            STATE["posts"] = []
        cdp.js(self.ws, "(() => { const i = document.querySelector('input[name=display_name]'); i.focus({preventScroll: true});"
                        " i.select(); return 1; })()")
        self.ws.call("Input.dispatchKeyEvent", type="keyDown", key="Backspace", code="Backspace", windowsVirtualKeyCode=8)
        self.ws.call("Input.dispatchKeyEvent", type="keyUp", key="Backspace", code="Backspace", windowsVirtualKeyCode=8)
        ok, s = self.until("Couldn't save", lambda s: (s["/me/profile"]["status"] or "").startswith("Couldn"))
        self.check("S5 an empty display name shows Couldn't save with a Retry button",
                   ok and s["/me/profile"]["retry"] and any(x["body"].get("display_name") == "" for x in self.posts()),
                   {"s": s, "posts": self.posts()})
        self.check("S3b still no page load after all that", isinstance(s, dict) and s.get("marker"), s)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    src = __import__("server_source").read(platform)  # whole server source (http_server.cpp + its src/site pieces)
    markup = m.me_handler_markup(src)
    STATE["release"].set()
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0),
                                          make_handler(section_forms(markup), "<script>" + open(os.path.join(platform, "assets", "draft_publish.js"), encoding="utf-8").read() + "</script>"))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1])
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-me-autosave-sections-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases(markup)
        if os.environ.get("LG_SHOT"):
            import base64
            with open(os.environ["LG_SHOT"], "wb") as f:
                f.write(base64.b64decode(r.ws.call("Page.captureScreenshot", format="png")["data"]))
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        STATE["release"].set()
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT me-autosave-sections: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
