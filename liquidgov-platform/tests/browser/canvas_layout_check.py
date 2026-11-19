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
    # a variable between two + is filled in (icons as a plain 16 px box); then the literals are read in order
    def fill(m):
        name = m.group(1)
        if name.startswith("kIcon"):
            return '+"<svg width=\\"16\\" height=\\"16\\" viewBox=\\"0 0 16 16\\"><rect width=\\"16\\" height=\\"16\\"/></svg>"+'
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
