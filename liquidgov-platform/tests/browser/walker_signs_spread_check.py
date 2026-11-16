"""Channel mouths are spread round a crowded pool and crowded names give way (Brandon 2026-11-16; follows
walker_signs_check.py, which stays as committed).

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, ten extra
groups under Groups (as walker_signs_check.py), the walker on (pools), Groups opened.

  S0  precondition: Groups is open, its pool has at least 8 channels with a sign each, and the pool's middle is known
  S1  no two channels leave the Groups pool in (nearly) the same direction: every pair of mouths is at least 8 degrees
      apart as seen from the pool's middle (before: overflow children shared an angle)
  S2  names give way: two mouths closer than 36 map units never both show a name; every SHOWN sign is at least 3
      characters and no two shown signs overlap (the farther one is hidden, not cut to a fragment)
  S3  the spread is even, not just distinct: the largest gap between neighbouring mouths is at most 4 times the
      average gap (the golden-angle ordering fills the shore, it does not leave one stretch bare)

Usage: python3 walker_signs_spread_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-signs-spread: ALL-PASS" or the failures; exit 1 on a FAIL."""
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
import walker_signs_check as ws  # noqa: E402,F401  (adds the ten extra groups to the fake graph on import)

# every sign of the Groups pool: text, x, y, shown (display not none), text width, plus the pool's own middle
PROBE = ("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=\"mine:groups\"]'); if (!g) return null;"
         " const s = g.querySelector('.lgs'); if (!s) return null;"
         " return JSON.stringify({cx: +s.getAttribute('cx'), cy: +s.getAttribute('cy'), signs: [...g.querySelectorAll('.lge-sign')].map(t => "
         "[t.textContent, +t.getAttribute('x'), +t.getAttribute('y'), t.style.display !== 'none', t.getComputedTextLength()])}); })()")


class Run(wi.Run):
    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        self.j("LG_MAP.goTo('mine:groups')")
        time.sleep(2.5)
        raw = self.j(PROBE)
        data = raw if isinstance(raw, dict) else None   # self.j() already parses the JSON text
        signs = data["signs"] if data else []
        self.check("S0 precondition: Groups is open, at least 8 channels each with a sign, the pool's middle is known",
                   ok and data is not None and len(signs) >= 8, {"loaded": ok, "signs": len(signs)})
        if not data or len(signs) < 8:
            return
        ang = sorted(math.degrees(math.atan2(s[2] - data["cy"], s[1] - data["cx"])) % 360 for s in signs)
        gaps = [(ang[(i + 1) % len(ang)] - ang[i]) % 360 for i in range(len(ang))]
        self.check("S1 no two channels leave the Groups pool in nearly the same direction (every gap at least 8 degrees)",
                   min(gaps) >= 8.0, {"smallest gap": round(min(gaps), 1), "angles": [round(a) for a in ang]})
        bad_close, bad_short, bad_over = [], [], []
        for i, a in enumerate(signs):
            if a[3] and len(a[0].strip()) < 3:
                bad_short.append(a[0])
            for b in signs[i + 1:]:
                d = math.hypot(a[1] - b[1], a[2] - b[2])
                if a[3] and b[3]:
                    if d < 36:
                        bad_close.append((a[0], b[0], round(d)))
                    if d < (a[4] + b[4]) / 2.0:
                        bad_over.append((a[0], b[0], round(d)))
        self.check("S2 names give way: close mouths never both show a name; shown signs are 3+ characters and do not overlap",
                   not bad_close and not bad_short and not bad_over,
                   {"both shown but close": bad_close, "short": bad_short, "overlap": bad_over,
                    "shown": sum(1 for s in signs if s[3]), "of": len(signs)})
        avg = 360.0 / len(gaps)
        self.check("S3 the spread is even: the largest gap is at most 4 times the average gap",
                   max(gaps) <= 4 * avg, {"largest gap": round(max(gaps), 1), "average": round(avg, 1)})


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
        proc, r.ws = cdp.launch_edge("lg-walker-spread-")
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
        print("VERDICT walker-signs-spread: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
