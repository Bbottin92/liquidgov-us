"""Each pool's shore takes the colour of its own kind (Brandon 2026-11-16: "add the far pool shore colour"; follows
walker_kind_marks_check.py, which stays as committed).

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, the walker
on (pools), your pool opened. The shore is the pale rim stroke of each pool's oval (.lgs). Its computed colour is read
from the page (getComputedStyle) and compared with the kind colour the mouth symbols use: tinted TOWARD the kind (closer
to it than the plain pale rim is), still pale (not the kind colour itself, so the rim stays a rim), and the width kept.
Events has no kind colour and keeps the plain pale rim exactly.

  H0  precondition: the map loaded, your pool is open and the seven pools below are on the map
  H1  Friends gold, Groups violet, Places green, Politics coral, Feeds teal, Interests pink, and your own pool white:
      each shore is closer to its kind colour than the plain rim (rgb 143,208,234) is, by at least 25%, and not equal to it
  H2  Events keeps the plain pale rim exactly (143,208,234), and every shore is still 7 px wide
  H3  the pool you stand in (walking, brighter rim) is tinted too: closer to its colour than the walking rim (191,233,248)

Usage: python3 walker_shore_colour_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-shore-colour: ALL-PASS" or the failures; exit 1 on a FAIL."""
import http.server
import os
import re
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import walker_controls_check as wc  # noqa: E402
import walker_instance_check as wi  # noqa: E402

PLAIN = (143, 208, 234)     # the plain pale rim
PLAIN_WALK = (191, 233, 248)  # the brighter rim of the pool you stand in
# the kind colours (the same ones walker_kind_marks_check.py reads off the mouth symbols)
WANT = {"mine:friends": (242, 201, 76), "mine:groups": (179, 146, 240), "mine:places": (95, 208, 104), "mine:politics": (255, 127, 110),
        "mine:feeds": (63, 208, 196), "mine:interests": (255, 126, 182), "you": (255, 255, 255)}
# every pool's shore: computed stroke colour, stroke width, and whether it is the walking pool
PROBE = ("(() => { const out = {}; document.querySelectorAll('#lgm-nodes .lgn[data-id]').forEach(g => { const s = g.querySelector('.lgs');"
         " if (!s) return; const c = getComputedStyle(s); out[g.getAttribute('data-id')] = {stroke: c.stroke, width: c.strokeWidth,"
         " walk: g.classList.contains('lgn-walk')}; }); return JSON.stringify(out); })()")


def rgb(text):
    """'rgb(1, 2, 3)', 'rgba(...)' or 'color(srgb 0.1 0.2 0.3)' -> (r, g, b) in 0..255, or None"""
    m = re.match(r"\s*rgba?\(\s*([\d.]+)[ ,]+([\d.]+)[ ,]+([\d.]+)", text or "")
    if m:
        return tuple(float(x) for x in m.groups())
    m = re.match(r"\s*color\(srgb\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)", text or "")
    if m:
        return tuple(float(x) * 255 for x in m.groups())
    return None


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


class Run(wi.Run):
    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        self.j("LG_MAP.goTo('you')")
        time.sleep(2.5)
        pools = self.j(PROBE) or {}
        need = list(WANT) + ["mine:events"]
        self.check("H0 precondition: the map loaded, your pool is open and the pools to measure are on the map",
                   ok and all(k in pools for k in need), {"loaded": ok, "missing": [k for k in need if k not in pools]})
        if not ok or not all(k in pools for k in need):
            return
        # a zoomed-out look, so the far pools show (for the eye; the colours are read from the page either way)
        for _ in range(8):   # wheel out over the map, as a person zooms out
            self.j("(() => { const s = document.querySelector('#lgmap svg') || document.querySelector('svg'), f = LG_MAP.frame();"
                   " s.dispatchEvent(new WheelEvent('wheel', {deltaY: 240, ctrlKey: true, clientX: f.left + f.width / 2, clientY: f.top + f.height / 2,"
                   " bubbles: true, cancelable: true})); return 1; })()")
            time.sleep(0.15)
        time.sleep(1.2)
        self.shot("shore-colour-out")
        bad, walk_bad = {}, {}
        for k, kind in WANT.items():
            d = pools[k]
            c = rgb(d["stroke"])
            base = PLAIN_WALK if d["walk"] else PLAIN
            tinted = c is not None and dist(c, kind) <= 0.75 * dist(base, kind) and dist(c, base) > 3
            if not tinted:
                (walk_bad if d["walk"] else bad)[k] = {"stroke": d["stroke"], "plain": base, "kind": kind}
        self.check("H1 each far pool's shore is tinted toward its own kind colour (and is still a pale rim)", not bad, bad)
        ev = pools["mine:events"]
        evc = rgb(ev["stroke"])
        widths = {k: d["width"] for k, d in pools.items() if d["width"] not in ("7px", "7")}
        self.check("H2 Events keeps the plain pale rim exactly, and every shore is still 7 px wide",
                   evc is not None and dist(evc, PLAIN) < 1.5 and not widths, {"events": ev["stroke"], "other widths": widths})
        walking = [k for k, d in pools.items() if d["walk"]]
        self.check("H3 the pool you stand in (walking rim) is tinted too",
                   bool(walking) and not walk_bad and all(k in WANT for k in walking), {"walking": walking, "bad": walk_bad})


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), wc.make_handler(os.path.join(platform, "assets"),
                                                                           wc.mlc.page_markup(platform), wc.home_markup(platform)))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-walker-shore-")
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
        print("VERDICT walker-shore-colour: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
