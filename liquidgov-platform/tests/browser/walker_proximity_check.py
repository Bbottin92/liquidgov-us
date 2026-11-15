"""Spatial step 4 (SPATIAL_EVOLUTION.md): the proximity filter on the map. Zoomed in (the walking zoom), only the box
the walker is at (Inside) and the boxes connected to it (Near) are shown; every other box (Far) and its connectors are
hidden. Zoom out yourself and the full map shows again.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph,
and walker_instance_check.py's touchpad walk, reused as they are. "Hidden" is the computed visibility of a box.

  F1  the map loaded (8 boxes); standing on the you box every box is connected to it, so none is hidden
  F2  walk onto the friends box and stay (it opens): layers outward from there (2026-11-15, Brandon: "3 or 4 layers of
      pools and streams", rings outward from wherever you are): friends 0, you and the friends 1 (clear), the six other
      kinds 2 (half there); nothing near is hidden
  F3  zoomed out by you (the wheel): every box and connector is shown at full strength again

2026-11-15 (Brandon's OK): walking is always on, so the Scroll cases (old F0, F3) became "zoom out yourself".

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
        "  edgesHidden: edges.filter(e => !vis(e)).length, edges: edges.length,"
        "  layer: Object.fromEntries(boxes.map(b => [b.getAttribute('data-id'), [b.getAttribute('data-layer') || '', b.style.opacity || '1']]))}); })()")


class Run(wi.Run):
    def seen(self):
        return self.j(SEEN) or {}

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        time.sleep(0.6)   # the camera glides in
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        s = self.seen()
        everything = s.get("shown", [])
        self.check("F1 the map loaded (8 boxes); standing on the you box every box is connected to it, so none is hidden",
                   ok and len(everything) == 8 and not s.get("hidden") and s.get("edgesHidden") == 0, s)
        st = self.hop("mine:friends")
        self.open_here()       # staying in a pool opens it: the friends spread round it
        time.sleep(0.5)
        s = self.seen()
        lay = s.get("layer") or {}
        sib = sorted(set(everything) - {"you", "mine:friends"})
        friends = sorted(i for i in lay if i.startswith("user:"))
        self.check("F2 on the friends box, layers (Brandon 2026-11-15, rings outward from where you are): friends layer 0, "
                   "you and the friends next to it clear (layer 1), the six other kinds two channels away half there (layer 2)",
                   st.get("at") == "mine:friends" and lay.get("mine:friends") == ["0", "1"] and lay.get("you") == ["1", "1"]
                   and len(friends) >= 1 and all(lay[f] == ["1", "1"] for f in friends)
                   and len(sib) == 6 and all(lay.get(b) == ["2", "0.5"] for b in sib) and not s.get("hidden"),
                   {"at": st.get("at"), "layer": lay})
        # zoom out yourself (the wheel over the map; a pinch does the same, walker_controls_check Z2): the whole map shows
        f = (self.j(wc.MAPZ) or {}).get("frame") or {}
        x, y = (f.get("l", 0) + f.get("r", 0)) / 2, max(f.get("t", 0), 0) + 60
        for _ in range(12):
            self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=x, y=y, deltaX=0, deltaY=100)
            time.sleep(0.05)
        time.sleep(0.8)
        s = self.seen()
        self.check("F3 zoomed out by you: every box and connector is shown at full strength, still standing on friends",
                   set(everything) <= set(s.get("shown", [])) and not s.get("hidden") and s.get("edgesHidden") == 0
                   and all(v == ["", "1"] for v in (s.get("layer") or {}).values())
                   and self.state().get("at") == "mine:friends", s)


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
