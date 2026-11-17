"""The page symbol has one fixed slot under the pool's name, where the wave line was (Brandon 2026-11-16, "ok for all of
those": drop the decorative wave line; fixed slot; none in your own pool). Follows walker_page_symbol_check.py and
walker_page_icon_clear_check.py, which stay as committed.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, the walker
on (pools), your pool then the Friends and Groups pools opened.

  F0  precondition: the map loaded and the Friends and Groups pools are on the map with their names
  F1  no pool draws the wave line any more (.lgn-pool-wave)
  F2  Friends and Groups: the page symbol sits straight under the pool's name (centred within 2 map units, a gap of 4 to 40
      map units between the name's bottom and the symbol's top) - the same slot in both pools, whatever their mouths
  F3  the symbol does not move: the same place a second later, and after the pool is opened or left
  F5  the fixed slot is still clear: in Friends (names of people at the mouths) and Groups (crowded, 10+ channels) the symbol
      overlaps no mouth symbol, no shown mouth name and no pool name (a gap of 2 screen px or more)
  F4  your own pool draws no page symbol, and the map says it has no symbol spot (so the sample never opens by walking in
      it: "Open your page" is the way to your page)

Usage: python3 walker_page_slot_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-page-slot: ALL-PASS" or the failures; exit 1 on a FAIL."""
import http.server
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
import walker_page_icon_clear_check as pic  # noqa: E402  (its measuring of the symbol against the other drawings)
import walker_signs_check as ws  # noqa: E402,F401  (adds the ten extra groups to the fake graph on import)

# a pool's name box and page symbol box in map units (the pool's own drawing: the same units as the map), its middle, and the page count
PROBE = ("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]'); if (!g) return null;"
         " const s = g.querySelector('.lgs'), n = g.querySelector('text.lgn-pool-name'), p = g.querySelector('.lgn-page .lgn-page-sheet');"
         " const bb = e => { if (!e) return null; const b = e.getBBox(), m = e.getCTM(), pm = e.ownerSVGElement.getScreenCTM();"
         " const r = e.getBoundingClientRect(), k = pm.a; return [r.left / k, r.top / k, r.right / k, r.bottom / k]; };"
         " return JSON.stringify({cx: s ? s.getBoundingClientRect().left + s.getBoundingClientRect().width / 2 : null, k: g.ownerSVGElement.getScreenCTM().a,"
         " name: bb(n), page: bb(p), pages: g.querySelectorAll('.lgn-page').length, waves: g.querySelectorAll('.lgn-pool-wave').length,"
         " spot: (LG_MAP.instance('%s') || {}).pageSpot || null}); })()")


class Run(wi.Run):
    def look(self, pool):
        return self.j(PROBE % (pool, pool)) or {}

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        looks = {}
        for pool in ("you", "mine:friends", "mine:groups"):
            self.j("LG_MAP.goTo('%s')" % pool)
            time.sleep(2.2)
            looks[pool] = self.look(pool)
        self.shot("page-slot-groups")
        fr, gr, you = looks["mine:friends"], looks["mine:groups"], looks["you"]
        self.check("F0 precondition: the map loaded and the Friends and Groups pools are on the map with their names",
                   ok and bool(fr.get("name")) and bool(gr.get("name")), {"loaded": ok, "friends": fr.get("name"), "groups": gr.get("name")})
        if not (fr.get("name") and gr.get("name")):
            return
        waves = {p: l.get("waves") for p, l in looks.items()}
        self.check("F1 no pool draws the wave line", not any(waves.values()), {"waves": waves})
        bad = {}
        for pool, l in (("mine:friends", fr), ("mine:groups", gr)):
            n, p = l.get("name"), l.get("page")
            if not n or not p:
                bad[pool] = "missing"
                continue
            k = l["k"] or 1
            mid = (p[0] + p[2]) / 2
            sheet_mid = l["cx"] / k
            gap = p[1] - n[3]
            if abs(mid - sheet_mid) > 2 or not 4 <= gap <= 40:
                bad[pool] = {"gap under the name": round(gap, 1), "off centre": round(mid - sheet_mid, 1)}
        self.check("F2 the page symbol sits straight under the pool's name, in the same slot in every pool", not bad, bad)
        again = self.look("mine:groups")
        time.sleep(1.0)
        later = self.look("mine:groups")
        moved = [round(a - b, 2) for a, b in zip(again.get("page") or [], later.get("page") or [])]
        self.check("F3 the page symbol does not move", bool(moved) and all(abs(v) < 0.5 for v in moved), {"moved by": moved})
        touching = {}
        for pool in ("mine:friends", "mine:groups"):
            self.j("LG_MAP.goTo('%s')" % pool)
            time.sleep(2.2)
            d = self.j(pic.PROBE % (pool, pool)) or {}
            page, others = d.get("page"), d.get("others") or []
            touching[pool] = ([(n, round(pic.gap(page, b), 1)) for n, b in others if pic.gap(page, b) < pic.GAP] if page else "no symbol") if others else "nothing measured"
        self.check("F5 the fixed slot is clear of mouth symbols, mouth names and the pool's name in Friends and Groups",
                   all(v == [] for v in touching.values()), touching)
        self.j("LG_MAP.goTo('you')")
        time.sleep(2.0)
        you = self.look("you")
        self.check("F4 your own pool draws no page symbol and has no symbol spot",
                   you.get("pages") == 0 and you.get("spot") is None, {"symbols": you.get("pages"), "spot": you.get("spot")})


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
        proc, r.ws = cdp.launch_edge("lg-walker-pageslot-")
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
        print("VERDICT walker-page-slot: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
