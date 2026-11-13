"""The live-map faults Brandon saw on 2026-11-11 ("all funky... definitely not right"; Grok Bot's MAP-LIVE-FINDINGS.md,
deploy 76a5ec1), checked in a real Chromium against the real lgmap.js/lgmap.css, with NO C++ server: a small stdlib
server here serves the map page's markup (cut from map_page_body in src/http_server.cpp, so it follows the real page)
and a fake /map/node graph shaped like what Brandon's account showed (Groups > The Gov, Politics' long proposal names,
Places > Roseville, Interests). New file; the committed browser checks are not edited.

Cases (desktop 1600x1000 unless said; "narrow" is a 755 px desktop window, Grok Bot's narrow look):
  L0  precondition: the page loads, the map draws You with its ring
  L2  #2 tapping a child box (The Gov, under Groups) makes it the center: trail You > Groups > The Gov, The Gov sits
      at the ring's middle (L2b), what is around it is The Gov's own connections (L2c), Groups' other boxes are gone (L2d)
  L3  #3 every shown box lies inside the map frame: You, Groups, The Gov, Roseville, MAINstream, and narrow
  L4  #4 no connector line runs through a box (lines end at box edges); L4b a label up to 40 characters is shown in
      full on a desktop (the box grows instead of ellipsizing), and every label stays inside its box (L4c)
  L5  #5 Back (the trail's previous box, .lgm-trail-prev; Brandon 2026-11-13: no Back button): no previous box at the
      start view; each click goes back exactly one center, also after the side panel's page followed a link (an iframe
      history entry); no previous box again at the start
  L6  #6 the top MAINstream button opens the MAINstream view (trail You > MAINstream, the MAINstream at the ring's
      middle) from a deep level and from You, instead of panning the map
  L7  #7 turning every chip off at the MAINstream does not move the map (frame and MAINstream box keep their place)
  L8  #8 the first view is centered in the frame (vertically too), not stuck in the top half
  L9  #9 in Scroll mode a plain mouse wheel over the map zooms the map and the page does not scroll (Brandon
      2026-11-13); ctrl+wheel (a trackpad pinch)
      zooms the map and does not scroll the page (L9b); after zooming out every box is still inside the frame (L9c)
Usage: LG_BROWSER=$(command -v google-chrome) python3 map_live_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT live-map: ALL-PASS" or the failed cases; writes <out>/map-live.json and screenshots. Exit 1 on a FAIL."""
import base64
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

ME = "Bbottin"


def N(id_, label, kind, page="", rel=0.6, sub=""):
    return {"id": id_, "label": label, "kind": kind, "page": page, "rel": rel, "sub": sub}


# What Brandon's account showed on 2026-11-11 (screenshots in map-live-look/), with ids in the server's shapes.
NODES = {n["id"]: n for n in [
    N("you", ME, "you", "/u/" + ME, 1),
    N("mine:politics", "Politics", "mine", "/proposals", 1, "3 proposals you touched"),
    N("mine:friends", "Friends", "mine", "/friends", 1, "3 people"),
    N("mine:feeds", "Feeds", "mine", "/feed", 1, "Friends · Groups · Yours"),
    N("mine:places", "Places", "mine", "", 1, "Roseville, then the rest of the site"),
    N("mine:groups", "Groups", "mine", "/communities", 1, "The Gov, and 3 more"),
    N("mine:interests", "Interests", "mine", "/me", 1, "Being Free, Snowboarding"),
    N("prop:7", "Genesis Executive Officer (Temporary)", "prop", "/p/7", 0.9),
    N("prop:5", "A Poll To Determine What We Call Ourselves", "prop", "/p/5", 0.9),
    N("prop:6", "Name of The Nation After the Founding Vote", "prop", "/p/6", 0.9),
    N("person:bottinbrandon", "bottinbrandon", "person", "/u/bottinbrandon", 0.7),
    N("person:samyraine", "samyraine", "person", "/u/samyraine", 0.6),
    N("person:pal", "pal", "person", "/u/pal", 0.6),
    N("person:" + ME, ME, "person", "/u/" + ME, 1),
    N("group:gov", "The Gov", "group", "/g/gov", 0.95),
    N("group:podcast", "Tim Foil Hat Podcast", "group", "/g/podcast", 0.95),
    N("group:roseville", "Roseville", "group", "/g/roseville", 0.95),
    N("group:thinktank", "LiquidGov Think Tank", "group", "/g/thinktank", 0.95),
    N("post:1", "LiGo boiiiii #SendIt", "post", "/post/1", 0.5),
    N("sitewide", "Sitewide", "sitewide", "/sitewide", 0.5),
    N("city:roseville", "Roseville", "city", "/city/roseville", 1),
    N("city:fresno", "Fresno", "city", "/city/fresno", 0.4),
    N("dept:1", "Roseville City Council", "dept", "/dept/1", 0.6),
    N("root", "MAINstream", "root", "", -1),
    N("feed:friends", "Friends", "feed", "/feed?f=friends", 0.8),
    N("feed:groups", "Groups", "feed", "/feed?f=groups", 0.8),
    N("feed:mine", "Yours", "feed", "/feed?f=mine", 0.8),
    N("int:free", "Being Free", "interest", "", 0.8),
    N("int:snow", "Snowboarding", "interest", "", 0.8),
]}
LINKS = {
    "you": ["mine:politics", "mine:friends", "mine:feeds", "mine:places", "mine:groups", "mine:interests"],
    "mine:politics": ["you", "prop:7", "prop:5", "prop:6"],
    "mine:friends": ["you", "person:bottinbrandon", "person:samyraine", "person:pal"],
    "mine:groups": ["you", "group:gov", "group:podcast", "group:roseville", "group:thinktank"],
    "mine:places": ["you", "city:roseville", "root"],
    "mine:feeds": ["you", "feed:friends", "feed:groups", "feed:mine"],
    "mine:interests": ["you", "int:free", "int:snow"],
    "group:gov": ["mine:groups", "sitewide", "person:" + ME, "person:bottinbrandon", "person:samyraine", "post:1"],
    "group:podcast": ["mine:groups", "person:bottinbrandon"],
    "group:roseville": ["mine:groups", "city:roseville", "person:" + ME],
    "group:thinktank": ["mine:groups", "person:samyraine"],
    "city:roseville": ["mine:places", "group:roseville", "dept:1", "root"],
    "city:fresno": ["root"],
    "root": ["you", "city:roseville", "city:fresno", "sitewide"],
    "sitewide": ["root", "group:gov"],
}


def page_markup(platform):
    """map_page_body's string literals, in order (the parts built at run time, like the asset URLs, are left out)."""
    src = __import__("server_source").read(platform)  # whole server source (http_server.cpp + its src/site pieces)
    a = src.index("std::string map_page_body(")
    b = src.index("\n}\n", a)
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', src[a:b])
    html = "".join(p.encode().decode("unicode_escape") for p in parts)
    html = html[html.index('<div id="lgmap"'):]
    html = html[:html.index("<script")]
    return html.replace('data-me=""', 'data-me="%s"' % ME)


def make_handler(platform):
    markup = page_markup(platform)

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
                if nid not in NODES:
                    return self.send(404, "application/json", b"{}")
                d = {"node": NODES[nid], "links": [{"node": NODES[t], "edge": "in"} for t in LINKS.get(nid, [])]}
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
            # any other path is a box's page, shown in the side panel; it links on, so the frame can make history
            body = ("<!doctype html><html><body><h1 id=t>%s</h1><a id=next href='/other%s'>next page</a></body></html>"
                    % (u.path, u.path.replace("/", "-")))
            return self.send(200, "text/html; charset=utf-8", body.encode())

    return H


# In-page helpers. Boxes are measured by their shape (.lgs), on screen.
HELPERS = r"""
window.__m = {
  frame() { const r = document.getElementById('lgm-svg').getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom}; },
  boxes() {
    return [...document.querySelectorAll('#lgm-nodes > g.lgn')].filter(g => getComputedStyle(g).display !== 'none').map(g => {
      const s = g.querySelector('.lgs').getBoundingClientRect();
      const texts = [...g.querySelectorAll('text')].filter(t => t.textContent).map(t => { const r = t.getBoundingClientRect();
        return {s: t.textContent, l: r.left, t: r.top, r: r.right, b: r.bottom}; });
      return {id: g.getAttribute('data-id'), label: g.querySelector('title').textContent, cls: g.getAttribute('class'),
              l: s.left, t: s.top, r: s.right, b: s.bottom, texts: texts};
    });
  },
  trail() { return [...document.querySelectorAll('#lgm-trail .lgm-trail-step')].map(b => b.textContent.trim()); },
  ring() { const e = document.getElementById('lgm-ring'); if (!e) return null; const r = e.getBoundingClientRect();
           return {x: (r.left + r.right) / 2, y: (r.top + r.bottom) / 2, l: r.left, t: r.top, r: r.right, b: r.bottom}; },
  view() { return document.getElementById('lgm-view').style.transform; },
  back() { return !document.querySelector('#lgm-trail .lgm-trail-prev'); },
  outside() {
    const f = this.frame();
    return this.boxes().filter(b => b.l < f.l - 1 || b.r > f.r + 1 || b.t < f.t - 1 || b.b > f.b + 1).map(b => b.label);
  },
  // points of every shown line that fall inside a shown box (shrunk 3 px, so a line ending on the edge is fine)
  lineHits() {
    const boxes = this.boxes(), hits = [];
    for (const p of document.querySelectorAll('#lgm-edges path.lge')) {
      if (getComputedStyle(p).display === 'none') continue;
      const len = p.getTotalLength(), m = p.getScreenCTM();
      for (let i = 0; i <= 60; i++) {
        const q = p.getPointAtLength(len * i / 60), x = m.a * q.x + m.c * q.y + m.e, y = m.b * q.x + m.d * q.y + m.f;
        // You (a circle) and cities (ovals) by their own shape: a line ending on the curve passes their box's corners
        const inBox = b => /lgn-(you|city)\b/.test(b.cls)
          ? ((x - (b.l + b.r) / 2) / ((b.r - b.l) / 2 - 3)) ** 2 + ((y - (b.t + b.b) / 2) / ((b.b - b.t) / 2 - 3)) ** 2 < 1
          : x > b.l + 3 && x < b.r - 3 && y > b.t + 3 && y < b.b - 3;
        for (const b of boxes) if (inBox(b)) {
          hits.push({edge: p.getAttribute('data-a') + '|' + p.getAttribute('data-b'), box: b.label}); i = 61; break; }
      }
    }
    return hits;
  },
  // the bounding box of every shown box and the dotted ring, on screen
  extent() {
    const bs = this.boxes(), r = this.ring(); if (r) bs.push(r);
    return {l: Math.min(...bs.map(b => b.l)), t: Math.min(...bs.map(b => b.t)), r: Math.max(...bs.map(b => b.r)), b: Math.max(...bs.map(b => b.b))};
  },
};
1
"""


class LiveRun:
    def __init__(self, base, out):
        self.base, self.out = base, out
        self.rep = {"checks": {}}
        self.fails = []
        self.ws = None

    def check(self, name, ok, detail):
        self.rep["checks"][name] = {"ok": bool(ok), "detail": detail}
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:600]), flush=True)
        if not ok:
            self.fails.append(name)

    def j(self, expr):
        v = cdp.js(self.ws, "JSON.stringify(%s)" % expr)
        try:
            return json.loads(v) if isinstance(v, str) else v
        except ValueError:  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def shot(self, name):
        with open(os.path.join(self.out, "map-live-%s.png" % name), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))

    def size(self, w, h):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=1, mobile=False)

    def load(self, settle=2.0):
        self.ws.call("Page.navigate", url=self.base + "/map")
        for _ in range(80):
            if cdp.js(self.ws, "document.readyState") == "complete":
                break
            time.sleep(0.25)
        time.sleep(settle)
        cdp.js(self.ws, HELPERS)

    def click_xy(self, x, y, modifiers=0):
        for t in ("mousePressed", "mouseReleased"):
            self.ws.call("Input.dispatchMouseEvent", type=t, x=x, y=y, button="left", clickCount=1, modifiers=modifiers)

    def click_box(self, box_id, settle=1.2):
        b = self.j("__m.boxes().find(b => b.id === %s) || null" % json.dumps(box_id))
        if not isinstance(b, dict):
            self.rep.setdefault("missing_clicks", []).append(box_id)
            return False
        self.click_xy((b["l"] + b["r"]) / 2, (b["t"] + b["b"]) / 2)
        time.sleep(settle)
        return True

    def click_sel(self, sel, settle=1.2):
        b = self.j("(() => { const e = document.querySelector(%s); if (!e) return null; const r = e.getBoundingClientRect();"
                   " return {x: (r.left + r.right) / 2, y: (r.top + r.bottom) / 2}; })()" % json.dumps(sel))
        if not isinstance(b, dict):
            self.rep.setdefault("missing_clicks", []).append(sel)
            return False
        self.click_xy(b["x"], b["y"])
        time.sleep(settle)
        return True

    def centered_on(self, label):
        """The box with this label sits at the dotted ring's middle (it is the center)."""
        r = self.j("{ring: __m.ring(), boxes: __m.boxes().filter(b => b.label === %s)}" % json.dumps(label))
        if not isinstance(r, dict) or not r.get("ring") or not r.get("boxes"):
            return False, r
        b = r["boxes"][0]
        dx, dy = (b["l"] + b["r"]) / 2 - r["ring"]["x"], (b["t"] + b["b"]) / 2 - r["ring"]["y"]
        return abs(dx) <= 3 and abs(dy) <= 3, {"dx": dx, "dy": dy, "box": b, "ring": r["ring"]}

    def inside(self, name):
        out = self.j("__m.outside()")
        self.check("L3 every box inside the map frame: " + name, out == [], {"outside": out, "frame": self.j("__m.frame()")})

    def lines(self, name):
        hits = self.j("__m.lineHits()")
        self.check("L4 no line runs through a box: " + name, hits == [], hits)

    def labels(self, name):
        bs = self.j("__m.boxes()")
        short = [b["label"] for b in bs if len(b["label"]) <= 40 and b["texts"] and "…" in b["texts"][0]["s"]]
        self.check("L4b labels up to 40 characters shown in full: " + name, isinstance(bs, list) and short == [], short)
        spill = [{"label": b["label"], "text": t} for b in bs for t in b["texts"]
                 if t["l"] < b["l"] - 1 or t["r"] > b["r"] + 1]
        self.check("L4c every label stays inside its box: " + name, isinstance(bs, list) and spill == [], spill)

    def cases(self):
        self.size(1600, 1000)
        self.load()
        b0 = self.j("__m.boxes()")
        self.check("L0 precondition: the map draws You and its ring", isinstance(b0, list) and len(b0) >= 6
                   and any(b["id"] == "you" for b in b0), b0)
        self.shot("you")
        # L8: centered in the frame
        f, e = self.j("__m.frame()"), self.j("__m.extent()")
        if isinstance(f, dict) and isinstance(e, dict):
            dy = (e["t"] + e["b"]) / 2 - (f["t"] + f["b"]) / 2
            dx = (e["l"] + e["r"]) / 2 - (f["l"] + f["r"]) / 2
            self.check("L8 the first view is centered in the frame (within 30 px both ways)", abs(dy) <= 30 and abs(dx) <= 30,
                       {"dx": dx, "dy": dy, "frame": f, "extent": e})
        else:
            self.check("L8 the first view is centered in the frame (within 30 px both ways)", False, [f, e])
        self.inside("You")
        self.lines("You")
        self.check("L5 no previous box at the start view", self.j("__m.back()") is True, self.j("__m.back()"))

        # L2 / L3 / L4: Groups, then The Gov (desktop click: the side panel opens and the map narrows, as on the live site)
        self.click_box("mine:groups")
        self.inside("Groups")
        self.check("L5 precondition: a previous box one step in", self.j("__m.back()") is False, None)
        self.click_box("group:gov", settle=1.6)
        self.shot("the-gov")
        t = self.j("__m.trail()")
        self.check("L2 tapping The Gov makes it the center: trail You > Groups > The Gov", t == ["You", "Groups", "The Gov"], t)
        ok, d = self.centered_on("The Gov")
        self.check("L2b The Gov sits at the middle of the ring", ok, d)
        ids = sorted(b["id"] for b in self.j("__m.boxes()") if b["id"] != "group:gov")
        want = sorted(i for i in LINKS["group:gov"] if i != "mine:groups")
        self.check("L2c around The Gov: its own connections (the way back is the trail)", ids == want, {"shown": ids, "want": want})
        self.check("L2d Groups' other boxes are gone", not any(i in ids for i in ("group:podcast", "group:roseville", "group:thinktank")), ids)
        self.inside("The Gov (side panel open)")
        self.lines("The Gov")
        self.labels("The Gov")

        # L5: the side panel's page follows a link (an iframe history entry), then Back twice
        cdp.js(self.ws, "(() => { const d = document.getElementById('lgm-frame').contentDocument; const a = d && d.getElementById('next');"
                        " if (a) a.click(); return !!a; })()")
        time.sleep(1.2)
        self.rep["frame_after_link"] = cdp.js(self.ws, "(() => { try { return document.getElementById('lgm-frame').contentWindow.location.pathname; }"
                                                       " catch (e) { return String(e); } })()")
        self.click_sel("#lgm-trail .lgm-trail-prev")
        t1 = self.j("__m.trail()")
        self.check("L5 Back once (after the panel's page followed a link) goes to Groups", t1 == ["You", "Groups"], t1)
        self.click_sel("#lgm-trail .lgm-trail-prev")
        t2 = self.j("__m.trail()")
        self.check("L5 Back again goes to You", t2 == ["You"], t2)
        self.check("L5 no previous box again at the start view", self.j("__m.back()") is True, None)
        cdp.js(self.ws, "(() => { const p = document.querySelector('#lgm-trail .lgm-trail-prev'); if (p) p.click(); return 1; })()")
        time.sleep(0.6)
        self.check("L5 with no previous box nothing goes back (the trail stays You)", self.j("__m.trail()") == ["You"], self.j("__m.trail()"))
        # two steps down (You > Politics > a proposal), then two Backs: one step each
        self.click_box("mine:politics")
        self.click_box("prop:7")
        steps = [self.j("__m.trail()")]
        for _ in range(2):
            self.click_sel("#lgm-trail .lgm-trail-prev", settle=0.8)
            steps.append(self.j("__m.trail()"))
        self.check("L5 each Back is exactly one step (Genesis... > Politics > You)",
                   [len(s) if isinstance(s, list) else -1 for s in steps] == [3, 2, 1], steps)

        # L4 long labels: Politics' proposals
        self.click_box("mine:politics")
        self.shot("politics")
        self.labels("Politics")
        self.lines("Politics")
        self.inside("Politics")
        self.click_sel("#lgm-trail .lgm-trail-prev")

        # L6: MAINstream button from a deep level, then from You
        self.click_box("mine:places")
        self.click_box("city:roseville")
        self.inside("Roseville")
        self.lines("Roseville")
        self.click_sel("#lgm-main", settle=1.6)
        self.shot("mainstream-from-deep")
        t6 = self.j("__m.trail()")
        ok, d = self.centered_on("MAINstream")
        self.check("L6 MAINstream button from Places > Roseville opens the MAINstream view (trail You > MAINstream)",
                   t6 == ["You", "MAINstream"] and ok, {"trail": t6, "center": d})
        self.inside("MAINstream")
        self.lines("MAINstream")
        # L7: every chip off at the MAINstream: nothing moves
        f0, r0 = self.j("__m.frame()"), self.j("__m.boxes().find(b => b.id === 'root') || null")
        cdp.js(self.ws, "(() => { document.querySelectorAll('#lgm-filter .lgm-chip').forEach(c => c.click()); return 1; })()")
        time.sleep(1)
        f1, r1 = self.j("__m.frame()"), self.j("__m.boxes().find(b => b.id === 'root') || null")
        moved = (not isinstance(f0, dict) or not isinstance(f1, dict) or not isinstance(r0, dict) or not isinstance(r1, dict)
                 or abs(f0["t"] - f1["t"]) > 1 or abs(f0["b"] - f1["b"]) > 1 or abs(r0["t"] - r1["t"]) > 1 or abs(r0["l"] - r1["l"]) > 1)
        self.check("L7 all chips off at the MAINstream: the frame and the MAINstream box stay put", not moved,
                   {"frame": [f0, f1], "root": [r0, r1]})
        self.shot("mainstream-chips-off")
        cdp.js(self.ws, "(() => { document.querySelectorAll('#lgm-filter .lgm-chip').forEach(c => c.click()); return 1; })()")
        time.sleep(0.5)
        # back to You by the trail, then the MAINstream button from You
        self.click_sel("#lgm-trail .lgm-trail-step")
        self.check("L6 precondition: back at You", self.j("__m.trail()") == ["You"], self.j("__m.trail()"))
        self.click_sel("#lgm-main", settle=1.6)
        t6b = self.j("__m.trail()")
        ok, d = self.centered_on("MAINstream")
        self.check("L6 MAINstream button from You opens the MAINstream view", t6b == ["You", "MAINstream"] and ok,
                   {"trail": t6b, "center": d})
        f = self.j("__m.frame()")
        rb = self.j("__m.boxes().find(b => b.id === 'root') || null")
        mid = isinstance(f, dict) and isinstance(rb, dict) and abs((rb["l"] + rb["r"]) / 2 - (f["l"] + f["r"]) / 2) <= 40
        self.check("L6 ...with the MAINstream in the middle of the frame, not panned aside", mid, {"frame": f, "root": rb})

        # L9: the wheel. Back at You, page scrolled to the top, pointer over the map's middle.
        self.load()
        cdp.js(self.ws, "window.scrollTo(0, 0), 1")
        time.sleep(0.3)
        f = self.j("__m.frame()")
        x, y = (f["l"] + f["r"]) / 2, (f["t"] + f["b"]) / 2
        v0, s0 = self.j("__m.view()"), self.j("window.scrollY")
        self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=240)
        time.sleep(0.8)
        v1, s1 = self.j("__m.view()"), self.j("window.scrollY")
        self.check("L9 Scroll mode: a plain wheel over the map zooms the map and the page does not scroll", s1 == s0 and v1 != v0,
                   {"scroll": [s0, s1], "view": [v0, v1]})
        cdp.js(self.ws, "window.scrollTo(0, 0), 1")
        time.sleep(0.4)
        f = self.j("__m.frame()")
        x, y = (f["l"] + f["r"]) / 2, (f["t"] + f["b"]) / 2
        v0, s0 = self.j("__m.view()"), self.j("window.scrollY")
        for _ in range(3):
            self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=120, modifiers=2)
            time.sleep(0.2)
        time.sleep(0.6)
        v1, s1 = self.j("__m.view()"), self.j("window.scrollY")
        self.check("L9b ctrl+wheel (pinch) zooms the map and does not scroll the page", v1 != v0 and s1 == s0,
                   {"scroll": [s0, s1], "view": [v0, v1]})
        self.shot("after-zoom-out")
        self.inside("after ctrl+wheel zoom out (L9c)")

        # narrow: a 755 px window (Grok Bot's narrow look)
        self.size(755, 900)
        self.load()
        self.shot("narrow-you")
        self.inside("narrow You")
        self.lines("narrow You")
        self.click_box("mine:groups", settle=1.6)
        self.click_box("group:gov", settle=1.6)
        self.shot("narrow-the-gov")
        t = self.j("__m.trail()")
        self.check("L2 narrow: tapping The Gov makes it the center", t == ["You", "Groups", "The Gov"], t)
        self.inside("narrow The Gov")
        self.lines("narrow The Gov")
        cdp.js(self.ws, "window.scrollTo(0, 0), 1")
        time.sleep(0.3)
        f = self.j("__m.frame()")
        v0, s0 = self.j("__m.view()"), self.j("window.scrollY")
        self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=(f["l"] + f["r"]) / 2, y=(f["t"] + f["b"]) / 2,
                     deltaX=0, deltaY=240)
        time.sleep(0.8)
        v1, s1 = self.j("__m.view()"), self.j("window.scrollY")
        self.check("L9 narrow, Scroll mode: a plain wheel over the map zooms the map and the page does not scroll", s1 == s0 and v1 != v0,
                   {"scroll": [s0, s1], "view": [v0, v1]})


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = argv[1] if len(argv) > 1 else os.path.join("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."), "lg-map-live")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(platform))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = LiveRun("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-map-live-")
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
        r.rep["verdict"] = "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails)
        path = os.path.join(out, "map-live.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT live-map: %s  (%d checks; report %s)" % (r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
