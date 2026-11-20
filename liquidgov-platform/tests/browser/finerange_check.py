"""assets/finerange.js, the site-wide fine-scrubbing sliders (Brandon 2026-11-19: "the farther away up or down you slide while controlling the slider,
the more fine the adjustment should become ... that should be true of all sliders in liquidgov").
Real headless Chromium, a stdlib server serving a page with three range sliders and the real assets/finerange.js; no C++ server. Pointer events are
dispatched by script (touch type), the way a finger would send them.
  R1  the same 60 px sideways drag changes the value much less when the finger is 300 px above the slider than when it is on it (under 0.35x)
  R2  the finer the farther: 150 px away changes it less than 50 px away
  R3  grabbing the thumb does not move it (no jump); tapping the track far from the thumb jumps there
  R4  input events fire while dragging and one change event fires at the end
  R5  the slider's own step is honoured (a step-5 slider only ever shows multiples of 5)
  R6  a slider added to the page later works too (the script listens on the document)
Usage: LG_BROWSER=$(command -v google-chrome) python3 finerange_check.py [<liquidgov-platform dir>]
Prints "VERDICT finerange: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file."""
import http.server
import json
import os
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import juketube_overlap_check as jo  # noqa: E402,F401  (sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

PAGE = ("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width'>"
        "<style>body{margin:0;padding:80px 20px}input[type=range]{display:block;width:300px;margin:60px 0;touch-action:none}</style></head><body>"
        "<input type=range id=a min=0 max=100 step=0.1 value=0><input type=range id=b min=0 max=100 step=5 value=50><input type=range id=c min=-100 max=100 step=1 value=0>"
        "<script>window.__ev={input:0,change:0};document.addEventListener('input',e=>{if(e.target.id==='a')__ev.input++;});document.addEventListener('change',e=>{if(e.target.id==='a')__ev.change++;});</script>"
        "<script src='/finerange.js'></script></body></html>")

RUN = r"""(async () => {
  const wait = ms => new Promise(r => setTimeout(r, ms)), out = {};
  const mk = (t, id) => (x, y) => (t === 'pointerdown' ? document.getElementById(id) : document).dispatchEvent(new PointerEvent(t, {pointerId: 5, pointerType: 'touch', clientX: x, clientY: y, bubbles: true, cancelable: true}));
  const geo = id => { const b = document.getElementById(id).getBoundingClientRect(); return {l: b.left, w: b.width, cy: b.top + b.height / 2}; };
  const drag = async (id, startFrac, dx, dy) => { const g = geo(id), x0 = g.l + g.w * startFrac, y0 = g.cy, el = document.getElementById(id);
    mk('pointerdown', id)(x0, y0); for (let i = 1; i <= 6; i++) mk('pointermove', id)(x0 + dx * i / 6, y0 + dy); mk('pointerup', id)(x0 + dx, y0 + dy); await wait(20); return Number(el.value); };
  const a = document.getElementById('a'); window.__ev.input = 0; window.__ev.change = 0;
  a.value = '50'; out.near = (await drag('a', 0.5, 60, 0)) - 50;
  a.value = '50'; out.far300 = (await drag('a', 0.5, 60, -300)) - 50;
  a.value = '50'; out.mid150 = (await drag('a', 0.5, 60, 150)) - 50;
  a.value = '50'; out.mid50 = (await drag('a', 0.5, 60, 50)) - 50;
  window.__ev.input = 0; window.__ev.change = 0; a.value = '30'; await drag('a', 0.3, 40, 0); out.events = {input: window.__ev.input, change: window.__ev.change};
  a.value = '40'; const g = geo('a'); mk('pointerdown', 'a')(g.l + g.w * 0.4, g.cy); mk('pointerup', 'a')(g.l + g.w * 0.4, g.cy); out.afterGrab = Number(a.value);
  a.value = '10'; mk('pointerdown', 'a')(g.l + g.w * 0.9, g.cy); mk('pointerup', 'a')(g.l + g.w * 0.9, g.cy); out.afterTap = Number(a.value);
  const b = document.getElementById('b'); const seen = new Set(); b.value = '50';
  for (const [f, dx] of [[0.5, 37], [0.2, 91], [0.7, -53]]) { await drag('b', f, dx, 0); seen.add(Number(b.value)); }
  out.stepOk = [...seen].every(v => v % 5 === 0);
  const late = document.createElement('input'); late.type = 'range'; late.id = 'late'; late.min = '0'; late.max = '100'; late.value = '0'; late.style.cssText = 'display:block;width:300px;margin:60px 0';
  document.body.appendChild(late); await wait(50); out.late = await drag('late', 0.1, 120, 0);
  return out;
})()"""


class Run:
    def __init__(self):
        self.fails, self.n = [], 0

    def check(self, name, ok, detail):
        self.n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:400]), flush=True)
        if not ok:
            self.fails.append(name)


def make_handler(script):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            body, ctype = (script, "application/javascript") if self.path.startswith("/finerange.js") else (PAGE.encode(), "text/html; charset=utf-8")
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body if isinstance(body, bytes) else body.encode())

    return H


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    path = os.path.join(platform, "assets", "finerange.js")
    script = open(path, encoding="utf-8").read() if os.path.isfile(path) else "/* assets/finerange.js is missing */"
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(script))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r, proc, ws = Run(), None, None
    try:
        proc, ws = cdp.launch_edge("lg-finerange-")
        for d in ("Page.enable", "Runtime.enable"):
            ws.call(d)
        ws.call("Emulation.setDeviceMetricsOverride", width=390, height=844, deviceScaleFactor=1, mobile=True)
        ws.call("Page.navigate", url="http://127.0.0.1:%d/" % srv.server_address[1])
        time.sleep(1.0)
        v = json.loads(cdp.js(ws, "(async () => JSON.stringify(await %s))()" % RUN))
        near = v.get("near", 0)
        r.check("R1 a 60 px drag 300 px away from the slider changes it under 0.35x as much as on it", near > 5 and abs(v.get("far300", 99)) < 0.35 * near, v)
        r.check("R2 the farther the finer: 150 px away changes it less than 50 px away", 0 < abs(v.get("mid150", 99)) < abs(v.get("mid50", 0)) < near, v)
        r.check("R3 grabbing the thumb does not move it; tapping the track far from it jumps there",
                v.get("afterGrab") == 40 and 80 <= v.get("afterTap", 0) <= 100, v)
        r.check("R4 input events fire while dragging and one change event at the end", v.get("events", {}).get("input", 0) >= 2 and v.get("events", {}).get("change") == 1, v)
        r.check("R5 the slider's own step is honoured", v.get("stepOk") is True, v)
        r.check("R6 a slider added later works too", v.get("late", 0) > 10, v)
    except Exception as e:
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, ws.profile if ws else None)
        srv.shutdown()
        print("VERDICT finerange: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
