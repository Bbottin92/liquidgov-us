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
  T4  one tip at a time (LG_TIP): while one shows, another waits for a later page view without counting it
  C2  on the map the switch sits at the end of the map's own row (after the trail; no Back or Zoom out buttons, Brandon
      5:01 AM), so the map keeps its place on the page (no strip at the top there)
  C3  the map's nav row (Brandon 4:26 AM: the switch squeezed the trail's "You" to "ou"): no Back or Zoom out button,
      the switch does not meet the trail, the trail shows its whole first step, and the row stays one line (the map
      keeps its place); at 390x844 and 1280x800
  Map zoom and back without buttons (Brandon 5:01 AM), in Scroll mode:
  Z1  1280x800: the mouse wheel over the map zooms in toward the pointer (the box under it stays under it) and out
      again, the page does not scroll, and the zoom stops at LG_MAP.ZOOM.max / never goes under ZOOM.min
  Z2  390x844: a two-finger pinch spreads the map (scale grows with the fingers); one finger on the empty map still
      pans without zooming
  Z3  the trail marks the box you came from (lgm-trail-prev, "Back to ..."); clicking it goes back there
  A1  the switch's labels meet WCAG AA (4.5:1) against what is behind them, the chosen one and the other, on the
      profile-like page and inside the map's dark frame; the colors are the site's --lg-switch-* variables
  T5  a first-time tip's close button (x) does not cover its text: the walker's note, the home page's note and a
      Juketube tip (#jbhint, long text)
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
  Home page (Brandon 4:03 AM: Drop as the home page's hook; genesis_body's own markup, with tip.js + walker.js +
  home_drop.js), phone 390x844:
  H0  it starts as usual: the switch marks Scroll, no drop on the page, and the switch pulses (CONTROLS.home.invite,
      a running animation) to invite a tap; the first-time note ("Stop, Drop & Scroll") shows under the switch
  H1  tapped, the pulse stops, the switch marks Drop, your drop appears just under the switch (and the sticky header:
      nothing covers it) and a guide
      (CONTROLS.home.guide.text) points down, under the drop
  H2  walking down keeps the guide with you until the scrolling-words section (CONTROLS.home.belt.after); past it the
      guide is gone and the moving sidewalk (CONTROLS.home.belt.cls) covers the floor from that section's bottom
  H3  on the sidewalk, walking with it (belt.dir) is about (1 + boost) x your speed off it, against it about (1 - boost) x
  H4  A (Enter) with nothing next to you: a squish-and-bounce (the drop's CONTROLS.hop animation and a splat mark run;
      data-hops counts it); nothing is clicked and the page stays
  H5  A next to a link still uses it (positive control for H4: the link's page opens)
  H6  reduced motion: no pulse, guide, sidewalk or hop animation, and the drop stays round while walking; A with
      nothing next to you still counts (no animation)
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


def home_markup(platform):
    """genesis_body's markup (src/site/features/pages/genesis.cpp): its return expression with each named piece filled
    from the literals it is built of (the ticker from its problems list, doubled as the page does); scripts left out
    (the harness loads the assets itself)."""
    src = open(os.path.join(platform, "src", "site", "features", "pages", "genesis.cpp"), encoding="utf-8").read()
    lit = lambda t: re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t"}.get(m.group(1), m.group(1)), t)
    parts = {}
    for name, expr in re.findall(r'(?:std::string|const char\*) (\w+) =\s*(.*?);\n', src, re.S):
        parts[name] = expr
    problems = [lit(p) for p in re.findall(r'"((?:[^"\\]|\\.)*)"', src[src.index("problems = {"):src.index("};", src.index("problems = {"))])]
    filled = {"ticker": "".join("<div>%s</div>" % p for p in problems * 2)}

    def build(expr):
        out = []
        for q, ident in re.findall(r'"((?:[^"\\]|\\.)*)"|\b([A-Za-z_]\w*)\b', expr):
            if q or not ident:
                out.append(lit(q))
            elif ident in filled:
                out.append(filled[ident])
            elif ident in parts and ident not in ("stats",):
                filled[ident] = build(parts[ident])
                out.append(filled[ident])
        return "".join(out)
    a = src.index("return \"<div style")
    html = build(src[a:src.index(";\n}", a)])
    return re.sub(r"<script.*?</script>", "", html, flags=re.S)


def home_page(markup):
    # the logged-out home page: genesis_body + the sign-up card the route adds; scripts in genesis_body's order
    return ("<!doctype html><html lang=en><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'><title>Home</title>"
            "<link rel=stylesheet href=/assets/site.css></head><body>"
            "<header class=top><div class=topinner><a class=brand href=/><div><div class=word><strong>LiquidGov</strong>"
            "</div></div></a><label class=navbtn>Menu</label></div></header><main class=wrap>%s"
            "<div class=card><p class=row><a class=btn href=/signup>Create an account</a> <a class='btn ghost' href=/login>Log in</a></p></div>"
            "</main><footer class=site>Not a government.</footer>"
            "<script>window.LG_CLICKS = 0; document.addEventListener('click', function () { LG_CLICKS++; }, true);</script>"
            "<script src=/assets/tip.js defer></script><script src=/assets/walker.js defer></script>"
            "<script src=/assets/home_drop.js defer></script></body></html>" % markup)


def make_handler(assets, markup, home=""):
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
            if path == "/home":
                return self.reply(200, "text/html", home_page(home).encode())
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
# The Center button's label against its background (WCAG relative luminance), disarmed and armed; the site's colors.
CONTRAST = r"""(() => { const D = LG_WALKER.CONTROLS.buttons.center, b = document.querySelector('#lg-here-pad .' + D.cls);
  const lum = c => { const [r, g, bb] = c.match(/[\d.]+/g).map(Number).slice(0, 3).map(v => { v /= 255; return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }); return 0.2126 * r + 0.7152 * g + 0.0722 * bb; };
  const ratio = (a, c) => { const x = lum(a), y = lum(c); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
  const out = {opacity: getComputedStyle(b).opacity, states: {},
    vars: ['--lg-switch-bg', '--lg-switch-ink', '--lg-switch-on-bg', '--lg-switch-on-ink'].map(v => getComputedStyle(document.documentElement).getPropertyValue(v).trim())};
  const was = b.getAttribute('aria-pressed');
  ['false', 'true'].forEach(v => { b.setAttribute('aria-pressed', v); const c = getComputedStyle(b);
    out.states[v] = {color: c.color, back: c.backgroundColor, ratio: Math.round(ratio(c.color, c.backgroundColor) * 100) / 100}; });
  b.setAttribute('aria-pressed', was);
  return JSON.stringify(out); })()"""
# A tip's close button against its text (sel: the tip).
TIPCLEAR = r"""((sel) => { const t = document.querySelector(sel); if (!t || getComputedStyle(t).display === 'none') return JSON.stringify(null);
  const x = t.querySelector('.lg-tip-close'), txt = t.firstElementChild, box = e => { const r = e.getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom}; };
  const a = box(txt), c = box(x), T = box(t);
  return JSON.stringify({text: a, close: c, tip: T, overlap: a.l < c.r && c.l < a.r && a.t < c.b && c.t < a.b, inside: c.l >= T.l && c.r <= T.r && c.t >= T.t && c.b <= T.b}); })"""
# The map's nav row: the trail (no Back, Zoom out or switch).
NAVROW = r"""(() => { const q = s => document.querySelector(s), box = e => { if (!e) return null; const r = e.getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom}; };
  const tr = q('#lgm-trail'), row = q('.lgm-navrow');
  return JSON.stringify({trail: box(tr), row: box(row), sy: scrollY,
    gone: !q('#lgm-back') && !q('#lgm-zoomout') && !q('.lg-here-drop') && !q('.lg-here-walk') && !q('.lg-here-dropbar'),
    children: row ? row.children.length : null,
    trailText: tr ? tr.textContent : null, trailFits: !!tr && tr.scrollWidth <= tr.clientWidth + 1}); })()"""
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
        """Walking is always on (2026-11-15): nothing to tap. Kept for the checks that call it: waits for the pad."""
        self.wait("!!document.querySelector('#lg-here-pad .lg-here-keys')", 3)

    def tips(self):
        """T4/T5: tip.js on a page with the walker (the walker has no note of its own any more)."""
        self.wait("!!window.LG_TIP", 3)
        self.j("(() => { const d = document.createElement('div'); d.id = 'jbhint'; d.className = 'lg-tip';"
               " d.innerHTML = '<span>Juketube — search, stations, history. Tap a track and everyone on that station hears it.</span>"
               "<button type=\"button\" id=\"jbhintclose\" class=\"lg-tip-close\" aria-label=\"Hide this tip\">×</button>';"
               " document.body.appendChild(d); return '1'; })()")
        self.tip_clear("phone, Juketube's tip", "#jbhint")
        self.j("(() => { document.getElementById('jbhint').remove(); return '1'; })()")
        two = self.j("(() => { const mk = () => { const d = document.createElement('div'); d.innerHTML = '<span></span><button></button>'; document.body.appendChild(d); return d; };"
                     " ['t4a', 't4b'].forEach(n => { localStorage.removeItem(LG_TIP.flag(n)); localStorage.removeItem(LG_TIP.flag(n) + '-views'); });"
                     " const a = mk(), b = mk(), ta = LG_TIP.wire(a, a.lastChild, 't4a', {}), tb = LG_TIP.wire(b, b.lastChild, 't4b', {});"
                     " const r = {first: ta.shown, second: tb.shown, secondViews: localStorage.getItem(LG_TIP.flag('t4b') + '-views')};"
                     " ta.hide(false); a.remove(); b.remove(); return JSON.stringify(r); })()") or {}
        self.check("T4 one tip at a time: while one shows, another waits (its view isn't counted)",
                   two.get("first") is True and two.get("second") is False and two.get("secondViews") is None, two)

    def nav_row(self, tag, shot=None):
        n = self.j(NAVROW) or {}
        self.check("C3 %s: no Back, Zoom out or Drop/Scroll switch; the map's nav row is the trail alone and it shows its "
                   "whole first step" % tag,
                   n.get("gone") is True and bool(n.get("trail")) and n.get("children") == 1 and n.get("trailFits") is True, n)
        if shot and n.get("row"):
            r = n["row"]
            self.shot(shot, {"x": 0, "y": n.get("sy", 0) + max(0, r["t"] - 8), "width": self.j("JSON.stringify(innerWidth)") or 390,
                             "height": r["b"] - r["t"] + 16})
        return n

    def contrast(self, tag):
        c = self.j(CONTRAST) or {}
        st = c.get("states") or {}
        self.check("A1 %s: the Center button's label meets WCAG AA 4.5:1, disarmed and armed (site colors --lg-switch-*)" % tag,
                   len(st) == 2 and all(v.get("ratio", 0) >= 4.5 for v in st.values()) and c.get("opacity") == "1" and all(c.get("vars") or [""]),
                   c)

    def tip_clear(self, tag, sel):
        t = self.j("(%s)(%s)" % (TIPCLEAR.strip(), json.dumps(sel)))
        self.check("T5 %s: the tip's close button does not cover its text" % tag, bool(t) and t.get("overlap") is False and t.get("inside") is True, t)
        return t

    def phone(self):
        self.size(390, 844, True)
        self.goto("/walk")
        self.j("(() => { localStorage.removeItem(LG_WALKER.CONTROLS.buttons.center.store); return '1'; })()")
        self.goto("/walk")
        self.drop()
        time.sleep(0.3)
        none = self.j("JSON.stringify({sw: !!document.querySelector('.lg-here-drop,.lg-here-walk,.lg-here-state,.lg-here-dropbar,.lg-here-tip'),"
                      " padTop: document.body.style.paddingTop, keysHidden: document.querySelector('#lg-here-pad .lg-here-keys').hidden,"
                      " drop: !!(LG_WALKER.CONTROLS.buttons || {}).drop})") or {}
        self.check("N1 phone: there is no Drop/Scroll switch, strip or note; the pad is on the page from the start",
                   none.get("sw") is False and none.get("drop") is False and none.get("padTop") in ("", None) and none.get("keysHidden") is False, none)
        b = self.j(BOXES) or {}
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        acts = set((C.get("keys") or {}).values())
        self.check("K0 the controls map binds every direction, select and back to keys and buttons, and names Center",
                   bool(C) and set(C.get("dirs", [])) == set(WAY) and set(WAY) | {"select", "back"} <= acts
                   and all(a in C.get("buttons", {}) for a in ["select", "back", "center"])
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
        parts = {"touchpad": dp, "select": btn.get("select"), "back": btn.get("back"), "center": btn.get("center"), "juketube": b.get("jb")}
        names = list(parts)
        clash = [(x, y) for i, x in enumerate(names) for y in names[i + 1:] if overlaps(parts[x], parts[y])]
        self.check("P3 phone: no two controls overlap (touchpad, A, B, Center button, Juketube button)",
                   all(parts.values()) and not clash, {"overlaps": clash, "boxes": parts})
        ctl = dict(btn, touchpad=dp)
        small = {k: (r["w"], r["h"]) for k, r in ctl.items() if r and (r["w"] < 44 or r["h"] < 44)}
        self.check("P4 phone: every control is at least 44x44 px", len(ctl) >= 4 and all(ctl.values()) and not small,
                   {"small": small, "count": len(ctl)})
        self.contrast("phone")
        self.tips()
        self.shot("phone", {"x": 0, "y": 0, "width": vw, "height": vh})
        self.touchpad_pull(C, dp, vw, vh)
        self.drop_shape(C)

    CENTER = ("(() => { const b = document.querySelector('#lg-here-pad .' + LG_WALKER.CONTROLS.buttons.center.cls),"
              " d = document.querySelector('#lg-here .lg-here-dot.me .' + LG_WALKER.CONTROLS.shape.cls), r = d.getBoundingClientRect();"
              " return JSON.stringify({pressed: b.getAttribute('aria-pressed'), stored: localStorage.getItem(LG_WALKER.CONTROLS.buttons.center.store),"
              " y: r.top + r.height / 2, x: r.left + r.width / 2, vh: innerHeight, vw: innerWidth, sy: scrollY,"
              " pageY: r.top + r.height / 2 + scrollY, maxSy: document.documentElement.scrollHeight - innerHeight}); })()")

    def center_cases(self):
        """CE0-CE4 and SC1: the Center button and plain scrolling, on the profile-like page (phone 390x844)."""
        self.size(390, 844, True)
        self.goto("/walk")
        self.j("(() => { localStorage.removeItem(LG_WALKER.CONTROLS.buttons.center.store); return '1'; })()")
        self.goto("/walk")
        self.drop()
        time.sleep(0.4)
        c0 = self.j(self.CENTER) or {}
        self.check("CE0 a first visit: the Center button is disarmed", c0.get("pressed") == "false" and c0.get("stored") in (None, "0"), c0)
        self.key("s", 2.0)
        time.sleep(0.3)
        c1 = self.j(self.CENTER) or {}
        self.check("CE1 disarmed: walking down scrolls the page only to keep the dot in view (it is on the screen, not in the middle)",
                   c1.get("sy", 0) > 0 and 0 < c1.get("y", -1) < c1.get("vh", 0) and abs(c1["y"] - c1["vh"] / 2) > 80, c1)
        self.j("(() => { window.scrollBy(0, -260); return '1'; })()")
        time.sleep(0.8)
        c2 = self.j(self.CENTER) or {}
        self.check("SC1 scrolling by hand works as always: the page stays where you scrolled it and your dot stays where it stood",
                   abs(c2.get("sy", 0) - (c1.get("sy", 0) - 260)) <= 2 and abs(c2.get("pageY", 0) - c1.get("pageY", -99)) <= 2,
                   {"before": c1, "after": c2})
        self.tap("#lg-here-pad ." + (self.j("JSON.stringify(LG_WALKER.CONTROLS.buttons.center.cls)") or "none"))
        time.sleep(0.5)
        c3 = self.j(self.CENTER) or {}
        self.check("CE2 armed: the screen slides at once so the dot is in its middle (top to bottom)",
                   c3.get("pressed") == "true" and c3.get("stored") == "1" and abs(c3.get("y", 0) - c3.get("vh", 0) / 2) <= 12, c3)
        self.key("s", 0.6)
        time.sleep(0.3)
        c4 = self.j(self.CENTER) or {}
        self.key("w", 0.3)
        time.sleep(0.3)
        c5 = self.j(self.CENTER) or {}
        self.check("CE2 armed: after every move the dot is in the middle again (down, then up)",
                   c4.get("pageY", 0) > c3.get("pageY", 0) + 40 and abs(c4["y"] - c4["vh"] / 2) <= 12
                   and c5.get("pageY", 0) < c4.get("pageY", 0) - 20 and abs(c5["y"] - c5["vh"] / 2) <= 12, {"down": c4, "up": c5})
        self.shot("center-armed-phone", {"x": 0, "y": 0, "width": 390, "height": 844})
        self.goto("/walk")
        self.drop()
        time.sleep(0.3)
        c6 = self.j(self.CENTER) or {}
        self.check("CE3 the choice is remembered on this device: armed after a reload", c6.get("pressed") == "true", c6)
        self.tap("#lg-here-pad ." + (self.j("JSON.stringify(LG_WALKER.CONTROLS.buttons.center.cls)") or "none"))
        time.sleep(0.3)
        self.key("s", 2.0)
        time.sleep(0.3)
        c7 = self.j(self.CENTER) or {}
        self.goto("/walk")
        self.drop()
        c8 = self.j(self.CENTER) or {}
        self.check("CE4 disarmed again: walking is as before (not held in the middle), and that is remembered too",
                   c7.get("pressed") == "false" and abs(c7.get("y", 0) - c7.get("vh", 0) / 2) > 80 and c8.get("pressed") == "false",
                   {"walked": c7, "reloaded": c8})

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
        self.ws.call("Input.dispatchMouseEvent", type="mouseReleased", x=cx + dx, y=cy + dy, button="left", buttons=0, clickCount=1)
        time.sleep(0.1)      # a tick already on its way at the let-go may still land
        held = self.j(DOT)
        time.sleep(0.3)      # ~7 ticks: a dot that keeps walking would move 7 px a tick here
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
        expr = "(() => { const e = document.querySelector(%s); return JSON.stringify(e ? {speed:+e.dataset.speed, angle:+e.dataset.angle, tail:e.firstChild.style.transform, transform:e.style.transform, tailSide:(t => { const a=e.getBoundingClientRect(), b=t.getBoundingClientRect(); return Math.round((b.left+b.right)/2-(a.left+a.right)/2); })(e.firstChild), vw: innerWidth, at: (r => [Math.round(r.left), Math.round(r.right)])(e.getBoundingClientRect()), img:!!e.querySelector('img[src^=\"/portrait/\"],.lg-here-ph')} : null); })()" % json.dumps(sel)
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
            for _ in range(12):     # the walk is timed, so finish the way back in short taps (either way)
                now = self.j(DOT)
                if not (before and now):
                    break
                off = (now[0] - before[0]) * dx + (now[1] - before[1]) * dy
                if abs(off) <= 6:
                    break
                self.key(back_k if off > 0 else k, 0.05)
                time.sleep(0.1)
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
            " const at = w && !w.hidden ? w.getAttribute('data-at') : null, wr = w ? w.getBoundingClientRect() : null;"
            " const mids = {}; document.querySelectorAll('#lgm-nodes .lgn').forEach(b => { mids[b.getAttribute('data-id')] = mid(b.getAttribute('data-id')); });"
            " const s = document.getElementById('lgm-svg').getBoundingClientRect();"
            " const c = document.querySelector('#lg-here-pad .' + LG_WALKER.CONTROLS.buttons.center.cls), k = document.querySelector('#lg-here-pad .lg-here-keys');"
            " const padTop = k && k.getBoundingClientRect().height && c ? c.getBoundingClientRect().top : Infinity;"
            " const top = Math.max(s.top, 0), bot = Math.min(s.bottom, innerHeight, padTop - 8 > top + 80 ? padTop - 8 : Infinity);"
            " return JSON.stringify({at: at, mid: mid(at), you: mid('you'), mids: mids, trail: t ? t.textContent : null, edges: edges,"
            " panel: !!p && !p.hidden, page: f ? f.getAttribute('data-page') : null,"
            " pos: w && !w.hidden ? [+w.getAttribute('data-x'), +w.getAttribute('data-y')] : null,"
            " spot: wr && !w.hidden ? [wr.left + wr.width / 2, wr.top + wr.height / 2] : null,"
            " near: w ? w.getAttribute('data-near') : null, scale: LG_MAP.scale(), fit: LG_MAP.fitScale ? LG_MAP.fitScale() : null, zmax: LG_MAP.ZOOM.max,"
            " shown: [...document.querySelectorAll('#lgm-nodes .lgn')].filter(b => b.style.visibility !== 'hidden' && b.style.display !== 'none').map(b => b.getAttribute('data-id')),"
            " all: document.querySelectorAll('#lgm-nodes .lgn').length,"
            " seen: [(Math.max(s.left, 0) + Math.min(s.right, innerWidth)) / 2, (top + bot) / 2],"
            " w: (b => b ? b.getBoundingClientRect().width : null)(document.querySelector('#lgm-nodes .lgn[data-id=\"you\"]')),"
            " frame: [s.left + s.width / 2, s.top + s.height / 2, s.width, s.height]}); })()")

    def map_load(self):
        self.ws.call("Page.navigate", url=self.base + "/map")
        ok = self.wait("document.readyState === 'complete' && !!window.LG_WALKER"
                       " && document.querySelectorAll('#lgm-nodes .lgn').length >= 8"
                       " && !!document.querySelector('.lgm-trail-step.here')", 10)
        self.wait("!!document.getElementById('lgm-walker') && !document.getElementById('lgm-walker').hidden", 5)
        time.sleep(1.2)   # the first view fits itself into the frame, then the walker's zoom glides in
        return ok

    def map_walking(self):
        """Walking is always on (2026-11-15): the map's state once the walker stands on it."""
        self.wait("!!document.getElementById('lgm-walker') && !document.getElementById('lgm-walker').hidden", 5)
        time.sleep(0.2)
        return self.j(self.WALK) or {}

    @staticmethod
    def linked(st, a, b):
        return any(set(e) == {a, b} for e in st.get("edges", []))

    def map_go(self, target, limit_s=14, keys=None):
        """Walk the map's walker to box `target` with the keys (free walking): short holds toward its middle until the
        walker is in it (data-at) or the time is up. Returns the last state."""
        keys = keys or {"up": "w", "down": "s", "left": "a", "right": "d"}
        end = time.time() + limit_s
        st = self.j(self.WALK) or {}
        while time.time() < end:
            st = self.j(self.WALK) or {}
            to, spot = (st.get("mids") or {}).get(target), st.get("spot")
            if st.get("at") == target or not to or not spot:
                return st
            dx, dy = to[0] - spot[0], to[1] - spot[1]
            if abs(dx) >= abs(dy):
                self.key(keys["right" if dx > 0 else "left"], max(0.05, min(0.4, abs(dx) / 175.0)))
            else:
                self.key(keys["down" if dy > 0 else "up"], max(0.05, min(0.4, abs(dy) / 175.0)))
            time.sleep(0.08)
        return self.j(self.WALK) or {}

    def map_pull_to(self, target, tp, reach, limit_s=14):
        """The same with the touchpad: pulls straight toward the box."""
        end = time.time() + limit_s
        st = self.j(self.WALK) or {}
        while time.time() < end:
            st = self.j(self.WALK) or {}
            to, spot = (st.get("mids") or {}).get(target), st.get("spot")
            if st.get("at") == target or not to or not spot or not tp:
                return st
            dx, dy = to[0] - spot[0], to[1] - spot[1]
            n = (dx * dx + dy * dy) ** 0.5 or 1
            self.pull(tp, dx / n * reach, dy / n * reach, max(0.08, min(0.4, n / 175.0)))
        return self.j(self.WALK) or {}

    def map_desktop(self):
        self.size(1280, 800, False)
        ok = self.map_load()
        st = self.map_walking()
        zoom = (self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS.mapZoom) || null)") or 0)
        want = min(st.get("zmax") or 0, (st.get("fit") or 0) * zoom)
        self.check("M0 desktop: the map is drawn and the walker stands on you, with no switch to tap", ok and st.get("at") == "you", st.get("at"))
        sp, sn = st.get("spot") or [-9999, -9999], st.get("seen") or [0, 0]
        self.check("M4 desktop: the map starts zoomed in around the walker (%sx the fitted view; the walker in the middle of "
                   "the part of the map you can see)" % zoom,
                   bool(zoom) and abs((st.get("scale") or 0) - want) <= 0.03 * want and want > (st.get("fit") or 9) * 1.2
                   and abs(sp[0] - sn[0]) <= 12 and abs(sp[1] - sn[1]) <= 12,
                   {"scale": st.get("scale"), "fit": st.get("fit"), "want": want, "walker": sp, "middle": sn})
        self.nav_row("map desktop")
        m_expr = "(() => { const e=document.querySelector('#lgm-walker .' + LG_WALKER.CONTROLS.shape.cls); return JSON.stringify(e ? {speed:+e.dataset.speed, img:!!e.querySelector('img[src^=\"/portrait/\"],.lg-here-ph')} : null); })()"
        self.ws.call("Input.dispatchKeyEvent", type="keyDown", key="d", windowsVirtualKeyCode=68)
        time.sleep(0.15)
        mm = self.j(m_expr) or {}
        self.ws.call("Input.dispatchKeyEvent", type="keyUp", key="d", windowsVirtualKeyCode=68)
        time.sleep(0.45)
        ms = self.j(m_expr) or {}
        self.check("S4 map: the walker is your portrait drop, teardrop while it walks and a circle after", mm.get("img") and mm.get("speed") == 1 and ms.get("speed") == 0, {"moving": mm, "stopped": ms})
        self.key("a", 0.15)
        time.sleep(0.2)
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        letters = {a: k for k, a in (C.get("keys") or {}).items() if len(k) == 1 and a in WAY}
        opposite = {"up": "down", "down": "up", "left": "right", "right": "left"}
        top = C.get("maxSpeed") or 0
        for act in ("right", "left", "up", "down"):
            k = letters.get(act)
            before = self.j(self.WALK) or {}
            t0 = time.time()
            self.key(k or "?", 0.3)
            ticks = (time.time() - t0) / 0.04
            time.sleep(0.12)
            after = self.j(self.WALK) or {}
            dx, dy = WAY[act]
            b0, a0 = before.get("pos") or [0, 0], after.get("pos") or [0, 0]
            along = ((a0[0] - b0[0]) * dx + (a0[1] - b0[1]) * dy) * (after.get("scale") or 1)   # screen px
            across = abs((a0[0] - b0[0]) * dy) + abs((a0[1] - b0[1]) * dx)
            self.check("M1 desktop: %r walks the walker %s freely, at about maxSpeed (%s px a tick), not from box to box" % (k, act, top),
                       bool(k) and 0.5 * top * ticks <= along <= 1.4 * top * (ticks + 3) and across < 1,
                       {"before": b0, "after": a0, "px": along, "ticks": ticks})
            self.key(letters.get(opposite[act]) or "?", 0.3)
            time.sleep(0.12)
        # onto a connected box and back: stepping onto a box puts you in it
        first = self.j(self.WALK) or {}
        ring = [i for i in (first.get("shown") or []) if i != "you" and self.linked(first, "you", i)]
        to = self.map_go(ring[0]) if ring else {}
        self.check("M1 desktop: walking onto a connected box puts you in it (the walker stands in %s)" % (ring[0] if ring else "?"),
                   bool(ring) and to.get("at") == ring[0], {"ring": ring, "at": to.get("at")})
        self.shot("map-desktop", {"x": 0, "y": 0, "width": 1280, "height": 800})
        home = self.map_go("you")
        self.check("M1 desktop: and walking back puts you in you again", home.get("at") == "you", home.get("at"))
        # the near filter (zoomed in) and your own zoom out (the wheel)
        sx, sy = (home.get("frame") or [640, 400])[0], (home.get("frame") or [640, 400])[1]
        hid = self.j(self.WALK) or {}
        self.check("M5 desktop: zoomed in, only the box you are at and the boxes connected to it are shown",
                   hid.get("near") == "on" and set(hid.get("shown") or []) == {"you"} | {i for i in (hid.get("mids") or {}) if self.linked(hid, "you", i)},
                   {"near": hid.get("near"), "shown": hid.get("shown")})
        to = self.map_go(ring[0]) if ring else {}
        nb = {ring[0]} | {i for i in (to.get("mids") or {}) if self.linked(to, ring[0], i)} if ring else set()
        self.check("M5 desktop: standing in %s, boxes not connected to it are hidden (positive control: some are)" % (ring[0] if ring else "?"),
                   bool(ring) and set(to.get("shown") or []) == nb and len(to.get("shown") or []) < (to.get("all") or 0),
                   {"shown": to.get("shown"), "all": to.get("all")})
        for _ in range(8):
            self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=sx, y=sy, deltaX=0, deltaY=100)
            time.sleep(0.05)
        time.sleep(0.4)
        out = self.j(self.WALK) or {}
        self.key("d", 0.4)
        time.sleep(1.4)   # longer than the walker waits before it zooms back in by itself
        still = self.j(self.WALK) or {}
        self.check("M5 desktop: the wheel zooms out, then every box is shown, and walking on does not zoom back in",
                   out.get("near") == "off" and len(out.get("shown") or []) == out.get("all") and (out.get("scale") or 9) < 0.8 * want
                   and still.get("near") == "off" and abs((still.get("scale") or 0) - (out.get("scale") or -1)) < 1e-6,
                   {"zoomed out": [out.get("scale"), out.get("near"), len(out.get("shown") or []), out.get("all")],
                    "after walking": [still.get("scale"), still.get("near")]})
        for _ in range(12):
            self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=sx, y=sy, deltaX=0, deltaY=-100)
            time.sleep(0.05)
        time.sleep(0.4)
        back_in = self.j(self.WALK) or {}
        self.check("M5 desktop: the wheel zooms back in, and the far boxes are hidden again",
                   back_in.get("near") == "on" and len(back_in.get("shown") or []) < (back_in.get("all") or 0), {"near": back_in.get("near"), "shown": back_in.get("shown")})
        sel = sorted(kk for kk, a in C.get("keys", {}).items() if a == "select")
        bk = sorted(kk for kk, a in C.get("keys", {}).items() if a == "back")
        # stand on a box that has a page (Events has none yet)
        self.ws.call("Page.navigate", url=self.base + "/map")
        self.map_load()
        st = self.j(self.WALK) or {}
        paged = [i for i in (st.get("shown") or []) if i != "you" and (m2.NODES.get(i) or {}).get("page")]
        target = paged[0] if paged else None
        page = (m2.NODES.get(target) or {}).get("page")
        st = self.map_go(target) if target else st
        self.check("M2 desktop: positive control, you stand in a box that has a page and the side panel is closed",
                   bool(page) and st.get("at") == target and not st.get("panel"), {"at": st.get("at"), "want": target, "page": page, "panel": st.get("panel")})
        for kk in sel[:1]:
            self.key(kk)
        self.wait("!document.getElementById('lgm-panel').hidden", 5)
        time.sleep(0.4)
        st = self.j(self.WALK) or {}
        self.check("M2 desktop: select (%s) opens the box you stand in" % "/".join(sel[:1]),
                   bool(sel) and st.get("panel") and st.get("page") == page, {"want": page, "got": st.get("page"), "panel": st.get("panel")})
        for kk in bk[:1]:
            self.key(kk)
        self.wait("document.getElementById('lgm-panel').hidden", 3)
        st = self.j(self.WALK) or {}
        self.check("M2 desktop: back (%s) closes it" % "/".join(bk[:1]), bool(bk) and not st.get("panel"), st.get("panel"))
        time.sleep(2.2)   # opening a box makes the map fit itself again (zoomed out); the walker's zoom comes back by itself
        again = self.j(self.WALK) or {}
        self.check("M6 desktop: when the map zooms out by itself (a box was opened), the walking zoom comes back",
                   again.get("near") == "on" and (again.get("scale") or 0) >= 0.8 * min(again.get("zmax") or 0, (again.get("fit") or 0) * zoom),
                   {"scale": again.get("scale"), "fit": again.get("fit"), "near": again.get("near")})

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
        self.check("M0 phone: the map is drawn and the walker stands on you, with no switch to tap", ok and st.get("at") == "you", st.get("at"))
        self.nav_row("map phone", shot="map-navrow-phone")
        C = self.j("JSON.stringify((window.LG_WALKER && LG_WALKER.CONTROLS) || null)") or {}
        B = C.get("buttons") or {}
        tp = (self.j(BOXES) or {}).get("touchpad")
        reach = (C.get("touchpad") or {}).get("reach", 0)
        # Center disarmed (the default): a short walk moves the walker over a map that stays where it is
        a0 = self.j(self.WALK) or {}
        if tp:
            self.pull(tp, reach, 0, 0.3)
        time.sleep(0.2)
        a1 = self.j(self.WALK) or {}
        s0, s1, y0, y1 = a0.get("spot") or [0, 0], a1.get("spot") or [0, 0], a0.get("you") or [0, 0], a1.get("you") or [9, 9]
        self.check("MC1 phone, Center disarmed: the touchpad walks the walker across the map; the map itself stays put",
                   bool(tp) and s1[0] - s0[0] > 20 and abs(y1[0] - y0[0]) <= 2 and abs(y1[1] - y0[1]) <= 2, {"walker": [s0, s1], "you box": [y0, y1]})
        self.tap("#lg-here-pad ." + (B.get("center") or {}).get("cls", "none"))
        time.sleep(0.7)
        c0 = self.j(self.WALK) or {}
        if tp:
            self.pull(tp, 0, -reach, 0.4)
        time.sleep(0.3)
        c1 = self.j(self.WALK) or {}
        sp, sn = c1.get("spot") or [-999, -999], c1.get("seen") or [0, 0]
        p0, p1 = c0.get("pos") or [0, 0], c1.get("pos") or [0, 0]
        self.check("MC2 phone, Center armed: the walker walks on and the map slides so it stays in the middle of the part "
                   "of the map you can see (above the pad)",
                   p0[1] - p1[1] > 10 and abs(sp[0] - sn[0]) <= 12 and abs(sp[1] - sn[1]) <= 12
                   and abs(((c0.get("spot") or [0, 0])[1]) - sn[1]) <= 12, {"walked": [p0, p1], "walker": sp, "middle": sn})
        self.shot("map-center-phone", {"x": 0, "y": 0, "width": 390, "height": 844})
        self.tap("#lg-here-pad ." + (B.get("center") or {}).get("cls", "none"))   # disarmed again (it is remembered)
        time.sleep(0.2)
        first = self.j(self.WALK) or {}
        ring = [i for i in (first.get("shown") or []) if i != "you" and self.linked(first, "you", i)
                and (m2.NODES.get(i) or {}).get("label")]
        target = ring[0] if ring else None
        st = self.map_pull_to(target, tp, reach) if target else first
        self.check("M3 phone: the touchpad walks the walker onto a connected box", bool(target) and st.get("at") == target,
                   {"want": target, "at": st.get("at")})
        self.shot("map-phone", {"x": 0, "y": 0, "width": 390, "height": 844})
        self.tap("#lg-here-pad ." + (B.get("select") or {}).get("cls", "none"))
        label = (m2.NODES.get(target) or {}).get("label", "?")
        self.wait("(document.querySelector('.lgm-trail-step.here') || {}).textContent === %s" % json.dumps(label[:21]), 4)
        time.sleep(0.3)
        st = self.j(self.WALK) or {}
        self.check("M3 phone: A does what a tap does (the box you stand in becomes the center)",
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
        self.key("d", 0.05)   # what you stand next to is marked once you move
        self.key("a", 0.05)
        time.sleep(0.2)
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
        st = self.j("(() => { const w = document.getElementById('lgm-walker');"
                    " return JSON.stringify({path: location.pathname, at: w && !w.hidden ? w.getAttribute('data-at') : null}); })()") or {}
        self.check("Q2 select by the portal: the map, with your walker standing in this page's box (user:pal)",
                   st.get("path") == "/map" and st.get("at") == "user:pal", st)

HOME = r"""(() => { const C = LG_WALKER.CONTROLS, H = C.home;
  const box = e => { if (!e || e.hidden) return null; const r = e.getBoundingClientRect(); return r.width && r.height ? {l: r.left, t: r.top, r: r.right, b: r.bottom} : null; };
  const layer = document.getElementById('lg-here'), dot = layer && layer.querySelector('.lg-here-dot.me'), drop = dot && dot.querySelector('.' + C.shape.cls);
  const g = document.querySelector('.' + H.guide.cls), belt = document.querySelector('.' + H.belt.cls), after = document.querySelector(H.belt.after);
  const anim = (e, p) => e ? getComputedStyle(e, p).animationName : null;
  return JSON.stringify({noSwitch: !document.querySelector('.lg-here-drop,.lg-here-walk,.lg-here-dropbar,.lg-here-tip'),
    layerShown: !!layer && !layer.hidden, dot: dot && !layer.hidden ? box(dot) : null, at: dot ? [parseFloat(dot.style.left), parseFloat(dot.style.top)] : null,
    guide: g && !layer.hidden ? box(g) : null, guideText: g ? g.textContent : null, guideAnim: anim(g), want: H.guide.text,
    belt: belt && !layer.hidden ? {top: parseFloat(belt.style.top), h: parseFloat(belt.style.height)} : null, beltAnim: anim(belt, '::before'),
    afterBottom: after ? after.getBoundingClientRect().bottom + scrollY : null, boost: H.belt.boost, dir: H.belt.dir,
    hops: drop ? +(drop.getAttribute('data-hops') || 0) : null, anims: drop ? drop.getAnimations().length : null,
    splat: drop ? !!drop.querySelector('.' + C.hop.splatCls) : null, speed: drop ? drop.getAttribute('data-speed') : null,
    // the dots' layer takes no pointer (elementFromPoint skips it): the drop is in view when it is below the sticky header
    dotSeen: !!dot && !layer.hidden && (() => { const r = dot.querySelector('.' + C.shape.cls).getBoundingClientRect(); return r.top >= document.querySelector('header.top').getBoundingClientRect().bottom && r.bottom <= innerHeight; })(),
    headerB: document.querySelector('header.top').getBoundingClientRect().bottom, sy: scrollY, vw: innerWidth,
    near: !!document.querySelector('.lg-here-near'), clicks: window.LG_CLICKS, path: location.pathname}); })()"""


def _home(self):
    """The home page's hook (H0-H6), phone 390x844. Walking is always on (2026-11-15)."""
    self.size(390, 844, True)
    self.ws.call("Page.navigate", url=self.base + "/home")
    self.wait("document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")
    time.sleep(0.4)
    C = self.j("JSON.stringify(LG_WALKER.CONTROLS)") or {}
    h = self.j(HOME) or {}
    d, g = h.get("dot"), h.get("guide")
    self.check("H0 home: no switch, strip or note: your drop is on the page from the start, under the header, in view",
               h.get("noSwitch") is True and h.get("layerShown") and bool(d) and h.get("dotSeen")
               and 0 < d["t"] - h.get("headerB", 0) <= 220 and abs((d["l"] + d["r"]) / 2 - h.get("vw", 0) / 2) <= 30,
               {k: h.get(k) for k in ("noSwitch", "layerShown", "dot", "dotSeen", "headerB", "vw")})
    self.check("H1 home: a guide points down, under the drop",
               bool(d and g) and g["t"] >= d["t"] + 0.5 * (d["b"] - d["t"]) and h.get("guideText") == h.get("want"),
               {k: h.get(k) for k in ("dot", "guide", "guideText", "want")})
    self.shot("home-dropped-phone", {"x": 0, "y": 0, "width": 390, "height": 420})
    # walk down until past the scrolling words (the guide comes along until then)
    plain, guided = None, True
    for _ in range(80):
        h = self.j(HOME) or {}
        at, belt = h.get("at"), h.get("belt")
        if at and belt and at[1] >= belt["top"] + 260:
            break
        if at and belt and at[1] < belt["top"] and not h.get("guide"):
            guided = False
        if plain is None and at and belt and belt["top"] - at[1] > 260:
            t0 = time.time(); self.key("s", 1.0); dt = time.time() - t0
            h2 = self.j(HOME) or {}
            plain = (h2["at"][1] - at[1]) / dt
            continue
        self.key("s", 0.4)
    h = self.j(HOME) or {}
    at, belt = h.get("at"), h.get("belt")
    self.check("H2 home: the guide stays with you down to the scrolling words; past them it is gone and the sidewalk "
               "covers the floor from the section's bottom",
               guided and bool(at and belt) and at[1] > belt["top"] and not h.get("guide") and belt["h"] > 0
               and abs(belt["top"] - (h.get("afterBottom") or -1e9)) <= 2,
               {"at": at, "belt": belt, "sectionBottom": h.get("afterBottom"), "guide": h.get("guide"), "guidedOnTheWay": guided})
    dy = (h.get("dir") or {}).get("y", 0)
    fwd, back = ("s", "w") if dy >= 0 else ("w", "s")
    t0 = time.time(); a0 = (self.j(HOME) or {}).get("at"); self.key(fwd, 1.0); dt = time.time() - t0; a1 = (self.j(HOME) or {}).get("at")
    with_ = abs(a1[1] - a0[1]) / dt if a0 and a1 else 0
    self.shot("home-belt-phone", {"x": 0, "y": self.j("JSON.stringify(scrollY)") or 0, "width": 390, "height": 844})
    t0 = time.time(); self.key(back, 1.0); dt = time.time() - t0; a2 = (self.j(HOME) or {}).get("at")
    against = abs(a2[1] - a1[1]) / dt if a1 and a2 else 0
    b = h.get("boost") or 0
    self.check("H3 home: on the sidewalk, with it about (1 + %s) x and against it about (1 - %s) x your speed off it" % (b, b),
               bool(plain) and 0.8 * (1 + b) <= with_ / plain <= 1.2 * (1 + b) and 0.6 * (1 - b) <= against / plain <= 1.4 * (1 - b),
               {"px/s off": plain, "with": with_, "against": against, "with/off": with_ / plain if plain else None,
                "against/off": against / plain if plain else None})
    # A with nothing next to you (step aside until nothing is near)
    for _ in range(20):
        if not (self.j(HOME) or {}).get("near"):
            break
        self.key("w", 0.15)
    h0 = self.j(HOME) or {}
    self.key("enter")
    time.sleep(0.12)
    mid = self.j(HOME) or {}
    if mid.get("dot"):
        dd = mid["dot"]
        self.shot("home-hop-phone", {"x": max(0, dd["l"] - 60), "y": (self.j("JSON.stringify(scrollY)") or 0) + dd["t"] - 60, "width": 160, "height": 160})
    time.sleep(0.6)
    h1 = self.j(HOME) or {}
    self.check("H4 home: A with nothing next to you squishes and bounces (animation + splat), clicks nothing, stays on the page",
               not h0.get("near") and h1.get("hops") == (h0.get("hops") or 0) + 1 and (mid.get("anims") or 0) >= 1 and mid.get("splat")
               and h1.get("clicks") == h0.get("clicks") and h1.get("path") == "/home" and not h1.get("splat"),
               {"before": {k: h0.get(k) for k in ("near", "hops", "clicks")}, "mid": {k: mid.get(k) for k in ("anims", "splat")},
                "after": {k: h1.get(k) for k in ("hops", "clicks", "path", "splat")}})
    # A next to a link: walk to "Log in"
    target = "a[href='/login']"
    for _ in range(120):
        st = self.j("(() => { const t = document.querySelector(\"%s\").getBoundingClientRect(), d = document.querySelector('#lg-here .lg-here-dot.me');"
                    " const x = parseFloat(d.style.left) - scrollX, y = parseFloat(d.style.top) - scrollY;"
                    " return JSON.stringify({near: !!document.querySelector(\"%s.lg-here-near\"), dx: (t.left + t.right) / 2 - x, dy: (t.top + t.bottom) / 2 - y}); })()"
                    % (target, target)) or {}
        if st.get("near"):
            break
        if abs(st.get("dy", 0)) > 20:
            self.key("s" if st["dy"] > 0 else "w", 0.12)
        else:
            self.key("d" if st.get("dx", 0) > 0 else "a", 0.12)
    hops = (self.j(HOME) or {}).get("hops")
    self.key("enter")
    self.wait("location.pathname === '/login'", 3)
    self.check("H5 home: A next to a link still uses it (Log in opens; no hop)",
               self.j("JSON.stringify(location.pathname)") == "/login" and hops is not None, {"near": st, "path": self.j("JSON.stringify(location.pathname)")})
    # reduced motion
    self.ws.call("Emulation.setEmulatedMedia", features=[{"name": "prefers-reduced-motion", "value": "reduce"}])
    try:
        self.ws.call("Page.navigate", url=self.base + "/home")
        self.wait("document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")
        time.sleep(0.2)
        self.key("s", 0.5)
        rw = self.j(HOME) or {}
        for _ in range(10):
            if not rw.get("near"):
                break
            self.key("s", 0.2)
            rw = self.j(HOME) or {}
        self.ws.call("Input.dispatchKeyEvent", type="keyDown", key="s", windowsVirtualKeyCode=83)
        time.sleep(0.2)
        mv = self.j(HOME) or {}
        self.ws.call("Input.dispatchKeyEvent", type="keyUp", key="s", windowsVirtualKeyCode=83)
        self.key("enter")
        time.sleep(0.05)
        rh = self.j(HOME) or {}
        self.check("H6 home, reduced motion: no guide, sidewalk or hop animation; the drop stays round walking; A still counts",
                   rw.get("guideAnim") in ("none", None) and rw.get("beltAnim") in ("none", None)
                   and mv.get("speed") == "0.00" and rh.get("hops") == (rw.get("hops") or 0) + 1 and rh.get("anims") == 0,
                   {"guide": rw.get("guideAnim"), "belt": rw.get("beltAnim"), "speed": mv.get("speed"),
                    "hops": [rw.get("hops"), rh.get("hops")], "anims": rh.get("anims")})
    finally:
        self.ws.call("Emulation.setEmulatedMedia", features=[])


Run.home = _home


MAPZ = r"""(() => { const svg = document.getElementById('lgm-svg'), b = svg.getBoundingClientRect(), you = document.querySelector('#lgm-nodes .lgn-you, #lgm-nodes [data-id="you"]') || document.querySelector('#lgm-nodes .lgn');
  const r = you.getBoundingClientRect();
  return JSON.stringify({scale: LG_MAP.scale(), Z: LG_MAP.ZOOM, sy: scrollY, frame: {l: b.left, t: b.top, r: b.right, b: b.bottom},
    you: {x: (r.left + r.right) / 2, y: (r.top + r.bottom) / 2}, dropping: document.getElementById('lgmap').classList.contains('lgm-dropping'),
    trail: [...document.querySelectorAll('#lgm-trail .lgm-trail-step')].map(e => e.textContent),
    prev: (() => { const p = document.querySelector('#lgm-trail .lgm-trail-prev'); return p ? {text: p.textContent, label: p.getAttribute('aria-label'),
      border: getComputedStyle(p).borderTopStyle} : null; })()}); })()"""


def _map_zoom_back(self):
    """Z1-Z3: the map's zoom and back without buttons. Yours at any time now (walking is always on, 2026-11-15)."""
    self.size(1280, 800, False)
    ok = self.map_load()
    z0 = self.j(MAPZ) or {}
    Z = z0.get("Z") or {}
    self.ws.call("Runtime.evaluate", expression="window.scrollTo(0, 0)")
    x, y = z0["you"]["x"], z0["you"]["y"]
    sy0 = self.j("JSON.stringify(scrollY)")
    for _ in range(3):
        self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=-100)
        time.sleep(0.08)
    z1 = self.j(MAPZ) or {}
    stay = abs(z1["you"]["x"] - x) + abs(z1["you"]["y"] - y) if z1.get("you") else 999
    for _ in range(3):
        self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=100)
        time.sleep(0.08)
    z2 = self.j(MAPZ) or {}
    self.check("Z1 desktop: the wheel over the map zooms in toward the pointer and back out; the page does not scroll",
               ok and z1.get("scale", 0) > z0.get("scale", 0) * 1.2 and stay <= 6
               and abs(z2.get("scale", 0) - z0.get("scale", 0)) <= 0.02 * z0.get("scale", 1) and z1.get("sy") == sy0 and z2.get("sy") == sy0,
               {"scales": [z0.get("scale"), z1.get("scale"), z2.get("scale")], "box moved px": stay, "scrollY": [sy0, z1.get("sy"), z2.get("sy")]})
    for _ in range(40):
        self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=-100)
    time.sleep(0.2)
    hi = (self.j(MAPZ) or {}).get("scale")
    for _ in range(80):
        self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=100)
    time.sleep(0.2)
    lo = (self.j(MAPZ) or {}).get("scale")
    self.check("Z1 desktop: the zoom stops at LG_MAP.ZOOM.max and never goes under ZOOM.min",
               bool(Z) and hi is not None and abs(hi - Z["max"]) < 1e-6 and lo is not None and lo >= Z["min"] - 1e-6 and lo < hi,
               {"max": Z.get("max"), "min": Z.get("min"), "reached": [hi, lo]})
    # Z3: go into a ring box, then back by its trail step
    self.map_load()
    self.j("(() => { const g = [...document.querySelectorAll('#lgm-nodes .lgn')].find(e => !e.classList.contains('lgn-you') && e.getAttribute('data-id') !== 'you' && !e.classList.contains('lgn-root'));"
           " g.dispatchEvent(new MouseEvent('click', {bubbles: true})); return '1'; })()")
    self.wait("document.querySelectorAll('#lgm-trail .lgm-trail-step').length >= 2", 5)
    time.sleep(0.6)
    a = self.j(MAPZ) or {}
    self.tap("#lgm-trail .lgm-trail-prev")
    self.wait("document.querySelectorAll('#lgm-trail .lgm-trail-step').length === 1", 5)
    b = self.j(MAPZ) or {}
    p = a.get("prev") or {}
    self.check("Z3 the trail marks the box you came from (a \u2039 pill, 'Back to ...'); clicking it goes back there",
               len(a.get("trail") or []) == 2 and p.get("text") == (a.get("trail") or [None])[0] and (p.get("label") or "").startswith("Back to ")
               and p.get("border") not in (None, "none") and b.get("trail") == (a.get("trail") or [None])[:1],
               {"in": a.get("trail"), "prev": p, "after": b.get("trail")})
    # Z2: pinch on a phone
    self.size(390, 844, True)
    self.map_load()
    self.ws.call("Emulation.setTouchEmulationEnabled", enabled=True, maxTouchPoints=5)
    try:
        f = (self.j(MAPZ) or {}).get("frame")
        cx, cy = (f["l"] + f["r"]) / 2, f["t"] + 0.55 * (f["b"] - f["t"])
        s0 = (self.j(MAPZ) or {}).get("scale")
        pts = lambda d: [{"x": cx - d, "y": cy, "id": 1}, {"x": cx + d, "y": cy, "id": 2}]
        self.ws.call("Input.dispatchTouchEvent", type="touchStart", touchPoints=pts(30))
        for d in (40, 55, 70, 90):
            self.ws.call("Input.dispatchTouchEvent", type="touchMove", touchPoints=pts(d))
            time.sleep(0.04)
        s1 = (self.j(MAPZ) or {}).get("scale")
        self.ws.call("Input.dispatchTouchEvent", type="touchEnd", touchPoints=[])
        time.sleep(0.3)
        want = min((self.j(MAPZ) or {}).get("Z", {}).get("max", 9), (s0 or 0) * 3)
        self.check("Z2 phone: a two-finger pinch spreads the map (about the fingers' spread, 3x here)",
                   bool(s0) and s1 is not None and abs(s1 - want) <= 0.15 * want, {"before": s0, "pinched": s1, "want": want})
        # one finger on the empty map: a pan, no zoom
        e = self.j("(() => { const s = document.getElementById('lgm-svg').getBoundingClientRect(); for (let y = s.top + 12; y < s.bottom; y += 9) for (let x = s.left + 12; x < s.right; x += 9) {"
                   " const h = document.elementFromPoint(x, y); if (h && !h.closest('.lgn') && h.closest('#lgm-svg')) return JSON.stringify([x, y]); } return 'null'; })()")
        s2 = (self.j(MAPZ) or {}).get("scale")
        y0 = (self.j(MAPZ) or {}).get("you")
        if e:
            self.ws.call("Input.dispatchTouchEvent", type="touchStart", touchPoints=[{"x": e[0], "y": e[1], "id": 3}])
            for k in range(1, 5):
                self.ws.call("Input.dispatchTouchEvent", type="touchMove", touchPoints=[{"x": e[0] + 10 * k, "y": e[1], "id": 3}])
                time.sleep(0.03)
            self.ws.call("Input.dispatchTouchEvent", type="touchEnd", touchPoints=[])
            time.sleep(0.2)
        z3 = self.j(MAPZ) or {}
        self.check("Z2 phone: one finger on the empty map still pans, without zooming",
                   bool(e) and abs((z3.get("scale") or 0) - (s2 or -1)) < 1e-6 and bool(y0) and z3["you"]["x"] - y0["x"] > 20,
                   {"at": e, "scale": [s2, z3.get("scale")], "box x": [y0 and y0["x"], z3.get("you", {}).get("x")]})
    finally:
        self.ws.call("Emulation.setTouchEmulationEnabled", enabled=False)


Run.map_zoom_back = _map_zoom_back


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(os.path.join(platform, "assets"), mlc.page_markup(platform),
                                                                                  home_markup(platform)))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-walker-controls-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.phone()
        r.center_cases()
        r.desktop()
        r.map_desktop()
        r.map_phone()
        r.map_zoom_back()
        r.portal()
        r.home()
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
