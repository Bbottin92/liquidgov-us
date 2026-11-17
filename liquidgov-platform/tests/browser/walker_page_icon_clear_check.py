"""The page symbol never sits on a mouth symbol, a mouth name, the pool's name or wave, or "Open your page" (Brandon
2026-11-16, seeing the kind-marks screenshot: in his own pool the page icon sat right under "Open your page" and over the
Groups sign). Follows walker_page_symbol_check.py and walker_kind_marks_check.py, which stay as committed.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, the walker
on (pools), ten extra groups under Groups (as walker_signs_check.py) so that pool is crowded. Every box is measured on
the screen (getBoundingClientRect): the page symbol (.lgn-page) against each mouth symbol (.lge-mark), each SHOWN mouth
name (.lge-sign), the pool's name and wave, and (your pool) the "Open your page" button.

  P0  precondition: the map loaded, and the pool being measured has a page symbol and at least 3 things to keep clear of
  P1  your own pool ('you'): the page symbol overlaps none of them (a gap of at least 2 screen px)
  P2  the Groups pool (crowded: 10+ channels): the same
  P3  the Friends pool: the same, and the sample still opens only over the symbol: standing on the symbol's own middle
      opens it (the symbol the eye sees is the one the walker uses)

Usage: python3 walker_page_icon_clear_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-page-icon-clear: ALL-PASS" or the failures; exit 1 on a FAIL."""
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
import walker_page_symbol_check as wps  # noqa: E402
import walker_signs_check as ws  # noqa: E402,F401  (adds the ten extra groups to the fake graph on import)

# the page symbol's box and every box it must keep clear of, on the screen, for pool `%s`
PROBE = ("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]'); if (!g) return null;"
         " const pg = g.querySelector('.lgn-page'); if (!pg) return JSON.stringify({page: null, others: []});"
         " const box = r => [r.left, r.top, r.right, r.bottom], others = [];"
         " g.querySelectorAll('text.lgn-pool-name,.lgn-pool-wave,.lge-mark,.lge-sign').forEach(n => { if (n.style.display === 'none') return;"
         " const r = n.getBoundingClientRect(); if (r.width > 0 && r.height > 0) others.push([n.getAttribute('class') + ':' + (n.getAttribute('data-for') || n.textContent || ''), box(r)]); });"
         " if ('%s' === 'you') document.querySelectorAll('#lgm-nodes .lgn-openbtn').forEach(n => others.push(['openbtn', box(n.getBoundingClientRect())]));"
         " return JSON.stringify({page: box(pg.querySelector('.lgn-page-sheet').getBoundingClientRect()), others: others}); })()")
GAP = 2.0   # screen px of clear water between the symbol and anything else


def gap(a, b):
    dx = max(b[0] - a[2], a[0] - b[2], 0)
    dy = max(b[1] - a[3], a[1] - b[3], 0)
    return math.hypot(dx, dy)


class Run(wps.Run):
    def clear_of(self, pool):
        d = self.j(PROBE % (pool, pool)) or {}
        page, others = d.get("page"), d.get("others") or []
        hits = [(n, round(gap(page, b), 1)) for n, b in others if gap(page, b) < GAP] if page else []
        return page, others, hits

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        self.j("LG_MAP.goTo('you')")
        time.sleep(2.5)
        page, others, hits = self.clear_of("you")
        self.shot("page-icon-you")
        self.check("P0 precondition: the map loaded; your pool has a page symbol and at least 3 things to keep clear of",
                   ok and page is not None and len(others) >= 3, {"loaded": ok, "page": page, "others": len(others)})
        if not page:
            return
        self.check("P1 your own pool: the page symbol overlaps no mouth symbol or name, no pool name or wave, not \"Open your page\"",
                   not hits, {"touching": hits})
        self.j("LG_MAP.goTo('mine:groups')")
        time.sleep(2.5)
        page, others, hits = self.clear_of("mine:groups")
        self.shot("page-icon-groups")
        self.check("P2 the Groups pool (crowded): the page symbol overlaps nothing",
                   page is not None and len(others) >= 8 and not hits, {"page": page, "others": len(others), "touching": hits})
        st = self.map_go(wps.POOL)
        time.sleep(2.0)
        page, others, hits = self.clear_of(wps.POOL)
        self.shot("page-icon-friends")
        spot = self.symbol_spot()
        d = self.walk_to(self.symbol_spot, 10) if spot else None
        self.wait("!!document.getElementById('lgm-walker').getAttribute('data-peek')", 3)
        p = self.peek()
        self.check("P3 the Friends pool: the page symbol overlaps nothing, and standing on it opens the sample",
                   st.get("at") == wps.POOL and not hits and d is not None and d <= 10 and p.get("id") == wps.POOL and p.get("open"),
                   {"at": st.get("at"), "touching": hits, "distance px": d, "sample": p})


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
        proc, r.ws = cdp.launch_edge("lg-walker-pageicon-")
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
        print("VERDICT walker-page-icon-clear: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
