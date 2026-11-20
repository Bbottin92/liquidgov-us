"""Drawing canvas layout, /feed/canvas (Brandon 2026-11-11 4:37 PM, Brief 4 item 7; site-look2 FINDINGS 1: the
drawing surface used under half the card, controls scattered on three sides, sideways clipping at 250%).
The drawing surface fills the card's width at its 4:3 shape (large on a phone too); the controls are one toolbar above
it, in four groups in this order: tools | size, opacity, colour | layers | undo, redo, clipboard; at narrow widths it
wraps with nothing cut off sideways.

Real headless Chromium with the REAL site stylesheet (css() cut from src/http_server.cpp) and canvas_tool_page()'s own
markup, cut from the source with its few variables filled in as for /feed/canvas (640 x 480). The page's script is not
run (the layout is CSS and markup). NO C++ server. Widths 390, 768 and 1280.

  C1  the drawing surface (#ccanvaswrap) is at least 95% of the canvas card's inner width, and 4:3 (+-2%)
  C2  390: the drawing surface is at least 300 px wide
  C3  one [role=toolbar] holds the tools, size, opacity, colour, the layer buttons, undo/redo and copy/paste; it sits
      wholly above the drawing surface
  C4  the toolbar's groups (.ctgroup), in order: tools | properties | layers | edit (undo, clipboard)
  C5  no sideways clipping: the page has no sideways scroll and every control is inside the window
Studio look (Brandon 2026-11-19: "more esthetic and professional"; picked "Studio, same warm palette", toolbar and canvas
card only; q232). Same page, same widths; these fail on the pre-studio markup and CSS:
  S1  every tool button (.toolbtn) is round: corner radius at least half its height (and there are 11 of them)
  S2  the toolbar is one soft bar: corner radius at least 16 px and a filled background
  S3  size and opacity have range sliders (#csizer, #copacityr) that are shown, and the two wheel pairs are not
  S4  the drawing surface has soft corners: radius at least 12 px
  S5  390, with the real assets/canvas.js run on the page: moving a slider sets its number box (#csize, #copacity), and typing
      in a number box moves its slider (the number boxes stay: they are what canvas.js reads)
Usage: LG_BROWSER=$(command -v google-chrome) python3 canvas_layout_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT canvas-layout: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed checks unchanged."""
import base64
import http.server
import json
import os
import re
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import juketube_overlap_check as jo  # noqa: E402  (site_css; sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

VARS = {"cws": "640", "chs": "480", "aspect": "640/480", "title": "Compose a post", "errHtml": "",
        "intro": "Draw, crop, layer, paste in a photo, or drop text on it &mdash; then post it, caption included."}


def tool_markup(platform):
    """canvas_tool_page()'s markup up to the end of the canvas card, as /feed/canvas renders it."""
    src = __import__("server_source").read(platform)  # whole server source (http_server.cpp + its src/site pieces)
    a = src.index("std::string canvas_tool_page(")
    a = src.index("std::string html =", a)
    b = src.index("if (!portrait) {", a)
    code = "\n".join(l for l in src[a:b].splitlines() if not l.strip().startswith("//"))
    # the REAL icons (q236: the old stand-in boxes made every screenshot show black squares): each `const std::string kIconX =
    # "..." "...";` before the markup, kept as its C++ literals so the one read below turns them into the svg text
    start = src.rindex("std::string canvas_tool_page(", 0, a)
    icons = {m.group(1): m.group(2) for m in re.finditer(
        r'const std::string (kIcon\w+) =\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', "\n".join(
            l for l in src[start:a].splitlines() if not l.strip().startswith("//")))}
    # a variable between two + is filled in (its icon, or its value); then the literals are read in order
    def fill(m):
        name = m.group(1)
        if name in icons:
            return "+" + icons[name] + "+"
        return '+"' + VARS.get(name, "") + '"+'
    code = re.sub(r"\+\s*([A-Za-z_]\w*)\s*\+", fill, code)
    code = re.sub(r"\+\s*([A-Za-z_]\w*)\s*\+", fill, code)  # neighbours that shared a +
    return "".join(p.encode().decode("unicode_escape") for p in re.findall(r'"((?:[^"\\]|\\.)*)"', code))


def page(platform):
    return ("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width'>"
            "<style>" + jo.site_css(platform) + "</style></head><body>"
            "<header class='top'><div class='topinner'><a class='brand' href='/'>LiquidGov</a></div></header>"
            "<main class='wrap'>" + tool_markup(platform) + "</main><footer class='site'>footer</footer></body></html>")


def make_handler(html):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(html.encode())

    return H


PROBE = r"""(() => {
  const r = e => { const b = e.getBoundingClientRect(); return {l: b.left, t: b.top, r: b.right, b: b.bottom, w: b.width, h: b.height}; };
  const frame = document.querySelector('.canvas-frame'), wrap = document.getElementById('ccanvaswrap');
  const cs = getComputedStyle(frame);
  const inner = frame.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight);
  const bar = document.querySelector('.canvas-frame [role=toolbar]');
  const need = ['.toolbtn[data-tool=pen]', '#csize', '#copacity', '#ccolor', '.layerbtn', '#cundo', '#credo', '#ccopy', '#cpaste'];
  const missing = bar ? need.filter(s => !bar.querySelector(s)) : need;
  const groups = bar ? [...bar.querySelectorAll(':scope > .ctgroup')].map(g => {
    const has = s => !!g.querySelector(s);
    return has('.toolbtn') ? 'tools' : has('#csize') ? 'props' : has('.layerbtn') ? 'layers' : has('#cundo') ? 'edit' : '?'; }) : [];
  const ctrls = [...frame.querySelectorAll('button,input,select,.wheel')].filter(e => e.getClientRects().length && getComputedStyle(e).visibility !== 'hidden')
    .map(e => ({id: e.id || e.className || e.tagName, ...r(e)}));
  return {W: innerWidth, sw: document.documentElement.scrollWidth, inner: inner, wrap: r(wrap), bar: bar ? r(bar) : null,
          missing: missing, groups: groups, outside: ctrls.filter(c => c.l < -1 || c.r > innerWidth + 1).map(c => c.id)};
})()"""


PROBE_S = r"""(() => {
  const rad = e => { const b = e.getBoundingClientRect(); return {r: parseFloat(getComputedStyle(e).borderTopLeftRadius) || 0, h: b.height, w: b.width}; };
  const slider = id => { const e = document.getElementById(id); return e ? {type: e.type, shown: e.getClientRects().length > 0} : null; };
  const bar = document.querySelector('.canvas-frame [role=toolbar]');
  return {tools: [...document.querySelectorAll('.canvas-frame .toolbtn')].map(rad),
          bar: bar ? Object.assign(rad(bar), {bg: getComputedStyle(bar).backgroundColor}) : null,
          size: slider('csizer'), op: slider('copacityr'),
          wheelsShown: [...document.querySelectorAll('.canvas-frame .wheel')].some(e => e.getClientRects().length > 0),
          wrap: rad(document.getElementById('ccanvaswrap'))};
})()"""

SYNC_JS = r"""(() => {
  window.LG_CANVAS = {px: 1, frames: 0, user: 't', mode: 'portrait', postTarget: ''};
  if (!document.getElementById('csave')) { const b = document.createElement('button'); b.id = 'csave'; document.body.appendChild(b); }
  %s
  const fire = (e, v) => { e.value = v; e.dispatchEvent(new Event('input', {bubbles: true})); };
  const g = id => document.getElementById(id), out = {};
  fire(g('csizer'), 20); out.sizeFromSlider = g('csize').value;
  fire(g('csize'), 12); out.sliderFromSize = g('csizer').value;
  fire(g('copacityr'), 55); out.opFromSlider = g('copacity').value;
  fire(g('copacity'), 70); out.sliderFromOp = g('copacityr').value;
  return out;
})()"""


class Run:
    def __init__(self, base, out):
        self.base, self.out, self.fails, self.n = base, out, [], 0
        self.platform = PLATFORM

    def check(self, name, ok, detail):
        self.n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:500]), flush=True)
        if not ok:
            self.fails.append(name)

    def cases(self):
        for w, h in ((1280, 900), (768, 1000), (390, 844)):
            self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=1, mobile=w < 500)
            self.ws.call("Page.navigate", url=self.base + "/feed/canvas")
            time.sleep(1.2)
            with open(os.path.join(self.out, "canvas-%d.png" % w), "wb") as f:
                f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png", captureBeyondViewport=True)["data"]))
            p = json.loads(cdp.js(self.ws, "JSON.stringify(%s)" % PROBE))
            ww, wh = p["wrap"]["w"], p["wrap"]["h"]
            self.check("C1 %d: the drawing surface fills the card width at 4:3" % w,
                       ww >= 0.95 * p["inner"] and abs(wh / max(ww, 1) - 0.75) <= 0.015, {"wrap": p["wrap"], "inner": p["inner"]})
            if w == 390:
                self.check("C2 390: the drawing surface is at least 300 px wide", ww >= 300, p["wrap"])
            self.check("C3 %d: one toolbar holds every control group and sits above the drawing surface" % w,
                       p["bar"] is not None and not p["missing"] and p["bar"]["b"] <= p["wrap"]["t"] + 1,
                       {"bar": p["bar"], "missing": p["missing"], "wrap": p["wrap"]})
            self.check("C4 %d: the groups are tools | props | layers | edit" % w,
                       p["groups"] == ["tools", "props", "layers", "edit"], p["groups"])
            self.check("C5 %d: nothing is cut off sideways" % w, p["sw"] <= p["W"] and not p["outside"],
                       {"sw": p["sw"], "W": p["W"], "outside": p["outside"][:8]})
            s = json.loads(cdp.js(self.ws, "JSON.stringify(%s)" % PROBE_S))
            self.check("S1 %d: all 11 tool buttons are round" % w,
                       len(s["tools"]) == 11 and all(t["r"] >= t["h"] / 2 - 0.5 for t in s["tools"]), s["tools"][:3])
            self.check("S2 %d: the toolbar is one soft, filled bar" % w,
                       s["bar"] is not None and s["bar"]["r"] >= 16 and s["bar"]["bg"] not in ("rgba(0, 0, 0, 0)", "transparent"), s["bar"])
            self.check("S3 %d: size and opacity are range sliders and the wheels are gone" % w,
                       bool(s["size"] and s["size"]["type"] == "range" and s["size"]["shown"] and s["op"] and s["op"]["type"] == "range"
                            and s["op"]["shown"]) and not s["wheelsShown"], {"size": s["size"], "op": s["op"], "wheels": s["wheelsShown"]})
            self.check("S4 %d: the drawing surface has soft corners" % w, s["wrap"]["r"] >= 12, s["wrap"])
            if w == 390:
                src = open(os.path.join(self.platform, "assets", "canvas.js"), encoding="utf-8").read()
                try:
                    y = json.loads(cdp.js(self.ws, "JSON.stringify(" + (SYNC_JS % src) + ")"))
                except Exception as e:  # the script threw or a slider is missing: the case fails with the reason, never skipped
                    y = {"error": "%s: %s" % (type(e).__name__, e)}
                self.check("S5 390: sliders and number boxes follow each other",
                           y.get("sizeFromSlider") == "20" and y.get("sliderFromSize") == "12" and y.get("opFromSlider") == "55"
                           and y.get("sliderFromOp") == "70", y)
        self.fs_cases()

    def fs_cases(self):
        """Fullscreen editor on a phone (Brandon 2026-11-19: tools bunched at the top and the canvas below the fold; wants the
        canvas to fill the screen with floating buttons like Sketchbook, pinch zoom, two-finger pan, no scrolling away or Back by
        accident). 390 x 844, touch, the real assets/canvas.js run on the page (the harness stubs #csave and #cstatus)."""
        self.ws.call("Emulation.setDeviceMetricsOverride", width=390, height=844, deviceScaleFactor=2, mobile=True)
        self.ws.call("Page.navigate", url=self.base + "/feed/canvas")
        time.sleep(1.2)
        # the page gets assets/finerange.js on every page (wrap.cpp), so the harness runs it before the canvas script
        src = (open(os.path.join(self.platform, "assets", "finerange.js"), encoding="utf-8").read() + "\n"
               + open(os.path.join(self.platform, "assets", "canvas.js"), encoding="utf-8").read())
        try:
            cdp.js(self.ws, FS_BOOT % src)
        except Exception as e:  # a script that throws on boot fails every case below with the reason, not a skip
            self.check("F0 the canvas script runs on the phone page", False, "%s: %s" % (type(e).__name__, e))
            return
        time.sleep(0.4)
        with open(os.path.join(self.out, "canvas-fullscreen-390.png"), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))
        p = json.loads(cdp.js(self.ws, "JSON.stringify(%s)" % FS_PROBE))
        self.check("F1 phone: the page is locked to one screen (no scrolling away)",
                   p["cls"] and p["sh"] <= p["H"] + 1 and p["sw"] <= p["W"] + 1 and p["ovf"] == "hidden",
                   {k: p[k] for k in ("cls", "sh", "sw", "W", "H", "ovf")})
        self.check("F2 phone: the canvas area fills the whole screen and the canvas fits inside it, whole",
                   p["row"] is not None and p["row"]["w"] >= p["W"] - 1 and p["row"]["h"] >= p["H"] - 1
                   and p["wrap"]["l"] >= -1 and p["wrap"]["r"] <= p["W"] + 1 and p["wrap"]["t"] >= -1 and p["wrap"]["b"] <= p["H"] + 1
                   and p["wrap"]["w"] >= 0.85 * p["W"], {"row": p["row"], "wrap": p["wrap"], "style": p["wrapStyle"],
                                                         "pos": p["wrapPos"], "bitmap": p["bitmap"]})
        self.check("F3 phone: all 11 tools sit in a bottom dock that floats on top of the canvas",
                   p["dock"] is not None and p["dockTools"] == 11 and p["dock"]["b"] <= p["H"] + 1 and p["dock"]["t"] > 0.6 * p["H"]
                   and p["dockOver"], {"dock": p["dock"], "tools": p["dockTools"], "over": p["dockOver"]})
        self.check("F4 phone: a top bar with close, undo, redo and done floats at the top",
                   p["top"] is not None and p["top"]["t"] >= 0 and p["top"]["b"] < 0.2 * p["H"] and p["topOver"]
                   and p["topBtns"] == ["cfs-close", "cfs-undo", "cfs-redo", "cfs-done"], {"top": p["top"], "btns": p["topBtns"]})
        g = json.loads(cdp.js(self.ws, "JSON.stringify(%s)" % FS_GESTURES))
        self.check("F5 phone: two fingers moving apart zoom in (about 2x)", 1.6 <= g["pinched"]["s"] <= 2.6,
                   {"start": g["start"], "pinched": g["pinched"]})
        self.check("F6 phone: two fingers moving together pan the canvas", g["panned"]["x"] - g["pinched"]["x"] >= 25
                   and g["panned"]["y"] - g["pinched"]["y"] >= 15, {"pinched": g["pinched"], "panned": g["panned"]})
        self.check("F7 phone: one finger still draws", g["drew"], g)
        self.check("F8 phone: a second finger landing mid-stroke cancels the stroke (no stray ink)", g["strayInk"] == 0, g)
        self.check("F9 phone: the browser Back button asks before leaving (and stays on the canvas)", g["confirmCalls"] == 1, g)
        self.shot("canvas-fullscreen-zoomed-390.png")
        cdp.js(self.ws, "document.getElementById('cfs-props') && document.getElementById('cfs-props').click()")
        time.sleep(0.2)
        self.shot("canvas-fullscreen-sheet-390.png")
        t = json.loads(cdp.js(self.ws, "JSON.stringify(%s)" % FS_SHEETS))
        self.check("F10 phone: the sliders button opens size and opacity over the canvas, and closes again",
                   t["opened"] and not t["closed"], t)
        # Dive (Brandon 2026-11-19, q231: while you draw the picture keeps zooming IN around the pen tip, like an infinite fractal
        # zoom, with a dive-speed slider; the zoom is baked into the layers so it can go on for ever)
        v = json.loads(cdp.js(self.ws, "(async () => JSON.stringify(await %s))()" % FS_DIVE))
        self.check("F11 phone: the props sheet has a Dive toggle and a dive-speed slider (1 to 100)",
                   v.get("ui") and v.get("min") == "1" and v.get("max") == "100" and v.get("inSheet"), v)
        self.check("F12 phone: with Dive off, holding the pen down does not zoom", v.get("offZoom", 99) <= 1.01, v)
        self.check("F13 phone: with Dive on at full speed, holding the pen down zooms in (at least 1.8x) and bakes it into the layers",
                   v.get("onZoom", 0) >= 1.8 and v.get("bakes", 0) >= 1, v)
        self.check("F14 phone: after a bake the ink survives, the view scale was reset (under 2x) and the canvas still covers the screen centre",
                   v.get("ink", 0) > 0 and v.get("viewS", 99) <= 2.05 and v.get("wrapCovers", False), v)
        self.check("F15 phone: the toggle and the speed are remembered", v.get("saved") == {"on": True, "speed": 100}, v)
        self.shot("canvas-fullscreen-dive-390.png")
        # Timelapse (Brandon 2026-11-19, q239 slice 2a): every action is recorded as an event with its timing and can be watched again; the log
        # starts fresh when the chosen period (hour, day, week, month) has passed
        self.ws.call("Page.navigate", url=self.base + "/feed/canvas")
        time.sleep(1.2)
        cdp.js(self.ws, "localStorage.removeItem('lg-log'); localStorage.removeItem('lg-log-prev'); localStorage.removeItem('lg-log-reset');")
        cdp.js(self.ws, FS_BOOT % src)
        time.sleep(0.4)
        w = json.loads(cdp.js(self.ws, "(async () => JSON.stringify(await %s))()" % FS_LOG))
        self.check("F16 phone: the More sheet has a Watch button and a 'new timelapse every' choice",
                   w.get("ui") and w.get("inSheet"), w)
        self.check("F17 phone: drawing records events with their points and timing",
                   w.get("strokes", 0) >= 2 and w.get("firstPts", 0) >= 3 and w.get("firstTool") == "pen", w)
        self.check("F18 phone: Watch draws the log again (about the same ink as the live drawing)",
                   w.get("liveInk", 0) > 0 and 0.6 * w.get("liveInk", 1) <= w.get("replayInk", 0) <= 1.6 * w.get("liveInk", 1), w)
        self.check("F19 phone: an undo is part of the log (Watch shows less ink after it)",
                   0 < w.get("replayInk2", 0) < 0.75 * w.get("replayInk", 1), w)
        # Playback speed (Brandon 2026-11-19: one slider that starts at 0 and slides left for backward, right for forward, with fine control)
        sv = json.loads(cdp.js(self.ws, "(async () => JSON.stringify(await %s))()" % FS_SPEED))
        self.check("F21 phone: the speed slider is fine near 0 (under 0.2x at 5), 32x at the ends, symmetric, and 0 at the middle",
                   0 < sv.get("at5", 0) < 0.2 and 30 <= sv.get("at100", 0) <= 34 and -34 <= sv.get("atM100", 0) <= -30 and sv.get("at0") == 0, sv)
        self.check("F22 phone: sliding left plays backward (the drawing un-draws back to nothing)",
                   sv.get("endInk", 0) > 0 and sv.get("backInk", 99999) < 0.1 * max(sv.get("endInk", 1), 1) and sv.get("backIdx") == 0, sv)
        self.check("F23 phone: at 0 the replay holds still", sv.get("held") is True, sv)
        self.check("F24 phone: dragging the same distance 300 px away from the slider changes the speed far less (finer scrubbing)",
                   sv.get("dragNear", 0) > 5 and abs(sv.get("dragFar", 99)) < 0.35 * sv.get("dragNear", 0), sv)
        self.ws.call("Page.navigate", url=self.base + "/feed/canvas")
        time.sleep(1.2)
        cdp.js(self.ws, "localStorage.setItem('lg-log-reset','hour'); localStorage.setItem('lg-log', JSON.stringify({v:1,start:Date.now()-2*3600*1000,"
                        "events:[{type:'stroke',t:0,tool:'pen',color:'#000000',size:8,opacity:1,layer:0,pts:[[10,10,0],[20,20,5],[30,30,9]]}]}));")
        cdp.js(self.ws, FS_BOOT % src)
        time.sleep(0.4)
        r = json.loads(cdp.js(self.ws, "JSON.stringify((() => { const cur = JSON.parse(localStorage.getItem('lg-log') || 'null'), prev = JSON.parse(localStorage.getItem('lg-log-prev') || 'null');"
                                       "return {curFresh: !!cur && Date.now() - cur.start < 120000 && (cur.events || []).length === 0, prevKept: !!prev && (prev.events || []).length === 1}; })())"))
        self.check("F20 phone: a log older than the chosen period is archived and a fresh one starts", r.get("curFresh") and r.get("prevKept"), r)

    def shot(self, name):
        with open(os.path.join(self.out, name), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))


FS_LOG = r"""(async () => {
  const q = id => document.getElementById(id), ov = q('coverlay'), c0 = q('clayer0');
  const fire = (type, id, x, y) => ov.dispatchEvent(new PointerEvent(type, {pointerId: id, pointerType: 'touch', isPrimary: id === 1,
    clientX: x, clientY: y, bubbles: true, cancelable: true, button: 0, buttons: type === 'pointerup' ? 0 : 1}));
  const wait = ms => new Promise(r => setTimeout(r, ms));
  const inkOf = c => { const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i] > 0) n++; return n; };
  const out = {}, watch = q('cwatch'), reset = q('clogreset');
  out.ui = !!(watch && reset); out.inSheet = !!(watch && watch.closest('.cfs-pm'));
  if (!out.ui) return out;
  window.dispatchEvent(new Event('resize')); await wait(100);
  const b = ov.getBoundingClientRect(), x0 = b.left + 40, y0 = b.top + 40;
  const stroke = async (dx, dy) => { fire('pointerdown', 1, x0 + dx, y0 + dy); for (let i = 1; i <= 8; i++) { fire('pointermove', 1, x0 + dx + i * 14, y0 + dy + i * 6); await wait(8); } fire('pointerup', 1, x0 + dx + 112, y0 + dy + 48); await wait(40); };
  await stroke(0, 0); await stroke(0, 90);
  out.liveInk = inkOf(c0);
  await wait(350);                     // the log is written to localStorage 200 ms after the last action
  const log = JSON.parse(localStorage.getItem('lg-log') || 'null') || {events: []};
  const strokes = log.events.filter(e => e.type === 'stroke');
  out.strokes = strokes.length; out.firstPts = strokes[0] ? strokes[0].pts.length : 0; out.firstTool = strokes[0] ? strokes[0].tool : null;
  const play = async () => { watch.click(); await wait(150); const sp = q('cwatchspeed'); if (sp) { sp.value = '100'; sp.dispatchEvent(new Event('input', {bubbles: true})); }
    for (let i = 0; i < 40; i++) { await wait(100); const p = q('cwatchprogress'); if (p && Number(p.value) >= Number(p.max)) break; }
    const wc = q('cwatchcanvas'); const n = wc ? inkOf(wc) : -1; const close = q('cwatchclose'); if (close) close.click(); await wait(100); return n; };
  out.replayInk = await play();
  q('cfs-undo').click(); await wait(100);
  out.replayInk2 = await play();
  return out;
})()"""

FS_SPEED = r"""(async () => {
  const q = id => document.getElementById(id), wait = ms => new Promise(r => setTimeout(r, ms)), out = {};
  const inkOf = c => { const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i] > 0) n++; return n; };
  q('cwatch').click(); await wait(150);
  const sp = q('cwatchspeed'), prog = q('cwatchprogress'), wc = q('cwatchcanvas');
  const set = v => { sp.value = String(v); sp.dispatchEvent(new Event('input', {bubbles: true})); return parseFloat(sp.dataset.speed); };
  out.at5 = set(5); out.at100 = set(100); out.atM100 = set(-100); out.at0 = set(0);
  set(100); for (let i = 0; i < 40 && Number(prog.value) < Number(prog.max); i++) await wait(100);
  out.endInk = inkOf(wc);
  set(-100); for (let i = 0; i < 60 && Number(prog.value) > 0; i++) await wait(100);
  out.backIdx = Number(prog.value); out.backInk = inkOf(wc);
  set(100); await wait(120); set(0); const at = prog.value; await wait(400); out.held = prog.value === at;
  // the farther the finger is from the slider (up or down), the finer the adjustment: the same 60 px sideways drag, near and 300 px away
  const r = sp.getBoundingClientRect(), x0 = r.left + r.width / 2, y0 = r.top + r.height / 2;
  const fire = (t, x, y) => (t === 'pointerdown' ? sp : document).dispatchEvent(new PointerEvent(t, {pointerId: 7, pointerType: 'touch', clientX: x, clientY: y, bubbles: true, cancelable: true}));
  const dragBy = async dy => { set(0); fire('pointerdown', x0, y0); for (let i = 1; i <= 6; i++) fire('pointermove', x0 + i * 10, y0 + dy); fire('pointerup', x0 + 60, y0 + dy); await wait(30); return Number(sp.value); };
  out.dragNear = await dragBy(0); out.dragFar = await dragBy(300);
  q('cwatchclose').click(); await wait(100);
  return out;
})()"""

FS_DIVE = r"""(async () => {
  const ov = document.getElementById('coverlay'), wrap = document.getElementById('ccanvaswrap'), c0 = document.getElementById('clayer0');
  const fire = (type, id, x, y) => ov.dispatchEvent(new PointerEvent(type, {pointerId: id, pointerType: 'touch', isPrimary: id === 1,
    clientX: x, clientY: y, bubbles: true, cancelable: true, button: 0, buttons: type === 'pointerup' ? 0 : 1}));
  const wait = ms => new Promise(r => setTimeout(r, ms));
  const ink = () => { const d = c0.getContext('2d').getImageData(0, 0, c0.width, c0.height).data; let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i] > 0) n++; return n; };
  const wipe = () => { for (let i = 0; i < 3; i++) { const c = document.getElementById('clayer' + i); c.getContext('2d').clearRect(0, 0, c.width, c.height); } };
  const dive = () => parseFloat(wrap.dataset.dive || '1');
  const out = {};
  window.dispatchEvent(new Event('resize'));          // the view back to "fit"
  await wait(150);
  const toggle = document.getElementById('cdive'), speed = document.getElementById('cdivespeed');
  out.ui = !!(toggle && speed); if (!out.ui) return out;
  out.min = speed.min; out.max = speed.max; out.inSheet = !!speed.closest('.cfs-pp') && !!toggle.closest('.cfs-pp');
  const hold = async ms => { const b = ov.getBoundingClientRect(), x = b.left + b.width / 2, y = b.top + b.height / 2;
    fire('pointerdown', 1, x, y); fire('pointermove', 1, x + 3, y + 1); await wait(ms); const z = dive(); fire('pointerup', 1, x + 3, y + 1); return z; };
  wipe(); out.offZoom = await hold(600);
  toggle.click(); speed.value = '100'; speed.dispatchEvent(new Event('input', {bubbles: true}));
  window.dispatchEvent(new Event('resize')); await wait(150); wipe();
  const before = dive();
  out.onZoom = (await hold(1500)) / before;
  out.bakes = parseInt(wrap.dataset.bakes || '0', 10);
  out.ink = ink();
  const m = new DOMMatrix(getComputedStyle(wrap).transform); out.viewS = m.a;
  const r = wrap.getBoundingClientRect();   // the view stays zoomed in after the pen lifts (inward only), so it must still cover the screen centre
  out.wrapCovers = r.left <= innerWidth / 2 && r.right >= innerWidth / 2 && r.top <= innerHeight / 2 && r.bottom >= innerHeight / 2;
  try { out.saved = JSON.parse(localStorage.getItem('lg-dive')); } catch (e) { out.saved = null; }
  return out;
})()"""

FS_BOOT = r"""(() => {
  window.LG_CANVAS = {px: 1, frames: 0, user: 't', mode: 'portrait', postTarget: ''};
  for (const id of ['csave', 'cstatus']) if (!document.getElementById(id)) { const b = document.createElement(id === 'csave' ? 'button' : 'p'); b.id = id; document.body.appendChild(b); }
  window.confirm = () => { window.__confirms = (window.__confirms || 0) + 1; return false; };
  %s
})()"""

FS_PROBE = r"""(() => {
  const r = e => { if (!e) return null; const b = e.getBoundingClientRect(); return {l: b.left, t: b.top, r: b.right, b: b.bottom, w: b.width, h: b.height}; };
  const q = s => document.querySelector(s);
  const dock = q('.cfs-dock'), top = q('.cfs-top'), row = q('.ccanvasrow'), wrap = document.getElementById('ccanvaswrap');
  const de = document.documentElement, se = document.scrollingElement;
  const hit = e => { const b = e.getBoundingClientRect(); const t = document.elementFromPoint(b.left + b.width / 2, b.top + b.height / 2); return !!t && e.contains(t); };
  return {cls: de.classList.contains('cfs'), sh: se.scrollHeight, sw: se.scrollWidth, W: innerWidth, H: innerHeight,
          ovf: getComputedStyle(de).overflow, row: r(row), wrap: r(wrap), dock: r(dock), top: r(top),
          wrapStyle: wrap.getAttribute('style'), wrapPos: getComputedStyle(wrap).position,
          bitmap: [document.getElementById('clayer0').width, document.getElementById('clayer0').height],
          dockTools: dock ? dock.querySelectorAll('.toolbtn').length : 0,
          topBtns: top ? ['cfs-close', 'cfs-undo', 'cfs-redo', 'cfs-done'].filter(id => top.querySelector('#' + id)) : [],
          dockOver: dock ? hit(dock) : false, topOver: top ? hit(top) : false};
})()"""

FS_GESTURES = r"""(() => {
  const ov = document.getElementById('coverlay'), wrap = document.getElementById('ccanvaswrap'), c0 = document.getElementById('clayer0');
  const fire = (type, id, x, y) => ov.dispatchEvent(new PointerEvent(type, {pointerId: id, pointerType: 'touch', isPrimary: id === 1,
    clientX: x, clientY: y, bubbles: true, cancelable: true, button: 0, buttons: type === 'pointerup' ? 0 : 1}));
  const view = () => { const m = new DOMMatrix(getComputedStyle(wrap).transform); return {s: m.a, x: m.e, y: m.f}; };
  const ink = () => { const d = c0.getContext('2d').getImageData(0, 0, c0.width, c0.height).data; let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i] > 0) n++; return n; };
  const wipe = () => { for (let i = 0; i < 3; i++) { const c = document.getElementById('clayer' + i); c.getContext('2d').clearRect(0, 0, c.width, c.height); } };
  const out = {start: view()}, b = ov.getBoundingClientRect(), cx = b.left + b.width / 2, cy = b.top + b.height / 2;
  fire('pointerdown', 1, cx - 50, cy); fire('pointerdown', 2, cx + 50, cy);
  for (let i = 1; i <= 10; i++) { fire('pointermove', 1, cx - 50 - i * 5, cy); fire('pointermove', 2, cx + 50 + i * 5, cy); }
  fire('pointerup', 1, cx - 100, cy); fire('pointerup', 2, cx + 100, cy);
  out.pinched = view();
  fire('pointerdown', 1, cx - 20, cy); fire('pointerdown', 2, cx + 20, cy);
  for (let i = 1; i <= 10; i++) { fire('pointermove', 1, cx - 20 + i * 4, cy + i * 3); fire('pointermove', 2, cx + 20 + i * 4, cy + i * 3); }
  fire('pointerup', 1, cx + 20, cy + 30); fire('pointerup', 2, cx + 60, cy + 30);
  out.panned = view();
  wipe();
  const b2 = ov.getBoundingClientRect(), px = b2.left + b2.width / 2, py = b2.top + b2.height / 2;
  fire('pointerdown', 1, px - 30, py); for (let i = 1; i <= 8; i++) fire('pointermove', 1, px - 30 + i * 8, py + i); fire('pointerup', 1, px + 34, py + 8);
  out.drew = ink() > 0;
  wipe();
  fire('pointerdown', 1, px - 30, py); fire('pointermove', 1, px - 10, py + 5); fire('pointerdown', 2, px + 40, py + 40);
  fire('pointermove', 2, px + 50, py + 40); fire('pointerup', 2, px + 50, py + 40); fire('pointerup', 1, px - 10, py + 5);
  out.strayInk = ink();
  window.__confirms = 0;
  window.dispatchEvent(new PopStateEvent('popstate', {state: null}));
  out.confirmCalls = window.__confirms;
  const b3 = ov.getBoundingClientRect();   // a visible stroke for the screenshot (not asserted)
  fire('pointerdown', 1, b3.left + 30, b3.top + 60);
  for (let i = 1; i <= 40; i++) fire('pointermove', 1, b3.left + 30 + i * 9, b3.top + 60 + Math.sin(i / 4) * 40);
  fire('pointerup', 1, b3.left + 400, b3.top + 60);
  return out;
})()"""

FS_SHEETS = r"""(() => {
  const shown = id => { const e = document.getElementById(id); if (!e) return false; const b = e.getBoundingClientRect(); return b.width > 0 && b.height > 0 && getComputedStyle(e).visibility !== 'hidden'; };
  const btn = document.getElementById('cfs-props'); if (!btn) return {opened: false, closed: false, error: 'no #cfs-props'};
  const opened = shown('csizer') && shown('copacityr');   // the caller opened the sheet, so the screenshot shows it
  btn.click(); const closed = shown('csizer');
  return {opened: opened, closed: closed};
})()"""


PLATFORM = os.path.dirname(os.path.dirname(HERE))


def main(argv):
    global PLATFORM
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    PLATFORM = platform
    out = argv[1] if len(argv) > 1 else os.path.join("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."), "lg-canvas-layout")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(page(platform)))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-canvas-layout-")
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
        print("VERDICT canvas-layout: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
