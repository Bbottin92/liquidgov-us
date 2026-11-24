"""tools/edit_mode.js, the edit-mode overlay Claude injects into a page so Brandon can mark spots and comment on them
(q294, Brandon 2026-11-21: "draw an x in an area or over an object or in a certain shape ... a number assigned to it
followed by a comment line in an area on the right side of the screen").

Real headless Chromium; a stdlib server serves tools/edit_mode_demo.html WITHOUT its script tag and under a strict
Content-Security-Policy (default-src 'self'; style-src 'self'), and the real tools/edit_mode.js is injected the way
Claude injects it (Runtime.evaluate). Pointer events are dispatched by script, the way a mouse would send them.
  E0  injected under the strict CSP: window.LG_EDIT is there, the panel is laid out (fixed, 320 px on the right), the
      page is not covered while drawing is off (a click on Back reaches Back)
  E1  an X (two quick strokes over Back) is ONE mark, kind x, numbered 1, whose target is #lgm-back "Back"
  E2  a loop around the pool list's words is mark 2 and lists the three rows inside it
  E3  a tap on the heading is mark 3, a dot on the h1 "Your pools"
  E4  each mark has a comment line in the panel; typing in it shows in report()
  E5  numbers never shift: deleting mark 2 leaves 1 and 3, and the next mark is 4
  E6  marks are kept in page px: after scrolling 300 px the report's box is the same and its on-screen box moved 300 px up
  E7  a reload and a new injection bring the marks and comments back (localStorage per page path)
  E8  a phone (390 px): the panel is a bottom sheet across the width, and the Draw pill sits above it
Usage: LG_BROWSER=$(command -v google-chrome) python3 edit_mode_check.py [<liquidgov-platform dir> [<out dir>]]
Prints "VERDICT edit-mode: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file."""
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
import juketube_overlap_check as jo  # noqa: E402,F401  (sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

CSP = "default-src 'self'; style-src 'self'; script-src 'self'"

DRAW = r"""(async () => {
  const sleep = ms => new Promise(r => setTimeout(r, ms)), cv = document.querySelector('#lg-edit-root canvas');
  const ev = (type, x, y) => cv.dispatchEvent(new PointerEvent(type, {clientX: x, clientY: y, pointerId: 1, bubbles: true, button: 0, buttons: type === 'pointerup' ? 0 : 1}));
  const line = async pts => { ev('pointerdown', ...pts[0]); for (const p of pts.slice(1)) ev('pointermove', ...p); ev('pointerup', ...pts[pts.length - 1]); await sleep(60); };
  const seg = (x0, y0, x1, y1) => Array.from({length: 9}, (_, i) => [x0 + (x1 - x0) * i / 8, y0 + (y1 - y0) * i / 8]);
  LG_EDIT.on();
  const b = document.getElementById('lgm-back').getBoundingClientRect(), cx = b.left + b.width / 2, cy = b.top + b.height / 2;
  await line(seg(cx - 14, cy - 10, cx + 14, cy + 10));
  await line(seg(cx + 14, cy - 10, cx - 14, cy + 10));
  await sleep(1400);
  const rg = document.createRange(), rows = [...document.querySelectorAll('ul.pools li')].map(li => { rg.selectNodeContents(li); return rg.getBoundingClientRect(); });
  const l = Math.min(...rows.map(r => r.left)) - 12, r = Math.max(...rows.map(r => r.right)) + 12, t = rows[0].top - 10, bt = rows[rows.length - 1].bottom + 10;
  await line(Array.from({length: 25}, (_, i) => [(l + r) / 2 + Math.cos(i / 24 * 6.3) * (r - l) / 2, (t + bt) / 2 + Math.sin(i / 24 * 6.3) * (bt - t) / 2]));
  await sleep(1400);
  const h = document.querySelector('h1').getBoundingClientRect();
  await line([[h.left + 30, h.top + h.height / 2]]);
  const tas = document.querySelectorAll('#lg-edit-root textarea');
  ['make Back bigger', 'show these as pool chips', 'rename to My pools'].forEach((v, i) => { if (tas[i]) { tas[i].value = v; tas[i].dispatchEvent(new Event('input')); } });
  LG_EDIT.off();
  return JSON.stringify({rows: tas.length});
})()"""

STATE = r"""(() => { const p = document.querySelector('#lg-edit-root > div'), cs = p ? getComputedStyle(p) : null, pr = p ? p.getBoundingClientRect() : null;
  const pill = document.querySelector('#lg-edit-root > button'), pl = pill ? pill.getBoundingClientRect() : null;
  return JSON.stringify({edit: !!window.LG_EDIT, panel: cs && {display: cs.display, l: pr.left, t: pr.top, w: pr.width, h: pr.height},
    pill: pl && {t: pl.top, b: pl.bottom}, vw: innerWidth, vh: innerHeight,
    report: window.LG_EDIT ? JSON.parse(LG_EDIT.report()) : null}); })()"""


class Run:
    def __init__(self):
        self.fails, self.n = [], 0

    def check(self, name, ok, detail):
        self.n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:600]), flush=True)
        if not ok:
            self.fails.append(name)


def make_handler(page):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Security-Policy", CSP)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(page.encode())
    return H


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    tools = os.path.join(platform, "tools")
    page = open(os.path.join(tools, "edit_mode_demo.html"), encoding="utf-8").read()
    page = re.sub(r"<script[^>]*></script>", "", page)   # the injection is the only way the tool gets in, as on the site
    script = open(os.path.join(tools, "edit_mode.js"), encoding="utf-8").read()
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(page))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = "http://127.0.0.1:%d/demo" % srv.server_address[1]
    r, proc, ws = Run(), None, None

    def inject():
        v = cdp.js(ws, script + "\n;JSON.stringify(!!window.LG_EDIT)")
        return v

    def state():
        v = cdp.js(ws, STATE)
        return json.loads(v) if isinstance(v, str) else {"__error": v}

    def load(w, h, mobile):
        ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=1, mobile=mobile)
        ws.call("Page.navigate", url=url)
        time.sleep(0.8)

    def shot(name):
        path = os.path.join(out, "edit-mode-%s.png" % name)
        with open(path, "wb") as f:
            f.write(base64.b64decode(ws.call("Page.captureScreenshot", format="png")["data"]))
        print("SHOT " + path, flush=True)

    try:
        proc, ws = cdp.launch_edge("lg-edit-mode-")
        for d in ("Page.enable", "Runtime.enable"):
            ws.call(d)
        load(1280, 800, False)
        cdp.js(ws, "(localStorage.clear(), '1')")
        injected = inject()
        cdp.js(ws, "(window.__clicks = 0, document.getElementById('lgm-back').addEventListener('click', () => window.__clicks++), '1')")
        b = json.loads(cdp.js(ws, "JSON.stringify((r => ({x: r.left + r.width / 2, y: r.top + r.height / 2}))(document.getElementById('lgm-back').getBoundingClientRect()))"))
        for kind in ("mousePressed", "mouseReleased"):
            ws.call("Input.dispatchMouseEvent", type=kind, x=b["x"], y=b["y"], button="left", clickCount=1)
        time.sleep(0.2)
        s0 = state()
        p0 = s0.get("panel") or {}
        r.check("E0 under a strict CSP the tool is in, its panel is laid out (320 px on the right) and, drawing off, a click reaches the page",
                injected == "true" and s0.get("edit") is True and p0.get("display") == "flex" and abs(p0.get("w", 0) - 320) < 1
                and abs(p0.get("l", 0) - (s0["vw"] - 320)) < 1 and cdp.js(ws, "JSON.stringify(window.__clicks)") == "1",
                {"injected": injected, "state": {k: s0.get(k) for k in ("edit", "panel", "vw")}, "clicks": cdp.js(ws, "JSON.stringify(window.__clicks)")})
        drew = cdp.js(ws, DRAW)
        s1 = state()
        shot("desktop")
        ms = {m["n"]: m for m in ((s1.get("report") or {}).get("marks") or [])}
        m1, m2, m3 = ms.get(1, {}), ms.get(2, {}), ms.get(3, {})
        r.check("E1 an X over Back is one mark, kind x, numbered 1, its target #lgm-back 'Back'",
                m1.get("kind") == "x" and len(m1.get("strokes", [])) == 2 and (m1.get("target") or {}).get("selector") == "#lgm-back"
                and (m1.get("target") or {}).get("text") == "Back", {"drew": drew, "mark1": {k: m1.get(k) for k in ("kind", "target")}})
        r.check("E2 a loop around the pool list's words is mark 2 and lists the three rows inside it",
                m2.get("kind") == "shape" and [x.get("text") for x in m2.get("inside", []) if x.get("tag") == "li"] == ["Friends", "Politics", "Places"],
                {k: m2.get(k) for k in ("kind", "target", "inside")})
        r.check("E3 a tap on the heading is mark 3, a dot on the h1 'Your pools'",
                m3.get("kind") == "dot" and (m3.get("target") or {}).get("tag") == "h1" and (m3.get("target") or {}).get("text") == "Your pools",
                {k: m3.get(k) for k in ("kind", "target")})
        r.check("E4 each mark has a comment line; what is typed shows in report()",
                [ms.get(i, {}).get("comment") for i in (1, 2, 3)] == ["make Back bigger", "show these as pool chips", "rename to My pools"],
                {"comments": [ms.get(i, {}).get("comment") for i in (1, 2, 3)]})
        box_before = m1.get("box")
        # the strict CSP drops the demo's inline style attributes, so the spacer is sized through the CSSOM (allowed under CSP)
        scrolled = cdp.js(ws, "(document.getElementById('spacer').style.height = '1400px', scrollTo(0, 300), JSON.stringify(scrollY))")
        time.sleep(0.2)
        s2 = state()
        m1s = {m["n"]: m for m in s2["report"]["marks"]}.get(1, {})
        r.check("E6 marks are kept in page px: after a 300 px scroll the box is the same and on screen it moved 300 px up",
                scrolled == "300" and m1s.get("box") == box_before and box_before and abs((box_before["t"] - m1s["seen"]["t"]) - 300) < 2,
                {"scrollY": scrolled, "before": box_before, "after": m1s.get("box"), "seen": m1s.get("seen")})
        cdp.js(ws, "(scrollTo(0, 0), LG_EDIT.remove(2), '1')")
        cdp.js(ws, "(async () => { LG_EDIT.on(); const cv = document.querySelector('#lg-edit-root canvas');"
                   " const ev = (t, x, y) => cv.dispatchEvent(new PointerEvent(t, {clientX: x, clientY: y, pointerId: 1, bubbles: true, button: 0, buttons: t === 'pointerup' ? 0 : 1}));"
                   " await new Promise(r => setTimeout(r, 1400)); ev('pointerdown', 600, 500); ev('pointermove', 640, 520); ev('pointerup', 640, 520); LG_EDIT.off(); return '1'; })()")
        ns = [m["n"] for m in state()["report"]["marks"]]
        r.check("E5 numbers never shift: deleting mark 2 leaves 1 and 3, and the next mark is 4", ns == [1, 3, 4], ns)
        load(1280, 800, False)
        inject()
        s3 = state()
        kept = [(m["n"], m["comment"]) for m in (s3.get("report") or {}).get("marks", [])]
        r.check("E7 a reload and a new injection bring the marks and comments back",
                kept == [(1, "make Back bigger"), (3, "rename to My pools"), (4, "")], kept)
        load(390, 844, True)
        inject()
        s4 = state()
        shot("phone")
        p4, pill = s4.get("panel") or {}, s4.get("pill") or {}
        r.check("E8 a phone: the panel is a bottom sheet across the width and the Draw pill sits above it",
                abs(p4.get("w", 0) - s4["vw"]) < 1 and abs(p4.get("t", 0) + p4.get("h", 0) - s4["vh"]) < 1 and p4.get("h", 0) < s4["vh"] * 0.5
                and pill.get("b", 9e9) <= p4.get("t", 0), {"panel": p4, "pill": pill, "vw": s4.get("vw"), "vh": s4.get("vh")})
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, ws.profile if ws else None)
        srv.shutdown()
        print("VERDICT edit-mode: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
