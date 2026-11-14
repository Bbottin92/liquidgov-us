"""Spatial step 3 (SPATIAL_EVOLUTION.md): the map walker's instanceId. Walking onto a box sets it to that box; back at
the you box, or in Scroll mode, it is the parent, "mainstream". New file: no committed check is edited.

Real headless Chromium at phone size (the touchpad can push straight at any box), NO C++ server: walker_controls_check.py's stdlib server and fake graph (the real lgmap.js,
walker.js and lgmap_walk.js), reused as they are. The walker's instance is read from #lgm-walker[data-instance].

  I0  precondition: the map loaded; in Scroll mode the instance is mainstream
  I1  Drop on, standing on the you box: still mainstream
  I2  walk onto a profile box (kind person): the instance is that box's id
  I3  walk off it: the instance leaves the profile box; back at the you box it is mainstream again
  I4  the Drop/Scroll switch still works: back in Scroll mode the walker is hidden and the instance is mainstream

Usage: python3 walker_instance_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-instance: ALL-PASS" or the failures; exit 1 on a FAIL."""
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

STATE = ("(() => { const w = document.getElementById('lgm-walker');"
         " const boxes = [...document.querySelectorAll('#lgm-nodes .lgn')].filter(b => b.style.display !== 'none').map(b => {"
         "  const r = b.getBoundingClientRect(); return {id: b.getAttribute('data-id'), person: b.classList.contains('lgn-person'),"
         "   x: r.left + r.width / 2, y: r.top + r.height / 2}; });"
         " const edges = [...document.querySelectorAll('#lgm-edges .lge')].filter(e => e.style.display !== 'none')"
         "  .map(e => [e.getAttribute('data-a'), e.getAttribute('data-b')]);"
         " return JSON.stringify({at: w ? w.getAttribute('data-at') : null, hidden: !w || w.hidden,"
         "  instance: w ? w.getAttribute('data-instance') : null, boxes: boxes, edges: edges}); })()")


class Run(wc.Run):
    def state(self):
        return self.j(STATE) or {}

    def hop(self, target):
        """One push of the touchpad straight toward box `target` (the walker takes the connector that points that way)."""
        st = self.state()
        mid = {b["id"]: b for b in st.get("boxes", [])}
        if st.get("at") not in mid or target not in mid:
            return st
        dx, dy = mid[target]["x"] - mid[st["at"]]["x"], mid[target]["y"] - mid[st["at"]]["y"]
        n = math.hypot(dx, dy) or 1
        self.pull(self.tp, dx / n * self.reach, dy / n * self.reach, 0.25)   # under mapRepeatMs: one connector
        time.sleep(0.4)
        return self.state()

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        st = self.state()
        self.check("I0 precondition: the map loaded; in Scroll mode the instance is mainstream",
                   ok and st.get("hidden") and st.get("instance") == "mainstream", {k: st.get(k) for k in ("at", "hidden", "instance")})
        self.map_walking()
        time.sleep(0.6)   # the camera glides in
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        st = self.state()
        self.check("I1 Drop on, standing on the you box: still mainstream",
                   bool(self.tp) and st.get("at") == "you" and not st.get("hidden") and st.get("instance") == "mainstream",
                   {k: st.get(k) for k in ("at", "hidden", "instance")})
        # the profile boxes are behind the friends box: walk there, open it (Enter), then walk onto a friend
        self.hop("mine:friends")
        self.key("enter")
        self.wait("!!document.querySelector('#lgm-nodes .lgn-person')", 5)
        time.sleep(0.8)
        people = [b["id"] for b in self.state().get("boxes", []) if b.get("person")]
        st = self.hop(people[0]) if people else self.state()
        self.check("I2 walk onto a profile box: the instance is that box's id",
                   bool(people) and st.get("at") == people[0] and st.get("instance") == people[0],
                   {"at": st.get("at"), "instance": st.get("instance"), "people": people})
        off = self.hop("mine:friends")       # off the profile box, onto the box it hangs from
        self.key("escape")                    # back a level: the you box is on the map again
        self.wait("!!document.querySelector('#lgm-nodes .lgn[data-id=\"you\"]')", 5)
        time.sleep(0.8)
        st = self.hop("you")
        self.check("I3 walk off it: the instance leaves the profile box, and back at the you box it is mainstream again",
                   bool(people) and off.get("instance") == "mine:friends" and st.get("at") == "you" and st.get("instance") == "mainstream",
                   {"off": {k: off.get(k) for k in ("at", "instance")}, "you": {k: st.get(k) for k in ("at", "instance")}})
        before = self.hop("mine:friends")     # stand on a box again, then switch to Scroll
        self.drop_off()
        st = self.state()
        self.check("I4 back in Scroll mode the walker is hidden and the instance is mainstream",
                   before.get("instance") == "mine:friends" and st.get("hidden") and st.get("instance") == "mainstream",
                   {"before": before.get("instance"), "hidden": st.get("hidden"), "instance": st.get("instance")})

    def drop_off(self):
        self.j("(() => { document.querySelector('.' + LG_WALKER.CONTROLS.buttons.drop.cls).click(); return '1'; })()")
        self.wait("document.getElementById('lgm-walker').hidden", 3)
        time.sleep(0.3)


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
        proc, r.ws = cdp.launch_edge("lg-walker-instance-")
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
        print("VERDICT walker-instance: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
