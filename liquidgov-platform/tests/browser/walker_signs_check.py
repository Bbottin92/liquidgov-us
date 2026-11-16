"""Names at the mouths of channels never run into each other (Brandon 2026-11-16, a phone screenshot of the live Groups
pool: "all of the group names are jumbled at the top"). Each channel's name is written inside its pool at the mouth,
along the shore (.lge-sign, lgmap_walk.js). With many channels the mouths sit close, and the names (up to 11 characters
at up to 22 px, bold) were wider than the shore between two mouths, so they printed on top of each other.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, with ten
more groups added under Groups (long names among them), the walker on (pools), Groups opened.

  G0  precondition: Groups is open and its pool has at least 8 channels, each with a sign
  G1  no two signs in the Groups pool overlap: for every pair of neighbouring signs on the shore, the distance between
      their centres is at least half the sum of their text widths
  G2  every sign still says something readable: at least 3 characters, none empty

Usage: python3 walker_signs_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-signs: ALL-PASS" or the failures; exit 1 on a FAIL."""
import http.server
import json
import math
import os
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import map_live_check2 as m2  # noqa: E402
import walker_controls_check as wc  # noqa: E402
import walker_instance_check as wi  # noqa: E402

NAMES = ["The Gov", "LiquidGov Think Tank", "Roseville", "The Great Recall", "Tim's Garage Band", "Tin Foil Hat Society",
         "Neighbors Helping Neighbors", "River Cleanup Crew", "Skateboarding", "Open Source Gardeners"]
for i, name in enumerate(NAMES):
    gid = "group:many%d" % i
    m2.NODES[gid] = m2.N(gid, name, "group", "/c/many%d" % i, 1)
    m2.LINKS[gid] = []
m2.LINKS["mine:groups"] = ["you"] + ["group:many%d" % i for i in range(len(NAMES))]

SIGNS = ("JSON.stringify([...document.querySelectorAll('#lgm-nodes .lgn[data-id=\"mine:groups\"] .lge-sign')].map(t => "
         "[t.textContent, +t.getAttribute('x'), +t.getAttribute('y'), t.getComputedTextLength(), parseFloat(t.style.fontSize) || 0]))")


class Run(wi.Run):
    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        self.j("LG_MAP.goTo('mine:groups')")
        time.sleep(2.5)   # the channels grow, the water pass (every 0.4 s) writes the signs
        signs = self.j(SIGNS) or []
        self.check("G0 precondition: Groups is open and its pool has at least 8 channels, each with a sign",
                   ok and len(signs) >= 8, {"loaded": ok, "signs": len(signs)})
        worst = None
        for i, a in enumerate(signs):
            for b in signs[i + 1:]:
                d = math.hypot(a[1] - b[1], a[2] - b[2])
                need = (a[3] + b[3]) / 2.0
                # two signs far apart round the shore are not neighbours; only those closer than their own widths matter
                if d < need * 1.0 and (worst is None or d - need < worst[0]):
                    worst = (round(d - need, 1), a[0], b[0])
        self.check("G1 no two signs in the Groups pool overlap (centres at least half the sum of the widths apart)",
                   len(signs) >= 8 and worst is None, {"worst overlap": worst, "signs": [(s[0], round(s[3]), s[4]) for s in signs]})
        self.check("G2 every sign still says something readable (3 characters or more)",
                   len(signs) >= 8 and all(len(s[0].strip()) >= 3 for s in signs), [s[0] for s in signs])


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
        proc, r.ws = cdp.launch_edge("lg-walker-signs-")
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
        print("VERDICT walker-signs: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
