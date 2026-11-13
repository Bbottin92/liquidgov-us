"""Walker controls (Brandon 2026-11-13, 12:28 AM: "the desktop would be wasd controls and enter to select and esc to go
back. also the button that turns dripping on should be at the top of the screen"; 12:30 AM: "d pad lower left, A and B
buttons lower right. right now theyre all scrunched in the lower left"). New file: no committed check is edited.

Real headless Chromium, NO C++ server (like site_hidden_check.py): a stdlib server serves a profile-like page (site
header, a button, a text field, the Juketube button, the footer) with the real assets/site.css, assets/juketube.css and
assets/walker.js + assets/here.js, and a stand-in for the dots WebSocket that echoes your own dot back. The map cases
serve map_page_body's markup with the real lgmap.js/lgmap.css + walker.js + lgmap_walk.js and map_live_check2.py's
fake graph. Every key and button name comes from walker.js's one controls map (window.LG_WALKER.CONTROLS); this file
keeps no copy of the bindings.

  K0  precondition: the controls map is on the page and binds every direction to keys, select and back to keys and
      buttons, and names the touchpad (reach, dead zone, size, translucency, place) and the top speed
  Phone 390x844 (walking; Brandon 1:08/1:09 AM: a see-through touchpad, A and B attached to its right side, at the
  bottom center in thumb reach):
  P1  the touchpad + A/B unit is centered, in the bottom quarter, lifted CONTROLS.touchpad.lift px off the bottom, and
      fits the screen with its margins
  P2  A and B are attached to the touchpad's right side, stacked, each half its height (together its height)
  U1  the touchpad, A and B are equally see-through (opacity = CONTROLS.touchpad.alpha, under 1)
  P3  no two controls overlap (touchpad, A, B, the Drop button, the Juketube button)
  P4  every control is at least 44x44 px
  J1  the touchpad replaced the D-pad: no direction buttons are left on the pad
  J2  a pull inside the dead zone does not move your dot (positive control: J3 moves it)
  J3  a full pull (from where the finger touched, not the middle) moves the dot its way at about CONTROLS.maxSpeed px
      per 40 ms, faster than a half pull; let go, it stops
  P5  the Drop button (Brandon's name; it turns walking on) is in the top 15% of the screen and does not cover the site header's middle
  P6  the Drop button is a Drop/Scroll switch: it shows both of the map's state names (CONTROLS.buttons.drop.states)
      and marks the one you are in (Scroll before, Drop while walking)
  First-time note (Brandon 2:05/2:07 AM; tip.js, the mechanism Juketube's first-time tip uses):
  T1  a first visit shows the note with the map's text (CONTROLS.buttons.drop.tip.text), styled as Juketube's tip
      (the same .lg-tip look: background, border, corners, shadow, font size as a #jbhint)
  T2  the note sits in the Drop strip under the switch and covers nothing (no link, button or field; not the switch)
  T3  its close button hides it and saves the seen-flag (LG_TIP.flag); the next page view shows no note
  C1  the Drop button covers nothing (Brandon 1:59 AM: on the map it covered MAINstream): its box meets neither the
      portal (CONTROLS.portal), nor the map's MAINstream button (#lgm-main), nor any other link, button or field;
      at 390x844 and 1280x800, on the profile-like page and on the map
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
  M3  390x844: the touchpad moves the walker to a connected box; A does what a tap does (the box becomes the center:
      the trail ends at it); B goes back (the trail is You again)
Usage: LG_BROWSER=$(command -v google-chrome) python3 walker_controls_check.py [<liquidgov-platform dir> [<out dir>]]
Writes <out dir>/phone-walker-controls-*.png. Prints "VERDICT walker-controls: ALL-PASS" or the failed cases.
Exit 1 on a FAIL."""
import base64
import http.server
import json
import re
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
            "%s<script src=/assets/walker.js></script><script src=/assets/here.js></script><script src=/assets/tip.js></script>"
            "</body></html>" % (title, FAKE_WS))


def map_page(markup):
    return ("<!doctype html><html lang=en><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'><title>map</title>"
            "<link rel=stylesheet href=/assets/site.css><link rel=stylesheet href=/assets/lgmap.css>"
            "<link rel=stylesheet href=/assets/juketube.css></head><body>"
            "<header class=top><div class=topinner><a class=brand href=/start><div><div class=word><strong>LiquidGov</strong>"
            "</div></div></a><label class=navbtn>Menu</label></div></header><main class=wrap>%s</main>"
            "<footer class=site>Not a government.</footer><script src=/assets/lgmap.js defer></script>"
            "<script src=/assets/walker.js defer></script><script src=/assets/lgmap_walk.js defer></script>"
            "<script src=/assets/tip.js defer></script>"
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
  const T = C.touchpad || {}; out.touchpad = box(q('#lg-here-pad .' + T.cls)); out.unit = box(q('#lg-here-pad .lg-here-keys'));
  out.alpha = ['.' + T.cls, '.' + C.buttons.select.cls, '.' + C.buttons.back.cls].map(s => { const e = q('#lg-here-pad ' + s); return e ? parseFloat(getComputedStyle(e).opacity) : null; }); out.dirButtons = document.querySelectorAll('#lg-here-pad [data-dir]').length;
  out.buttons = {}; Object.keys(C.buttons).forEach(k => { const e = q('#lg-here-pad .' + C.buttons[k].cls); out.buttons[k] = box(e); if (out.buttons[k]) out.buttons[k].text = e.textContent; });
  out.jb = box(q('#jbtoggle'));
  const h = q('header.top').getBoundingClientRect(), hit = document.elementFromPoint(h.left + h.width / 2, h.top + h.height / 2);
  out.headerHit = !!hit && q('header.top').contains(hit);
  return JSON.stringify(out); })()"""
# The Drop button against the portal, the map's MAINstream button and every other control it could cover.
COVER = r"""(() => { const C = LG_WALKER.CONTROLS, d = document.querySelector('#lg-here-pad .' + C.buttons.drop.cls);
  const box = e => { if (!e) return null; const r = e.getBoundingClientRect(); return r.width && r.height ? {l: r.left, t: r.top, r: r.right, b: r.bottom} : null; };
  const meets = (a, b) => !!a && !!b && a.l < b.r && b.l < a.r && a.t < b.b && b.t < a.b;
  const D = box(d), P = box(document.querySelector(C.portal.selector)), M = box(document.getElementById('lgm-main'));
  const covered = [...document.querySelectorAll('a[href],button,input:not([type=hidden]),select,textarea,summary,[role=button]')]
    .filter(e => !e.closest('#lg-here-pad') && meets(D, box(e))).map(e => e.id || e.className || e.tagName);
  return JSON.stringify({drop: D, portal: P, main: M, portalHit: meets(D, P), mainHit: meets(D, M), covered: covered}); })()"""
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

    def switch_state(self, tag, C, want):
        D = (C.get("buttons") or {}).get("drop") or {}
        st = self.j("(() => { const D = LG_WALKER.CONTROLS.buttons.drop, b = document.querySelector('#lg-here-pad .' + D.cls);"
                    " const s = b ? [...b.querySelectorAll('.' + D.stateCls)] : [];"
                    " return JSON.stringify({names: s.map(e => e.textContent), on: s.filter(e => e.classList.contains('on')).map(e => e.textContent),"
                    " pressed: b && b.getAttribute('aria-pressed')}); })()") or {}
        names = D.get("states") or {}
        self.check("%s the switch shows %r and %r (the map's names) and marks %r" % (tag, names.get("on"), names.get("off"), names.get(want)),
                   bool(names) and st.get("names") == [names.get("on"), names.get("off")] and st.get("on") == [names.get(want)]
                   and st.get("pressed") == ("true" if want == "on" else "false"), st)

    TIP = r"""(() => { const P = LG_WALKER.CONTROLS.buttons.drop.tip, t = document.querySelector('.' + P.cls);
  const box = e => { if (!e) return null; const r = e.getBoundingClientRect(); return r.width && r.height ? {l: r.left, t: r.top, r: r.right, b: r.bottom} : null; };
  const meets = (a, b) => !!a && !!b && a.l < b.r && b.l < a.r && a.t < b.b && b.t < a.b;
  const look = e => { const c = getComputedStyle(e); return [c.backgroundColor, c.borderTopColor, c.borderTopWidth, c.borderRadius, c.boxShadow, c.fontSize, c.display]; };
  const ref = document.createElement('div'); ref.id = 'jbhint'; ref.className = 'lg-tip'; ref.style.visibility = 'hidden'; document.body.appendChild(ref);
  const want = look(ref); ref.remove();
  const T = t && t.style.display !== 'none' ? box(t) : null, bar = box(document.querySelector('#lg-here-pad .' + LG_WALKER.CONTROLS.buttons.drop.bar));
  const covered = T ? [...document.querySelectorAll('a[href],button,input:not([type=hidden]),select,textarea,summary,[role=button]')]
    .filter(e => !t.contains(e) && meets(T, box(e))).map(e => e.id || e.className || e.tagName) : null;
  return JSON.stringify({shown: !!T, text: t ? t.firstChild.textContent : null, want: P.text, look: t ? look(t) : null, jbLook: want,
    box: T, bar: bar, covered: covered, flag: window.LG_TIP ? localStorage.getItem(LG_TIP.flag(P.name)) : 'no LG_TIP'}); })()"""

    def first_tip(self):
        self.wait("!!document.querySelector('.lg-here-tip')", 3)
        t = self.j(self.TIP) or {}
        self.check("T1 phone: a first visit shows the note with the map's text", t.get("shown") and t.get("text") == t.get("want"),
                   {"text": t.get("text"), "want": t.get("want")})
        self.check("T1 phone: the note looks like Juketube's first-time tip (.lg-tip)", bool(t.get("look")) and t.get("look") == t.get("jbLook"),
                   {"note": t.get("look"), "juketube tip": t.get("jbLook")})
        bx, bar = t.get("box"), t.get("bar")
        self.check("T2 phone: the note sits in the Drop strip and covers nothing",
                   bool(bx and bar) and bx["b"] <= bar["b"] and t.get("covered") == [], {"note": bx, "strip": bar, "covered": t.get("covered")})
        if bar:
            self.shot("drop-tip-phone", {"x": 0, "y": 0, "width": 390, "height": bar["b"] + 70})
        self.j("(() => { document.querySelector('.' + LG_WALKER.CONTROLS.buttons.drop.tip.cls + ' .lg-tip-close').click(); return '1'; })()")
        time.sleep(0.2)
        t2 = self.j(self.TIP) or {}
        self.check("T3 phone: closing hides the note and saves its seen-flag", not t2.get("shown") and t2.get("flag") == "1", t2)
        self.goto("/walk")
        time.sleep(0.5)
        t3 = self.j(self.TIP) or {}
        self.check("T3 phone: the next page view shows no note", not t3.get("shown"), t3)

    def covers_nothing(self, tag, need_main=False):
        c = self.j(COVER) or {}
        self.check("C1 %s: the Drop button covers nothing (not the portal%s, no other control)" % (tag, ", not MAINstream" if need_main else ""),
                   bool(c.get("drop")) and bool(c.get("portal")) and (bool(c.get("main")) or not need_main)
                   and not c.get("portalHit") and not c.get("mainHit") and not c.get("covered"), c)
        return c

    def drop_at_top(self, tag, b):
        t = (b.get("buttons") or {}).get("drop")
        self.check("%s the Drop button is in the top 15%% of the screen and the header's middle is uncovered" % tag,
                   bool(t) and t["b"] <= 0.15 * b["vh"] and t["t"] >= 0 and b.get("headerHit"), {"drop": t, "vh": b.get("vh"),
                                                                                                   "headerHit": b.get("headerHit")})

    def phone(self):
        self.size(390, 844, True)
        self.goto("/walk")
        self.first_tip()
        self.switch_state("P6 phone: before walking,", self.j("JSON.stringify(LG_WALKER.CONTROLS)") or {}, "off")
        self.drop()
        time.sleep(0.3)
        b = self.j(BOXES) or {}
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        acts = set((C.get("keys") or {}).values())
        self.check("K0 the controls map binds every direction, select and back to keys and buttons",
                   bool(C) and set(C.get("dirs", [])) == set(WAY) and set(WAY) | {"select", "back"} <= acts
                   and all(a in C.get("buttons", {}) for a in ["select", "back", "drop"])
                   and (C.get("maxSpeed") or 0) > 0 and all((C.get("touchpad") or {}).get(k, 0) > 0
                                                            for k in ("reach", "width", "height", "alpha", "lift", "margin"))
                   and 0 < (C.get("touchpad") or {}).get("deadZone", 0) < 1, C)
        vw, vh, btn = b.get("vw", 0), b.get("vh", 0), b.get("buttons") or {}
        dp, u, T = b.get("touchpad"), b.get("unit"), C.get("touchpad") or {}
        self.check("P1 phone: the touchpad + A/B unit is centered, in the bottom quarter, %spx off the bottom, inside %spx margins"
                   % (T.get("lift"), T.get("margin")),
                   bool(u) and abs((u["l"] + u["r"]) / 2 - vw / 2) <= 2 and u["t"] > 0.75 * vh
                   and abs((vh - u["b"]) - T.get("lift", -99)) <= 2 and u["l"] >= T.get("margin", 0) - 1
                   and u["r"] <= vw - T.get("margin", 0) + 1, {"unit": u, "vw": vw, "vh": vh})
        a, bb = btn.get("select"), btn.get("back")
        self.check("P2 phone: A and B are attached to the touchpad's right side, stacked, each half its height",
                   bool(dp and a and bb) and abs(a["l"] - dp["r"]) <= 1 and abs(bb["l"] - dp["r"]) <= 1
                   and abs(a["t"] - dp["t"]) <= 1 and abs(bb["b"] - dp["b"]) <= 1 and abs(a["b"] - bb["t"]) <= 1
                   and abs(a["h"] - dp["h"] / 2) <= 1 and abs(bb["h"] - dp["h"] / 2) <= 1, {"touchpad": dp, "A": a, "B": bb})
        al = b.get("alpha") or []
        self.check("U1 phone: the touchpad, A and B are equally see-through (opacity %s)" % T.get("alpha"),
                   len(al) == 3 and all(x is not None and abs(x - T.get("alpha", 9)) < 0.01 for x in al) and al[0] < 1, al)
        self.check("J1 phone: the touchpad replaced the D-pad (no direction buttons on the pad)", b.get("dirButtons") == 0, b.get("dirButtons"))
        parts = {"touchpad": dp, "select": btn.get("select"), "back": btn.get("back"), "drop": btn.get("drop"), "juketube": b.get("jb")}
        names = list(parts)
        clash = [(x, y) for i, x in enumerate(names) for y in names[i + 1:] if overlaps(parts[x], parts[y])]
        self.check("P3 phone: no two controls overlap (touchpad, A, B, Drop button, Juketube button)",
                   all(parts.values()) and not clash, {"overlaps": clash, "boxes": parts})
        ctl = dict(btn, touchpad=dp)
        small = {k: (r["w"], r["h"]) for k, r in ctl.items() if r and (r["w"] < 44 or r["h"] < 44)}
        self.check("P4 phone: every control is at least 44x44 px", len(ctl) >= 4 and all(ctl.values()) and not small,
                   {"small": small, "count": len(ctl)})
        self.drop_at_top("P5 phone:", b)
        self.covers_nothing("phone")
        self.switch_state("P6 phone: walking,", C, "on")
        self.shot("phone", {"x": 0, "y": 0, "width": vw, "height": vh})
        self.touchpad_pull(C, dp, vw, vh)
        self.drop_shape(C)

    def pull(self, tp, dx, dy, hold_s, shot=None):
        """Touch the touchpad left of its middle (the pull counts from there, not the middle), drag by (dx, dy) px, hold, let go; returns how far your dot went (x, y)."""
        cx, cy = tp["l"] + 0.4 * (tp["r"] - tp["l"]), (tp["t"] + tp["b"]) / 2
        before = self.j(DOT)
        t0 = time.time()
        self.ws.call("Input.dispatchMouseEvent", type="mousePressed", x=cx, y=cy, button="left", buttons=1, clickCount=1)
        for i in range(1, 5):
            self.ws.call("Input.dispatchMouseEvent", type="mouseMoved", x=cx + dx * i / 4, y=cy + dy * i / 4, button="left", buttons=1)
        time.sleep(hold_s)
        mid = self.j(DOT)
        self.ticks = (time.time() - t0) / 0.04   # the walk ticks every 40 ms; measured, so a slow CDP call doesn't skew speed
        if shot:
            shot()
        held = self.j(DOT)   # where it was at the let-go
        self.ws.call("Input.dispatchMouseEvent", type="mouseReleased", x=cx + dx, y=cy + dy, button="left", buttons=0, clickCount=1)
        time.sleep(0.25)
        after = self.j(DOT)
        went = (mid[0] - before[0], mid[1] - before[1]) if before and mid else None
        settled = (after[0] - held[0], after[1] - held[1]) if held and after else None
        return went, settled

    def touchpad_pull(self, C, tp, vw, vh):
        S, top = C.get("touchpad") or {}, C.get("maxSpeed") or 0
        if not tp or not S:
            self.check("J2 phone: the touchpad is there to pull", False, {"touchpad": tp, "map": S})
            return
        reach, dz = S["reach"], S["deadZone"]
        dead, _ = self.pull(tp, reach * dz * 0.6, 0, 0.5)
        self.check("J2 phone: a pull inside the dead zone (%.0f px of %s) does not move the dot" % (reach * dz * 0.6, reach),
                   dead is not None and abs(dead[0]) < 1 and abs(dead[1]) < 1, dead)
        half_px = reach * (dz + (1 - dz) * 0.5)
        half, _ = self.pull(tp, -half_px, 0, 0.8)   # left, then right: the dot ends where it began
        half_ticks = self.ticks
        full, stop = self.pull(tp, reach * 1.5, 0, 0.8,
                               shot=lambda: self.shot("touchpad", {"x": 0, "y": vh - 230, "width": vw, "height": 230}))
        # speed: px per 40 ms tick over the hold (0.8 s = 20 ticks)
        sh, sf = (abs(half[0]) / half_ticks if half else 0), (full[0] / self.ticks if full else 0)
        self.check("J3 phone: a full pull walks the dot its way at about maxSpeed (%s px a tick)" % top,
                   bool(full) and full[0] > 0 and 0.6 * top <= sf <= 1.25 * top and abs(full[1]) < 2, {"full": full, "px/tick": sf})
        self.check("J3 phone: a half pull walks slower than a full one (speed grows with the pull)",
                   bool(half) and half[0] < 0 and 0.25 * sf <= sh <= 0.75 * sf, {"half": half, "px/tick": sh, "full px/tick": sf})
        self.check("J3 phone: let go, the dot stops", stop is not None and abs(stop[0]) < 1 and abs(stop[1]) < 1, stop)

    def drop_shape(self, C):
        S = C.get("shape") or {}

        def sx(t):   # the scaleX in a transform string (the browser writes 0.9 for 0.900)
            mm = re.search(r"scaleX\(([-0-9.]+)\)", t or "")
            return float(mm.group(1)) if mm else -1
        sel = "." + S.get("cls", "missing")
        expr = "(() => { const e = document.querySelector(%s); return JSON.stringify(e ? {speed:+e.dataset.speed, angle:+e.dataset.angle, tail:e.firstChild.style.transform, transform:e.style.transform, tailSide:(t => { const a=e.getBoundingClientRect(), b=t.getBoundingClientRect(); return Math.round((b.left+b.right)/2-(a.left+a.right)/2); })(e.firstChild), vw: innerWidth, at: (r => [Math.round(r.left), Math.round(r.right)])(e.getBoundingClientRect()), img:!!e.querySelector('img[src^=\"/portrait/\"],.lg-walker-ph')} : null); })()" % json.dumps(sel)
        stopped = self.j(expr) or {}
        key = next((k for k,a in C.get("keys", {}).items() if a == "left" and len(k) == 1), "?")   # away from the button: open page behind the tail
        self.ws.call("Input.dispatchKeyEvent", type="keyDown", key=key)
        time.sleep(0.2)
        moving = self.j(expr) or {}
        d = self.j("(() => { const e=document.querySelector(%s),r=e.getBoundingClientRect(); return JSON.stringify({x:Math.max(0,r.left-150),y:Math.max(0,r.top-40),width:Math.min(260,innerWidth-Math.max(0,r.left-150)),height:r.height+95}); })()" % json.dumps(sel))
        if d:
            self.shot("drop-moving", d)
        self.ws.call("Input.dispatchKeyEvent", type="keyUp", key=key)
        time.sleep(0.4)
        after = self.j(expr) or {}
        self.check("S1 your drop is your portrait, circular stopped; at speed it stretches and the tail is opposite motion",
                   stopped.get("img") and stopped.get("speed") == 0 and moving.get("speed") == 1 and moving.get("angle") == 180
                   and "rotate(360deg)" in (moving.get("tail") or "") and (moving.get("tailSide") or 0) > 0 and abs(sx(moving.get("tail")) - S.get("tail", -9)) < 0.01
                   and abs(sx(moving.get("transform")) - 1 - S.get("stretch", -9)) < 0.01 and sx(stopped.get("tail")) == 0,
                   {"stopped": stopped, "moving": moving})
        self.check("S2 stopped, the drop returns to its circle", after.get("speed") == 0 and sx(after.get("tail")) == 0, after)
        self.ws.call("Emulation.setEmulatedMedia", features=[{"name":"prefers-reduced-motion","value":"reduce"}])
        self.j("(() => { const e=document.querySelector(%s); LG_WALKER.shape(e,1,0); return '1'; })()" % json.dumps(sel))
        reduced = self.j(expr) or {}
        self.check("S3 reduced motion: a circle, no stretch", reduced.get("speed") == 0, reduced)
        self.ws.call("Emulation.setEmulatedMedia", features=[])

    def desktop(self):
        self.size(1280, 800, False)
        self.goto("/start")
        self.j("(() => { location.href = '/walk'; return '1'; })()")
        time.sleep(0.3)
        self.wait("location.pathname === '/walk' && document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")
        self.drop()
        b = self.j(BOXES) or {}
        self.drop_at_top("D1 desktop:", b)
        self.covers_nothing("desktop")
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
        self.covers_nothing("map desktop", need_main=True)
        m_expr = "(() => { const e=document.querySelector('#lgm-walker .' + LG_WALKER.CONTROLS.shape.cls); return JSON.stringify(e ? {speed:+e.dataset.speed, img:!!e.querySelector('img[src^=\"/portrait/\"],.lg-walker-ph')} : null); })()"
        self.key("d", 0.02)
        time.sleep(0.05)
        mm = self.j(m_expr) or {}
        time.sleep(0.45)
        ms = self.j(m_expr) or {}
        self.check("S4 map: the walker is your portrait drop, teardrop on a step and circle after", mm.get("img") and mm.get("speed") == 1 and ms.get("speed") == 0, {"moving": mm, "stopped": ms})
        self.key("a", 0.02)
        time.sleep(0.45)
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
        c = self.covers_nothing("map phone", need_main=True)
        if c.get("main"):   # the map's top: the Drop strip, the header with the portal, MAINstream
            self.shot("map-top-phone", {"x": 0, "y": 0, "width": 390, "height": min(844, c["main"]["b"] + 24)})
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        B = C.get("buttons") or {}
        moved = None
        tp = (self.j(BOXES) or {}).get("touchpad")
        reach = (C.get("touchpad") or {}).get("reach", 0)
        for act in ("down", "right", "left", "up"):
            if not tp:
                break
            self.pull(tp, WAY[act][0] * reach, WAY[act][1] * reach, 0.25)   # one push: under mapRepeatMs, one connector
            time.sleep(0.2)
            st = self.j(self.WALK) or {}
            if st.get("at") not in (None, "you"):
                moved = (act, st)
                break
        self.check("M3 phone: a push on the touchpad walks from you along a connector to a connected box",
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
