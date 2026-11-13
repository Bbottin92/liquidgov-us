"""Walker controls (Brandon 2026-11-13, 12:28 AM: "the desktop would be wasd controls and enter to select and esc to go
back. also the button that turns dripping on should be at the top of the screen"; 12:30 AM: "d pad lower left, A and B
buttons lower right. right now theyre all scrunched in the lower left"). New file: no committed check is edited.

Real headless Chromium, NO C++ server (like site_hidden_check.py): a stdlib server serves a profile-like page (site
header, a button, a text field, the Juketube button, the footer) with the real assets/site.css, assets/juketube.css and
assets/walker.js + assets/here.js, and a stand-in for the dots WebSocket that echoes your own dot back. The map cases
serve map_page_body's markup with the real lgmap.js/lgmap.css + walker.js + lgmap_walk.js and map_live_check2.py's
fake graph. Every key and button name comes from walker.js's one controls map (window.LG_WALKER.CONTROLS); this file
keeps no copy of the bindings.

  K0  precondition: the controls map is on the page and binds every direction, select and back to keys and buttons
  Phone 390x844 (walking):
  P1  the D-pad's center is in the left half and the bottom quarter of the screen
  P2  A and B are each in the right half and the bottom quarter
  P3  no two controls overlap (D-pad, A, B, the Drop button, the Juketube button)
  P4  every control is at least 44x44 px
  P5  the Drop button (Brandon's name; it turns walking on) is in the top 15% of the screen and does not cover the site header's middle
  P6  the Drop button shows the map's own label for it ("Stop dropping" while on)
  Desktop 1280x800:
  D1  the Drop button is in the top 15% of the screen and does not cover the site header's middle
  D2  each movement key in the map (W/A/S/D and the arrows) moves your dot its way
  D3  select (Enter) uses the button you stand next to (positive control: nothing was used before)
  D4  keys typed in a text field stay in the field (the dot does not move); back (Esc) there leaves the field
  D5  back (Esc) goes back a page
  Map (Brandon 2026-11-13 12:36 AM: the walker stands on a box and walks along a connector to the next one):
  M1  1280x800: each letter key of the map (D/A/W/S) moves the walker from you along a drawn connector to a connected
      box that lies that way, and the opposite key walks it back to you
  Q1  the portal (walker.js's CONTROLS.portal: the header's MAINstream link) is round and marked when you stand by it
  Q2  dropped in next to it, select (Enter) takes you to the map, dropped in, standing at this page's box (user:pal)
  M4  1280x800: Drop zooms the map toward the walker (a box grows by about LG_WALKER.CONTROLS.mapZoom), the camera
      follows it to the next box (that box ends up near the frame's middle), and leaving Drop restores the zoom
  M2  1280x800: select (Enter) opens the box you stand on (its page opens in the side panel; positive control: closed
      before); back (Esc) closes it
  M3  390x844: the D-pad moves the walker to a connected box; A does what a tap does (the box becomes the center:
      the trail ends at it); B goes back (the trail is You again)
Usage: LG_BROWSER=$(command -v google-chrome) python3 walker_controls_check.py [<liquidgov-platform dir> [<out dir>]]
Writes <out dir>/phone-walker-controls-*.png. Prints "VERDICT walker-controls: ALL-PASS" or the failed cases.
Exit 1 on a FAIL."""
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
import me_autosave_check as m  # noqa: E402  (sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402
import map_live_check as mlc  # noqa: E402  (map_page_body's markup)
import map_live_check2 as m2  # noqa: E402  (the fake graph)

# Stand-in for /here/ws: you appear where the page says (next to #use), then every position you send is echoed back.
FAKE_WS = r"""<script>
window.LG_USED = 0;
window.WebSocket = function () {
  var ws = this; ws.readyState = 1;
  function tell(x, y) { if (ws.onmessage) ws.onmessage({data: JSON.stringify({people: [{u: 'Bbottin', x: x, y: y}]})}); }
  setTimeout(function () {
    var t = /near=portal/.test(location.search) ? document.querySelector(LG_WALKER.CONTROLS.portal.selector) : document.getElementById('use');
    var b = t.getBoundingClientRect();
    var W = Math.max(document.documentElement.scrollWidth, document.body.scrollWidth);
    var H = Math.max(document.documentElement.scrollHeight, document.body.scrollHeight);
    if (ws.onopen) ws.onopen();
    var x = t.id === 'use' ? b.left - 20 : b.right + 20;
    tell((x + scrollX) / W, (b.top + scrollY + b.height / 2) / H);
  }, 50);
  ws.send = function (s) { var p = JSON.parse(s); tell(p.x, p.y); };
};
window.LG_HERE = {me: 'Bbottin', page: '/u/pal', box: 'user:pal'};
</script>"""


def page(title):
    return ("<!doctype html><html lang=en><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'><title>%s</title>"
            "<link rel=stylesheet href=/assets/site.css><link rel=stylesheet href=/assets/juketube.css></head><body>"
            "<header class=top><div class=topinner><a class=brand href=/map><svg class=mark viewBox='0 0 48 48' width=28 height=28>"
            "<circle cx=24 cy=24 r=19 fill=none stroke=#8a6620 /></svg><div><div class=word><strong>LiquidGov</strong>"
            "<span class=tld>.US</span></div><div class=tagline>Civic association</div></div></a>"
            "<label class=navbtn>Menu</label></div></header>"
            "<main class=wrap><h1>stranger</h1><p>A profile page.</p>"
            "<p style='margin:3rem 0 3rem 200px'><button type=button id=use onclick='window.LG_USED++'>Send friend request</button></p>"
            "<p><input id=field type=text aria-label=Note></p><div style='height:900px'></div></main>"
            "<footer class=site>Not a government.</footer><button id=jbtoggle aria-label=Juketube></button>"
            "%s<script src=/assets/walker.js></script><script src=/assets/here.js></script></body></html>" % (title, FAKE_WS))


def map_page(markup):
    return ("<!doctype html><html lang=en><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'><title>map</title>"
            "<link rel=stylesheet href=/assets/site.css><link rel=stylesheet href=/assets/lgmap.css></head><body>"
            "<header class=top><div class=topinner><a class=brand href=/start><div><div class=word><strong>LiquidGov</strong>"
            "</div></div></a><label class=navbtn>Menu</label></div></header><main class=wrap>%s</main>"
            "<footer class=site>Not a government.</footer><script src=/assets/lgmap.js defer></script>"
            "<script src=/assets/walker.js defer></script><script src=/assets/lgmap_walk.js defer></script>"
            "</body></html>" % markup)


def make_handler(assets, markup):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            path = self.path.split("?")[0]
            if path == "/map/node":
                nid = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query).get("n", [""])[0]
                d = {"node": m2.NODES[nid], "links": [{"node": m2.NODES[t], "edge": "in"} for t in m2.LINKS.get(nid, [])]} \
                    if nid in m2.NODES else {}
                return self.reply(200 if d else 404, "application/json", json.dumps(d).encode())
            if path == "/map":
                return self.reply(200, "text/html", map_page(markup).encode())
            if path.startswith("/assets/"):
                f = os.path.join(assets, os.path.basename(path))
                if not os.path.isfile(f):
                    self.send_response(404)
                    self.end_headers()
                    return
                body = open(f, "rb").read()
                ctype = "text/css" if f.endswith(".css") else "text/javascript"
            elif path in ("/walk", "/start", "/u/pal"):
                body = page("walk" if path == "/walk" else "start").encode()
                ctype = "text/html"
            else:   # a box's page, opened in the map's side panel
                body = ("<!doctype html><html><body><h1 id=t>%s</h1></body></html>" % path).encode()
                ctype = "text/html"
            self.reply(200, ctype, body)

        def reply(self, code, ctype, body):
            self.send_response(code)
            self.send_header("Content-Type", ctype + "; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

    return H


# The controls' boxes, named by the map in here.js (no copy of the class names here).
BOXES = r"""(() => { const C = window.LG_WALKER && LG_WALKER.CONTROLS, q = s => document.querySelector(s), out = {vw: innerWidth, vh: innerHeight};
  if (!C) return JSON.stringify({noMap: true});
  const box = e => { if (!e) return null; const r = e.getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom, w: r.width, h: r.height}; };
  out.dpad = box(q('#lg-here-pad .lg-here-dpad'));
  out.buttons = {}; Object.keys(C.buttons).forEach(k => { const e = q('#lg-here-pad .' + C.buttons[k].cls); out.buttons[k] = box(e); if (out.buttons[k]) out.buttons[k].text = e.textContent; });
  out.jb = box(q('#jbtoggle'));
  const h = q('header.top').getBoundingClientRect(), hit = document.elementFromPoint(h.left + h.width / 2, h.top + h.height / 2);
  out.headerHit = !!hit && q('header.top').contains(hit);
  return JSON.stringify(out); })()"""
DOT = ("(() => { const d = document.querySelector('#lg-here .lg-here-dot.me'); if (!d) return JSON.stringify(null);"
       " const r = d.getBoundingClientRect(); return JSON.stringify([r.left + scrollX, r.top + scrollY]); })()")
WAY = {"up": (0, -1), "down": (0, 1), "left": (-1, 0), "right": (1, 0)}
VK = {"arrowup": 38, "arrowdown": 40, "arrowleft": 37, "arrowright": 39, "enter": 13, "escape": 27}
DOM_KEY = {"arrowup": "ArrowUp", "arrowdown": "ArrowDown", "arrowleft": "ArrowLeft", "arrowright": "ArrowRight",
           "enter": "Enter", "escape": "Escape"}


def overlaps(a, b):
    return a and b and a["l"] < b["r"] and b["l"] < a["r"] and a["t"] < b["b"] and b["t"] < a["b"]


class Run(m.Run):
    def __init__(self, base, out):
        super().__init__(base)
        self.out = out

    def j(self, expr):
        """expr evaluates to a JSON string (or a promise of one); returns it decoded."""
        v = cdp.js(self.ws, expr)
        try:
            return json.loads(v) if isinstance(v, str) else v
        except ValueError:  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def shot(self, name, clip=None):
        args = {"format": "png"}
        if clip:
            args["clip"] = dict(clip, scale=1)
        path = os.path.join(self.out, "phone-walker-controls-%s.png" % name)
        with open(path, "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", **args)["data"]))
        print("SHOT " + path, flush=True)

    def size(self, w, h, mobile):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=2 if mobile else 1, mobile=mobile)

    def goto(self, path):
        self.ws.call("Page.navigate", url=self.base + path)
        self.wait("document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")

    def wait(self, cond, limit=10):
        end = time.time() + limit
        while time.time() < end:
            if self.j("JSON.stringify(!!(%s))" % cond) is True:
                return True
            time.sleep(0.05)
        return False

    def key(self, k, hold_s=0.05, text=False):
        """k is a key as the map names it (lowercase KeyboardEvent.key)."""
        dom = DOM_KEY.get(k, k)
        vk = VK.get(k, ord(k.upper()) if len(k) == 1 else 0)
        extra = {"text": k} if text and len(k) == 1 else {}
        self.ws.call("Input.dispatchKeyEvent", type="keyDown", key=dom, windowsVirtualKeyCode=vk, **extra)
        time.sleep(hold_s)
        self.ws.call("Input.dispatchKeyEvent", type="keyUp", key=dom, windowsVirtualKeyCode=vk)

    def drop(self):
        self.j("(() => { document.querySelector('#lg-here-pad .' + LG_WALKER.CONTROLS.buttons.drop.cls).click(); return '1'; })()")
        self.wait("!document.querySelector('#lg-here-pad .lg-here-keys').hidden", 3)

    def drop_at_top(self, tag, b):
        t = (b.get("buttons") or {}).get("drop")
        self.check("%s the Drop button is in the top 15%% of the screen and the header's middle is uncovered" % tag,
                   bool(t) and t["b"] <= 0.15 * b["vh"] and t["t"] >= 0 and b.get("headerHit"), {"drop": t, "vh": b.get("vh"),
                                                                                                   "headerHit": b.get("headerHit")})

    def phone(self):
        self.size(390, 844, True)
        self.goto("/walk")
        self.drop()
        time.sleep(0.3)
        b = self.j(BOXES) or {}
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        acts = set((C.get("keys") or {}).values())
        self.check("K0 the controls map binds every direction, select and back to keys and buttons",
                   bool(C) and set(C.get("dirs", [])) == set(WAY) and set(WAY) | {"select", "back"} <= acts
                   and all(a in C.get("buttons", {}) for a in list(WAY) + ["select", "back", "drop"]), C)
        vw, vh, btn = b.get("vw", 0), b.get("vh", 0), b.get("buttons") or {}
        dp = b.get("dpad")
        self.check("P1 phone: the D-pad's center is in the left half and the bottom quarter",
                   bool(dp) and (dp["l"] + dp["r"]) / 2 < vw / 2 and (dp["t"] + dp["b"]) / 2 > 0.75 * vh, {"dpad": dp, "vw": vw, "vh": vh})
        for act in ("select", "back"):
            r = btn.get(act)
            self.check("P2 phone: %s (%s) is in the right half and the bottom quarter" % (act, (C.get("buttons", {}).get(act) or {}).get("text")),
                       bool(r) and (r["l"] + r["r"]) / 2 > vw / 2 and (r["t"] + r["b"]) / 2 > 0.75 * vh, r)
        parts = {"dpad": dp, "select": btn.get("select"), "back": btn.get("back"), "drop": btn.get("drop"), "juketube": b.get("jb")}
        names = list(parts)
        clash = [(x, y) for i, x in enumerate(names) for y in names[i + 1:] if overlaps(parts[x], parts[y])]
        self.check("P3 phone: no two controls overlap (D-pad, A, B, Drop button, Juketube button)",
                   all(parts.values()) and not clash, {"overlaps": clash, "boxes": parts})
        small = {k: (r["w"], r["h"]) for k, r in btn.items() if r and (r["w"] < 44 or r["h"] < 44)}
        self.check("P4 phone: every control is at least 44x44 px", len(btn) >= 7 and all(btn.values()) and not small,
                   {"small": small, "count": len(btn)})
        self.drop_at_top("P5 phone:", b)
        self.check("P6 the Drop button reads its name from the map (on: %r)" % (C.get("buttons", {}).get("drop") or {}).get("on"),
                   (btn.get("drop") or {}).get("text") == (C.get("buttons", {}).get("drop") or {}).get("on"), btn.get("drop"))
        self.shot("phone", {"x": 0, "y": 0, "width": vw, "height": vh})

    def desktop(self):
        self.size(1280, 800, False)
        self.goto("/start")
        self.j("(() => { location.href = '/walk'; return '1'; })()")
        time.sleep(0.3)
        self.wait("location.pathname === '/walk' && document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")
        self.drop()
        b = self.j(BOXES) or {}
        self.drop_at_top("D1 desktop:", b)
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        for k, act in sorted((C.get("keys") or {}).items()):
            if act not in WAY:
                continue
            before = self.j(DOT)
            self.key(k, 0.3)
            time.sleep(0.15)
            after = self.j(DOT)
            dx, dy = WAY[act]
            moved = bool(before and after) and ((after[0] - before[0]) * dx + (after[1] - before[1]) * dy) > 10
            self.check("D2 desktop: key %r moves the dot %s" % (k, act), moved, (before, after))
            back_k = sorted(kk for kk, a in C["keys"].items() if a == {"up": "down", "down": "up", "left": "right", "right": "left"}[act])[0]
            self.key(back_k, 0.3)   # walk back, so the dot stays next to the button
            time.sleep(0.15)
        self.shot("desktop", {"x": 0, "y": 0, "width": 1280, "height": 800})
        sel = [kk for kk, a in C.get("keys", {}).items() if a == "select"]
        near = self.j("JSON.stringify(!!document.querySelector('#use.lg-here-near'))")
        used0 = self.j("JSON.stringify(window.LG_USED)")
        self.check("D3 desktop: positive control, you stand next to the button and nothing was used yet", near is True and used0 == 0,
                   {"near": near, "used": used0})
        for kk in sel:
            self.key(kk)
        time.sleep(0.2)
        used = self.j("JSON.stringify(window.LG_USED)")
        self.check("D3 desktop: select (%s) uses the button you stand next to" % "/".join(sel), bool(sel) and used == len(sel), used)
        self.j("(() => { document.getElementById('field').focus(); return '1'; })()")
        before = self.j(DOT)
        typed = [kk for kk, a in C.get("keys", {}).items() if a in WAY and len(kk) == 1]
        for kk in typed:
            self.key(kk, 0.15, text=True)
        time.sleep(0.15)
        after = self.j(DOT)
        val = self.j("JSON.stringify(document.getElementById('field').value)")
        self.check("D4 desktop: keys typed in a text field stay there (the dot does not move)",
                   bool(typed) and before == after and val == "".join(typed), {"dot": (before, after), "value": val, "typed": typed})
        back = [kk for kk, a in C.get("keys", {}).items() if a == "back"]
        for kk in back:
            self.key(kk)
        time.sleep(0.1)
        self.check("D4 desktop: back (%s) in a text field leaves the field" % "/".join(back),
                   bool(back) and self.j("JSON.stringify(document.activeElement && document.activeElement.id)") != "field",
                   self.j("JSON.stringify(document.activeElement && document.activeElement.id)"))
        path0 = self.j("JSON.stringify(location.pathname)")
        for kk in back[:1]:
            self.key(kk)
        self.wait("location.pathname === '/start'", 5)
        path1 = self.j("JSON.stringify(location.pathname)")
        self.check("D5 desktop: back (%s) goes back a page" % "/".join(back[:1]), path0 == "/walk" and path1 == "/start", (path0, path1))


    # ---- the map
    WALK = ("(() => { const w = document.getElementById('lgm-walker'), t = document.querySelector('.lgm-trail-step.here'),"
            " p = document.getElementById('lgm-panel'), f = document.getElementById('lgm-frame');"
            " const edges = [...document.querySelectorAll('#lgm-edges .lge')].map(e => [e.getAttribute('data-a'), e.getAttribute('data-b')]);"
            " const mid = id => { const b = id && document.querySelector('#lgm-nodes .lgn[data-id=\"' + id + '\"]');"
            " if (!b) return null; const r = b.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2]; };"
            " const at = w && !w.hidden ? w.getAttribute('data-at') : null;"
            " return JSON.stringify({at: at, mid: mid(at), you: mid('you'), trail: t ? t.textContent : null, edges: edges,"
            " panel: !!p && !p.hidden, page: f ? f.getAttribute('data-page') : null,"
            " w: (b => b ? b.getBoundingClientRect().width : null)(document.querySelector('#lgm-nodes .lgn[data-id=\"you\"]')),"
            " frame: (r => [r.left + r.width / 2, r.top + r.height / 2, r.width, r.height])(document.getElementById('lgm-svg').getBoundingClientRect())}); })()")

    def map_load(self):
        self.ws.call("Page.navigate", url=self.base + "/map")
        ok = self.wait("document.readyState === 'complete' && !!window.LG_WALKER"
                       " && document.querySelectorAll('#lgm-nodes .lgn').length >= 8"
                       " && !!document.querySelector('.lgm-trail-step.here')", 10)
        time.sleep(0.8)   # the first view fits itself into the frame
        return ok

    def map_walking(self):
        self.drop()
        self.wait("!!document.getElementById('lgm-walker') && !document.getElementById('lgm-walker').hidden", 3)
        time.sleep(0.2)
        return self.j(self.WALK) or {}

    @staticmethod
    def linked(st, a, b):
        return any(set(e) == {a, b} for e in st.get("edges", []))

    def map_desktop(self):
        self.size(1280, 800, False)
        ok = self.map_load()
        plain = self.j(self.WALK) or {}
        st = self.map_walking()
        time.sleep(0.6)   # the camera glides in
        st = self.j(self.WALK) or {}
        zoom = (self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS.mapZoom) || null)") or 0)
        grow = (st.get("w") or 0) / (plain.get("w") or 1)
        self.check("M4 desktop: Drop zooms toward the walker (you grows about %sx)" % zoom,
                   bool(zoom) and 1.25 <= grow <= zoom * 1.05, {"plain w": plain.get("w"), "drop w": st.get("w"), "grow": grow})
        self.check("M0 desktop: the map is drawn and the walker stands on you", ok and st.get("at") == "you", st)
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        letters = {a: k for k, a in (C.get("keys") or {}).items() if len(k) == 1 and a in WAY}
        opposite = {"up": "down", "down": "up", "left": "right", "right": "left"}
        for act in ("right", "left", "up", "down"):
            k = letters.get(act)
            before = self.j(self.WALK) or {}
            self.key(k or "?")
            time.sleep(0.35)
            after = self.j(self.WALK) or {}
            dx, dy = WAY[act]
            # measured from you in the same snapshot: the camera moves the whole map while it follows the walker
            way = bool(after.get("you") and after.get("mid")) and \
                ((after["mid"][0] - after["you"][0]) * dx + (after["mid"][1] - after["you"][1]) * dy) > 0
            self.check("M1 desktop: %r walks from you along a connector to a box %s of it" % (k, act),
                       bool(k) and before.get("at") == "you" and after.get("at") not in (None, "you")
                       and self.linked(after, "you", after["at"]) and way, {"before": before.get("at"), "after": after.get("at"),
                                                                            "mids": (after.get("you"), after.get("mid"))})
            if act == "right":
                time.sleep(0.5)   # the camera glides after the walker
                fol = self.j(self.WALK) or {}
                fr, mid = fol.get("frame") or [0, 0, 1, 1], fol.get("mid") or [-9999, -9999]
                self.check("M4 desktop: the camera follows the walker to the next box (it is near the frame's middle)",
                           abs(mid[0] - fr[0]) <= 0.12 * fr[2] and abs(mid[1] - fr[1]) <= 0.12 * fr[3], {"mid": mid, "frame": fr})
                self.shot("map-desktop", {"x": 0, "y": 0, "width": 1280, "height": 800})
            self.key(letters.get(opposite[act]) or "?")
            time.sleep(0.35)
            back = self.j(self.WALK) or {}
            self.check("M1 desktop: %r walks back to you" % letters.get(opposite[act]), back.get("at") == "you", back.get("at"))
        sel = sorted(kk for kk, a in C.get("keys", {}).items() if a == "select")
        bk = sorted(kk for kk, a in C.get("keys", {}).items() if a == "back")
        # stand on a box that has a page (Events has none yet): try each way from you, walking back after a miss
        target, page, st = None, None, {}
        for act in ("right", "left", "up", "down"):
            self.key(letters.get(act) or "?")
            time.sleep(0.35)
            st = self.j(self.WALK) or {}
            target = st.get("at")
            page = (m2.NODES.get(target) or {}).get("page")
            if page:
                break
            self.key(letters.get(opposite[act]) or "?")
            time.sleep(0.35)
        self.check("M2 desktop: positive control, the side panel is closed and the box has a page", not st.get("panel") and bool(page),
                   {"at": target, "page": page, "panel": st.get("panel")})
        for kk in sel:
            self.key(kk)
        self.wait("!document.getElementById('lgm-panel').hidden", 5)
        time.sleep(0.4)
        st = self.j(self.WALK) or {}
        self.check("M2 desktop: select (%s) opens the box you stand on" % "/".join(sel),
                   bool(sel) and st.get("panel") and st.get("page") == page, {"want": page, "got": st.get("page"), "panel": st.get("panel")})
        for kk in bk:
            self.key(kk)
        self.wait("document.getElementById('lgm-panel').hidden", 3)
        st = self.j(self.WALK) or {}
        self.check("M2 desktop: back (%s) closes it" % "/".join(bk), bool(bk) and not st.get("panel"), st.get("panel"))
        self.drop()   # Drop off
        time.sleep(0.8)
        self.ws.call("Page.navigate", url=self.base + "/map")   # the same first view, to compare the zoom with
        self.map_load()
        self.drop()
        time.sleep(0.6)
        self.drop()   # and off again
        time.sleep(0.6)
        off = self.j(self.WALK) or {}
        self.check("M4 desktop: leaving Drop restores the normal zoom", plain.get("w") and off.get("w")
                   and abs(off["w"] - plain["w"]) <= 0.03 * plain["w"] and not off.get("at"), {"plain w": plain.get("w"), "after w": off.get("w")})

    def tap(self, selector):
        b = self.j("(() => { const e = document.querySelector(%s); if (!e) return 'null'; const r = e.getBoundingClientRect();"
                   " return JSON.stringify([r.left + r.width / 2, r.top + r.height / 2]); })()" % json.dumps(selector))
        if not b:
            return False
        for t in ("mousePressed", "mouseReleased"):
            self.ws.call("Input.dispatchMouseEvent", type=t, x=b[0], y=b[1], button="left", clickCount=1)
            time.sleep(0.08)
        return True

    def map_phone(self):
        self.size(390, 844, True)
        ok = self.map_load()
        st = self.map_walking()
        self.check("M0 phone: the map is drawn and the walker stands on you", ok and st.get("at") == "you", st)
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        B = C.get("buttons") or {}
        moved = None
        for act in ("down", "right", "left", "up"):
            self.tap("#lg-here-pad ." + (B.get(act) or {}).get("cls", "none"))
            time.sleep(0.35)
            st = self.j(self.WALK) or {}
            if st.get("at") not in (None, "you"):
                moved = (act, st)
                break
        self.check("M3 phone: the D-pad walks from you along a connector to a connected box",
                   bool(moved) and self.linked(moved[1], "you", moved[1]["at"]), moved)
        target = moved[1]["at"] if moved else None
        self.shot("map-phone", {"x": 0, "y": 0, "width": 390, "height": 844})
        self.tap("#lg-here-pad ." + (B.get("select") or {}).get("cls", "none"))
        label = (m2.NODES.get(target) or {}).get("label", "?")
        self.wait("(document.querySelector('.lgm-trail-step.here') || {}).textContent === %s" % json.dumps(label[:21]), 4)
        time.sleep(0.3)
        st = self.j(self.WALK) or {}
        self.check("M3 phone: A does what a tap does (the box you stand on becomes the center)",
                   bool(target) and (st.get("trail") or "").startswith(label[:15]), {"want": label, "trail": st.get("trail")})
        self.tap("#lg-here-pad ." + (B.get("back") or {}).get("cls", "none"))
        self.wait("(document.querySelector('.lgm-trail-step.here') || {}).textContent === 'You'", 4)
        st = self.j(self.WALK) or {}
        self.check("M3 phone: B goes back (the trail is You again)", st.get("trail") == "You", st.get("trail"))


    # ---- the portal
    def portal(self):
        self.size(1280, 800, False)
        self.ws.call("Page.navigate", url=self.base + "/u/pal?near=portal")
        self.wait("document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")
        self.drop()
        time.sleep(0.4)
        q = self.j("(() => { const P = LG_WALKER.CONTROLS.portal, e = document.querySelector(P.selector), m = e && e.querySelector('.mark');"
                   " return JSON.stringify({cls: !!e && e.classList.contains(P.cls), near: !!e && e.classList.contains('lg-here-near'),"
                   " round: m ? getComputedStyle(m).borderRadius : null, r: e ? (r => [r.left, r.top, r.right, r.bottom])(e.getBoundingClientRect()) : null}); })()") or {}
        self.check("Q1 the portal is round and marked when you stand by it", q.get("cls") and q.get("near") and q.get("round") == "50%", q)
        if q.get("r"):
            l, t, rr, b = q["r"]
            self.shot("portal", {"x": 0, "y": 0, "width": max(320, rr + 120), "height": b + 30})
        C = self.j("JSON.stringify(LG_WALKER.CONTROLS)") or {}
        for kk in sorted(k for k, a in C.get("keys", {}).items() if a == "select"):
            self.key(kk)
        self.wait("location.pathname === '/map' && !!document.getElementById('lgm-walker')"
                  " && document.getElementById('lgm-walker').getAttribute('data-at') === 'user:pal'", 10)
        time.sleep(0.5)
        st = self.j("(() => { const w = document.getElementById('lgm-walker'), d = document.querySelector('#lg-here-pad .' + LG_WALKER.CONTROLS.buttons.drop.cls);"
                    " return JSON.stringify({path: location.pathname, at: w && !w.hidden ? w.getAttribute('data-at') : null,"
                    " dropping: d ? d.getAttribute('aria-pressed') : null}); })()") or {}
        self.check("Q2 select by the portal: the map, dropped in, at this page's box (user:pal)",
                   st.get("path") == "/map" and st.get("at") == "user:pal" and st.get("dropping") == "true", st)

def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(os.path.join(platform, "assets"), mlc.page_markup(platform)))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-walker-controls-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.phone()
        r.desktop()
        r.map_desktop()
        r.map_phone()
        r.portal()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT walker-controls: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
