"""Spatial step 4 (SPATIAL_EVOLUTION.md): the Drop-mode proximity filter on the map. While Drop is on, only the box the
walker stands on (Inside) and the boxes connected to it (Near) are shown; every other box (Far) and its connectors
are hidden. Scroll mode shows the full map again. New file: no committed check is edited.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph,
and walker_instance_check.py's touchpad walk, reused as they are. "Hidden" is the computed visibility of a box.

  F0  precondition: the map loaded; in Scroll mode no box is hidden (8 boxes)
  F1  Drop on, standing on the you box: every box is connected to it, so none is hidden
  F2  walk onto the friends box: you and friends stay shown, the six other boxes are hidden, and so is every
      connector to a hidden box
  F3  back in Scroll mode: every box and connector is shown again

Usage: python3 walker_proximity_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-proximity: ALL-PASS" or the failures; exit 1 on a FAIL."""
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

SEEN = ("(() => { const vis = e => getComputedStyle(e).visibility !== 'hidden';"
        " const boxes = [...document.querySelectorAll('#lgm-nodes .lgn')].filter(b => b.style.display !== 'none');"
        " const edges = [...document.querySelectorAll('#lgm-edges .lge')].filter(e => e.style.display !== 'none');"
        " const hiddenBoxes = boxes.filter(b => !vis(b)).map(b => b.getAttribute('data-id')).sort();"
        " return JSON.stringify({shown: boxes.filter(vis).map(b => b.getAttribute('data-id')).sort(), hidden: hiddenBoxes,"
        "  edgesShown: edges.filter(vis).map(e => [e.getAttribute('data-a'), e.getAttribute('data-b')]),"
        "  edgesHidden: edges.filter(e => !vis(e)).length, edges: edges.length}); })()")


class Run(wi.Run):
    def seen(self):
        return self.j(SEEN) or {}

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        s = self.seen()
        everything = s.get("shown", [])
        self.check("F0 precondition: the map loaded; in Scroll mode no box is hidden (8 boxes)",
                   ok and len(everything) == 8 and not s.get("hidden") and s.get("edgesHidden") == 0, s)
        self.map_walking()
        time.sleep(0.6)   # the camera glides in
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        s = self.seen()
        self.check("F1 Drop on, standing on the you box: every box is connected to it, so none is hidden",
                   s.get("shown") == everything and not s.get("hidden"), s)
        st = self.hop("mine:friends")
        time.sleep(0.3)
        s = self.seen()
        far = sorted(set(everything) - {"you", "mine:friends"})
        self.check("F2 on the friends box: you and friends shown, the six other boxes and their connectors hidden",
                   st.get("at") == "mine:friends" and s.get("shown") == ["mine:friends", "you"] and s.get("hidden") == far
                   and len(far) == 6 and all(set(e) <= {"you", "mine:friends"} for e in s.get("edgesShown", []))
                   and s.get("edgesHidden") == 6, {"at": st.get("at"), "seen": s})
        self.drop_off()
        s = self.seen()
        self.check("F3 back in Scroll mode: every box and connector is shown again",
                   s.get("shown") == everything and not s.get("hidden") and s.get("edgesHidden") == 0, s)


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
        proc, r.ws = cdp.launch_edge("lg-walker-proximity-")
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
        print("VERDICT walker-proximity: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
