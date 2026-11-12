"""map_live_check.py's blind spots (Grok Bot's audit AUDIT-CLAUDE.md, ranked fix 4; Brief 5 item 3), in a new file
(the committed check is frozen). Real Chromium, the real lgmap.js/lgmap.css and map_page_body's markup, no C++ server.
What is different from map_live_check.py:
  - the fake /map/node graph follows the real server (src/http_server.cpp map links): people are user:<name> (never
    person:), a group links to its city or Sitewide, its user: members and its posts, and never back to mine:groups;
    a kind (mine:*) links back to you first, as the server does
  - a click on a box that is not drawn is a FAIL (map_live_check recorded it and went on, so later steps could quietly
    test something else); K5 proves the click helper reports it
  - no fixed sleeps: each step waits until the map is ready (the trail says where we are, the status line is not
    "Loading…", and the boxes and the view stop moving), with a 10 s limit that is itself a FAIL
  - the side panel's own navigation is asserted (map_live_check recorded frame_after_link but never checked it)

Cases (desktop 1600x1000; "narrow" is a 755 px window):
  K0  the first view is ready: You and its 7 kinds, Events included
  K1  Groups' ring holds exactly your 4 groups (the server's link back to you is the trail, not a ring box)
  K2  The Gov becomes the center (trail You > Groups > The Gov, at the ring's middle) with exactly its real links around
      it: Sitewide, its user: members and its post; inside the frame, no line through a box
  K3  a member (user:samyraine) becomes the center: trail one longer, ring = her links minus the trail (group:gov)
  K4  the side panel's page follows a link (its pathname changes: asserted), then Back goes exactly one center back
  K5  the click helper reports a box that is not drawn (user:nobody) instead of skipping it
  K6  zoom drift (d0cb34d's "kept inside the frame", which map_live_check's L9c never caught: it passed before the fix):
      ctrl+wheel in 4 notches at the frame's top-left corner, then out 8 notches at its bottom-right corner, to a scale
      where the map fits: every box is inside the frame
  K7  narrow: The Gov through Groups, every box inside the frame
Usage: LG_BROWSER=$(command -v google-chrome) python3 map_live_check2.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT live-map2: ALL-PASS" or the failed cases; writes <out>/map-live2.json and screenshots. Exit 1 on a FAIL."""
import base64
import http.server
import json
import os
import sys
import threading
import time
import traceback
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import map_live_check as mlc  # noqa: E402  (page_markup, HELPERS; sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

ME = mlc.ME
N = mlc.N
# Shapes from the real server: map_gnode (ids, kinds, pages) and map_links (who links to whom).
NODES = {n["id"]: n for n in [
    N("you", ME, "you", "/u/" + ME, 1),
    N("mine:politics", "Politics", "mine", "/proposals", 1, "1 proposal you touched"),
    N("mine:friends", "Friends", "mine", "/friends", 1, "3 people"),
    N("mine:events", "Events", "mine", "", 1, "No events yet"),
    N("mine:feeds", "Feeds", "mine", "/feed", 1, "Friends · Groups · Yours"),
    N("mine:places", "Places", "mine", "", 1, "Roseville, then the rest of the site"),
    N("mine:groups", "Groups", "mine", "/communities", 1, "The Gov, and 3 more"),
    N("mine:interests", "Interests", "mine", "/me", 1, "Snowboarding"),
    N("prop:7", "Genesis Executive Officer (Temporary)", "prop", "/p/7", 0.9),
    N("user:" + ME, ME, "person", "/u/" + ME, 1),
    N("user:bottinbrandon", "bottinbrandon", "person", "/u/bottinbrandon", 0.7),
    N("user:samyraine", "samyraine", "person", "/u/samyraine", 0.6),
    N("user:pal", "pal", "person", "/u/pal", 0.6),
    N("group:gov", "The Gov", "group", "/c/gov", 0.95),
    N("group:podcast", "Tim Foil Hat Podcast", "group", "/c/podcast", 0.95),
    N("group:roseville", "Roseville", "group", "/c/roseville", 0.95),
    N("group:thinktank", "LiquidGov Think Tank", "group", "/c/thinktank", 0.95),
    N("post:1", "LiGo boiiiii #SendIt", "post", "/post/1", 0.5),
    N("post:2", "Snow report", "post", "/post/2", 0.5),
    N("sitewide", "Sitewide", "sitewide", "/proposals", 0.5),
    N("city:roseville", "Roseville", "city", "/city/roseville", 1),
    N("root", "MAINstream", "root", "", -1),
    N("feed:friends", "Friends", "feed", "/feed?f=friends", 0.8),
    N("int:snow", "Snowboarding", "interest", "", 0.8),
]}
LINKS = {
    "you": ["mine:politics", "mine:friends", "mine:events", "mine:feeds", "mine:places", "mine:groups", "mine:interests"],
    "mine:politics": ["you", "prop:7"],
    "mine:friends": ["you", "user:bottinbrandon", "user:samyraine", "user:pal"],
    "mine:events": ["you"],
    "mine:feeds": ["you", "feed:friends"],
    "mine:places": ["you", "city:roseville", "root"],
    "mine:groups": ["you", "group:gov", "group:podcast", "group:roseville", "group:thinktank"],
    "mine:interests": ["you", "int:snow"],
    "group:gov": ["sitewide", "user:" + ME, "user:bottinbrandon", "user:samyraine", "post:1"],
    "group:podcast": ["sitewide", "user:bottinbrandon"],
    "group:roseville": ["city:roseville", "user:" + ME],
    "group:thinktank": ["sitewide", "user:samyraine"],
    "user:samyraine": ["group:gov", "group:thinktank", "post:2", "user:" + ME],
    "user:bottinbrandon": ["group:gov", "group:podcast", "user:" + ME],
    "city:roseville": ["root", "group:roseville", "user:" + ME],
    "root": ["you", "city:roseville", "sitewide"],
    "sitewide": ["prop:7", "group:gov", "group:podcast", "group:thinktank"],
}


def make_handler(platform, nodes, links):
    markup = mlc.page_markup(platform)

    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def send(self, code, ctype, body):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            u = urllib.parse.urlsplit(self.path)
            if u.path in ("/assets/lgmap.js", "/assets/lgmap.css"):
                with open(os.path.join(platform, "assets", os.path.basename(u.path)), "rb") as f:
                    body = f.read()
                return self.send(200, "text/css" if u.path.endswith(".css") else "application/javascript", body)
            if u.path == "/map/node":
                nid = urllib.parse.parse_qs(u.query).get("n", [""])[0]
                if nid not in nodes:
                    return self.send(404, "application/json", b"{}")
                d = {"node": nodes[nid], "links": [{"node": nodes[t], "edge": "in"} for t in links.get(nid, [])]}
                return self.send(200, "application/json", json.dumps(d).encode())
            if u.path == "/map":
                body = ("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                        "<link rel=stylesheet href=/assets/lgmap.css><style>:root{--accent:#8a6620;--paper:#f6efe0;--ink:#2a2418;"
                        "--rule:#d8ccb4;--panel:#efe6d2}body{margin:0;font-family:sans-serif;background:#f6efe0;color:#2a2418}"
                        "main{max-width:1600px;margin:0 auto;padding:0 8px}</style></head><body>"
                        "<header style='height:140px'>site header</header><main>%s</main>"
                        "<footer style='height:1400px'>site footer</footer><script src=/assets/lgmap.js defer></script>"
                        "</body></html>" % markup)
                return self.send(200, "text/html; charset=utf-8", body.encode())
            body = ("<!doctype html><html><body><h1 id=t>%s</h1><a id=next href='/other%s'>next page</a></body></html>"
                    % (u.path, u.path.replace("/", "-")))
            return self.send(200, "text/html; charset=utf-8", body.encode())

    return H


# Everything that moves when the map changes: the view's transform, every shown box's place, the status line.
SNAP = ("JSON.stringify({v: __m.view(), s: document.getElementById('lgm-status').textContent,"
        " t: __m.trail(), b: __m.boxes().map(b => [b.id, Math.round(b.l), Math.round(b.t), Math.round(b.r)])})")


class Run2(mlc.LiveRun):
    LIMIT = 10.0

    def __init__(self, base, out):
        super().__init__(base, out)
        self.missing = []

    def ready(self, what, trail=None):
        """Wait until the map is ready: trail as given (if given), not loading, and nothing moving (two snapshots 150 ms
        apart are equal). A FAIL after LIMIT seconds; returns whether it got ready."""
        end, last = time.time() + self.LIMIT, None
        while time.time() < end:
            raw = cdp.js(self.ws, "typeof __m === 'object' ? %s : null" % SNAP)
            snap = json.loads(raw) if isinstance(raw, str) else None
            ok = (isinstance(snap, dict) and snap["b"] and "Loading" not in snap["s"]
                  and (trail is None or snap["t"] == trail))
            if ok and snap == last:
                return True
            last = snap if ok else None
            time.sleep(0.15)
        self.check("ready within %gs: %s" % (self.LIMIT, what), False, {"last": last, "want trail": trail})
        return False

    def load(self, settle=None):
        self.ws.call("Page.navigate", url=self.base + "/map")
        end = time.time() + self.LIMIT
        while time.time() < end and cdp.js(self.ws, "document.readyState") != "complete":
            time.sleep(0.1)
        cdp.js(self.ws, mlc.HELPERS)
        return self.ready("first view", ["You"])

    def click_box(self, box_id, trail=None, must=True):
        """Click a drawn box. A box that is not drawn is a FAIL (must) or just reported (must=False, K5)."""
        b = self.j("__m.boxes().find(b => b.id === %s) || null" % json.dumps(box_id))
        if not isinstance(b, dict):
            self.missing.append(box_id)
            if must:
                self.check("click %s: the box is on the map" % box_id, False, [x["id"] for x in self.j("__m.boxes()") or []])
            return False
        self.click_xy((b["l"] + b["r"]) / 2, (b["t"] + b["b"]) / 2)
        return self.ready("after clicking " + box_id, trail)

    def click_sel(self, sel, trail=None):
        b = self.j("(() => { const e = document.querySelector(%s); if (!e) return null; const r = e.getBoundingClientRect();"
                   " return {x: (r.left + r.right) / 2, y: (r.top + r.bottom) / 2}; })()" % json.dumps(sel))
        if not isinstance(b, dict):
            self.check("click %s: it is on the page" % sel, False, None)
            return False
        self.click_xy(b["x"], b["y"])
        return self.ready("after clicking " + sel, trail)

    def ring_ids(self, center):
        return sorted(b["id"] for b in self.j("__m.boxes()") or [] if b["id"] != center)

    def wheel(self, x, y, dy, times):
        for _ in range(times):
            self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=dy, modifiers=2)
            self.ready("after a ctrl+wheel notch")

    def shot(self, name):
        with open(os.path.join(self.out, "map-live2-%s.png" % name), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))

    def cases(self):
        self.size(1600, 1000)
        ok = self.load()
        kinds = self.ring_ids("you")
        self.check("K0 the first view is ready: You and its 7 kinds, Events included",
                   ok and kinds == sorted(LINKS["you"]), kinds)
        self.shot("you")

        self.click_box("mine:groups", ["You", "Groups"])
        got = self.ring_ids("mine:groups")
        want = sorted(i for i in LINKS["mine:groups"] if i != "you")
        self.check("K1 Groups' ring is exactly your 4 groups (the link back to you is the trail)", got == want,
                   {"shown": got, "want": want})

        self.click_box("group:gov", ["You", "Groups", "The Gov"])
        self.shot("the-gov")
        c_ok, d = self.centered_on("The Gov")
        self.check("K2 The Gov is the center, at the middle of the ring", c_ok, d)
        got = self.ring_ids("group:gov")
        self.check("K2 around The Gov: exactly its real links (Sitewide, user: members, its post)",
                   got == sorted(LINKS["group:gov"]), {"shown": got, "want": sorted(LINKS["group:gov"])})
        self.inside("K2 The Gov")
        self.lines("K2 The Gov")

        self.click_box("user:samyraine", ["You", "Groups", "The Gov", "samyraine"])
        self.shot("samyraine")
        c_ok, d = self.centered_on("samyraine")
        self.check("K3 samyraine (user:samyraine) is the center", c_ok, d)
        got = self.ring_ids("user:samyraine")
        want = sorted(i for i in LINKS["user:samyraine"] if i != "group:gov")
        self.check("K3 around samyraine: her links minus the trail's The Gov", got == want, {"shown": got, "want": want})
        self.inside("K3 samyraine")

        # K4: the panel shows /u/samyraine; follow its link, then Back once
        p0 = cdp.js(self.ws, "(() => { try { return document.getElementById('lgm-frame').contentWindow.location.pathname; }"
                             " catch (e) { return String(e); } })()")
        clicked = cdp.js(self.ws, "(() => { const d = document.getElementById('lgm-frame').contentDocument;"
                                  " const a = d && d.getElementById('next'); if (a) a.click(); return !!a; })()")
        p1, end = p0, time.time() + self.LIMIT
        while time.time() < end and p1 == p0:
            time.sleep(0.1)
            p1 = cdp.js(self.ws, "(() => { try { return document.getElementById('lgm-frame').contentWindow.location.pathname; }"
                                 " catch (e) { return String(e); } })()")
        self.check("K4 the side panel's page followed its link (/u/samyraine -> /other-u-samyraine)",
                   clicked is True and p0 == "/u/samyraine" and p1 == "/other-u-samyraine", {"before": p0, "after": p1})
        self.click_sel("#lgm-back", ["You", "Groups", "The Gov"])
        t = self.j("__m.trail()")
        self.check("K4 then Back goes exactly one center back (The Gov)", t == ["You", "Groups", "The Gov"], t)

        # K5: the helper itself
        before = len(self.rep["checks"])
        got = self.click_box("user:nobody", must=False)
        self.check("K5 a click on a box that is not drawn is reported, not skipped",
                   got is False and self.missing[-1:] == ["user:nobody"] and len(self.rep["checks"]) == before,
                   {"returned": got, "missing": self.missing})

        # K6: zoom drift. Back at You, page at the top; in at the top-left corner, out at the bottom-right corner.
        self.load()
        cdp.js(self.ws, "window.scrollTo(0, 0), 1")
        f = self.j("__m.frame()")
        v0 = self.j("__m.view()")
        self.wheel(f["l"] + 40, f["t"] + 40, -120, 4)
        v1 = self.j("__m.view()")
        self.wheel(f["r"] - 40, f["b"] - 40, 120, 8)
        v2 = self.j("__m.view()")
        self.shot("zoom-drift")
        self.check("K6 precondition: ctrl+wheel zoomed in, then out", v0 != v1 != v2, [v0, v1, v2])
        out = self.j("__m.outside()")
        self.check("K6 after zooming in at one corner and out at the other, every box is inside the frame", out == [],
                   {"outside": out, "view": v2, "frame": f})

        # K7: narrow
        self.size(755, 900)
        self.load()
        self.click_box("mine:groups", ["You", "Groups"])
        self.click_box("group:gov", ["You", "Groups", "The Gov"])
        self.shot("narrow-the-gov")
        got = self.ring_ids("group:gov")
        self.check("K7 narrow: The Gov is the center with its real links", got == sorted(LINKS["group:gov"]), got)
        self.inside("K7 narrow The Gov")


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = argv[1] if len(argv) > 1 else os.path.join("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."), "lg-map-live2")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(platform, NODES, LINKS))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run2("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-map-live2-")
        r.rep["browser"] = r.ws.browser
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
        r.rep["missing_clicks"] = r.missing
        r.rep["verdict"] = "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails)
        path = os.path.join(out, "map-live2.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT live-map2: %s  (%d checks; report %s)" % (r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
