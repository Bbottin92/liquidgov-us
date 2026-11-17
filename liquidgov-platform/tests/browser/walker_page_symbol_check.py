"""The sample page shows only over a page symbol (Brandon 2026-11-16: "the sample page that shows up when you enter a pool
should only show up when you walk over a page symbol, whatever that looks like. otherwise, the sample is just like in
the way and clutters the view"; "there is no symbol currently... i think we should invent one").

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, the walker
on (pools). A pool that has a page now draws a small page symbol (.lgn-page: a sheet with a folded corner) on its
lower water; the sample (the portal's peek window, lgmap_walk.js) opens only while your drop is over it.

  Y0  precondition: the map loaded and the walker walked into the Friends pool (it has a page, /friends)
  Y1  standing in the pool, still, away from the symbol: no sample (data-peek empty, the peek window not open)
  Y2  a pool with a page draws exactly one page symbol; a pool with no page (Events) draws none
  Y3  walking onto the symbol opens the sample of that pool's page
  Y4  walking off the symbol closes the sample again

Usage: python3 walker_page_symbol_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-page-symbol: ALL-PASS" or the failures; exit 1 on a FAIL."""
import http.server
import math
import os
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import walker_controls_check as wc  # noqa: E402
import walker_instance_check as wi  # noqa: E402

POOL = "mine:friends"      # a pool with a page in the fake graph
BARE = "mine:events"       # a pool with no page

# the sample's state: the id the walker says it is showing, and whether the window is really open on the screen
PEEK = ("(() => { const w = document.getElementById('lgm-walker'), p = document.querySelector('.lgm-peek');"
        " return JSON.stringify({id: w ? (w.getAttribute('data-peek') || '') : null,"
        " open: !!p && !p.hidden && p.classList.contains('lgm-peek-on')}); })()")
# where the page symbols are: pool id -> how many, and the symbol's middle on the screen for the pool
SYMBOLS = ("(() => { const out = {}; document.querySelectorAll('#lgm-nodes .lgn[data-id]').forEach(b => {"
           " out[b.getAttribute('data-id')] = b.querySelectorAll('.lgn-page').length; });"
           " const s = document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"] .lgn-page'), r = s ? s.getBoundingClientRect() : null;"
           " return JSON.stringify({count: out, spot: r ? [r.left + r.width / 2, r.top + r.height / 2] : null}); })()")


class Run(wi.Run):
    def peek(self):
        return self.j(PEEK) or {}

    def symbol_spot(self):
        return (self.j(SYMBOLS % POOL) or {}).get("spot")

    def hold(self, dx, dy):
        """One short key hold toward (dx, dy) on the screen: one key, or two for a slant (as walker_controls_check does)."""
        keys = {"up": "w", "down": "s", "left": "a", "right": "d"}
        n = max(abs(dx), abs(dy)) or 1
        ks = []
        if abs(dx) > 0.4 * n:
            ks.append(keys["right" if dx > 0 else "left"])
        if abs(dy) > 0.4 * n:
            ks.append(keys["down" if dy > 0 else "up"])
        for k in ks:
            self.ws.call("Input.dispatchKeyEvent", type="keyDown", key=k, windowsVirtualKeyCode=wc.VK.get(k, ord(k.upper())))
        time.sleep(max(0.04, min(0.2, n / 300.0)))
        for k in ks:
            self.ws.call("Input.dispatchKeyEvent", type="keyUp", key=k, windowsVirtualKeyCode=wc.VK.get(k, ord(k.upper())))
        time.sleep(0.08)

    def walk_to(self, spot_fn, near_px, limit_s=12):
        """Short key holds toward spot_fn() (a screen point) until the dot is within near_px of it; the distance reached."""
        end, d = time.time() + limit_s, None
        while time.time() < end:
            to, st = spot_fn(), self.j(self.WALK) or {}
            spot = st.get("spot")
            if not to or not spot:
                return None
            dx, dy = to[0] - spot[0], to[1] - spot[1]
            d = math.hypot(dx, dy)
            if d <= near_px:
                return d
            self.hold(dx, dy)
        return d

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        time.sleep(0.6)
        st = self.map_go(POOL)
        time.sleep(2.0)   # the pool opens by itself after 0.7 s; the camera settles; the old sample needs 0.25 s of dwell
        self.check("Y0 precondition: the map loaded and the walker stands in the Friends pool",
                   ok and st.get("at") == POOL, {"loaded": ok, "at": st.get("at")})
        if not ok or st.get("at") != POOL:
            return
        # Y1: still, in the pool, and (if a symbol exists) not on it
        sym0 = self.j(SYMBOLS % POOL) or {}
        spot0 = (self.j(self.WALK) or {}).get("spot")
        far = bool(spot0) and (not sym0.get("spot") or math.hypot(sym0["spot"][0] - spot0[0], sym0["spot"][1] - spot0[1]) > 45)
        time.sleep(1.2)
        p1 = self.peek()
        self.shot("symbol-in-pool")
        self.check("Y1 standing still in the pool, away from the symbol: no sample page",
                   far and not p1.get("id") and not p1.get("open"), {"away from symbol": far, "sample": p1})
        counts = (sym0.get("count") or {})
        self.check("Y2 a pool with a page draws exactly one page symbol; a pool with no page draws none",
                   counts.get(POOL) == 1 and counts.get(BARE, 0) == 0, {POOL: counts.get(POOL), BARE: counts.get(BARE)})
        if not sym0.get("spot"):
            self.check("Y3 walking onto the symbol opens the sample of that pool's page", False, "there is no symbol to walk onto")
            self.check("Y4 walking off the symbol closes the sample again", False, "there is no symbol to walk onto")
            return
        d = self.walk_to(self.symbol_spot, 10)
        self.wait("!!document.getElementById('lgm-walker').getAttribute('data-peek')", 3)
        time.sleep(0.5)
        p3 = self.peek()
        self.shot("symbol-on")
        self.check("Y3 walking onto the symbol opens the sample of that pool's page",
                   d is not None and d <= 10 and p3.get("id") == POOL and p3.get("open"), {"distance px": d, "sample": p3})

        def off():   # toward the pool's name, well away from the symbol
            m = ((self.j(self.WALK) or {}).get("mids") or {}).get(POOL)
            return [m[0], m[1] - 20] if m else self.symbol_spot()
        self.walk_to(off, 12)
        spot2, sym2 = (self.j(self.WALK) or {}).get("spot"), self.symbol_spot()
        apart = bool(spot2 and sym2) and math.hypot(spot2[0] - sym2[0], spot2[1] - sym2[1]) > 45
        time.sleep(1.2)
        p4 = self.peek()
        self.check("Y4 walking off the symbol closes the sample again",
                   apart and not p4.get("id") and not p4.get("open"), {"away from symbol": apart, "sample": p4})


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
        proc, r.ws = cdp.launch_edge("lg-walker-symbol-")
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
        print("VERDICT walker-page-symbol: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
