"""Spatial step 3 (SPATIAL_EVOLUTION.md): the map walker's instanceId. Walking onto a box sets it to that box; back at
your own pool it is user:<you> (your own place, 2026-11-15); between boxes it is the shared "mainstream".

Real headless Chromium at phone size (the touchpad can push straight at any box), NO C++ server: walker_controls_check.py's stdlib server and fake graph (the real lgmap.js,
walker.js and lgmap_walk.js), reused as they are. The walker's instance is read from #lgm-walker[data-instance].

  I1  the map loaded and the walker stands in your own pool: user:<you>
  I2  walk onto a profile box (kind person): the instance is that box's id
  I3  walk off it: the instance leaves the profile box; back in your own pool it is user:<you> again
  I4  between boxes (in no box) the walker is still shown and the instance is mainstream

2026-11-15 (Brandon's OK to update the checks that test the switch or the hopping): walking is always on and free, so
the Scroll cases (old I0, I4) are gone, and hop() walks until it is inside the box instead of one push per connector.

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

    def hop(self, target, limit_s=14):
        """hop_once, and if the walker stopped short of the box, twice more: a person who stops short tries again. A busy
        machine can end one walk early (flakes in walker_valve_check V2, walker_valve_types_check Y2/Y3 and
        walker_controls_check M5, 2026-11-16, each passing alone). A box whose valve turns the walker away is still
        not entered however often it tries, so the refusal checks are unchanged."""
        st = self._hop_once(target, limit_s)
        for _ in range(2):
            if st.get("at") == target:
                break
            st = self._hop_once(target, limit_s)
        return st

    def _hop_once(self, target, limit_s=14):
        self.settle()   # (walker_controls_check.Run.settle: wait until the map is still before walking)
        """Walk to box `target` with the touchpad (free walking, 2026-11-15; it used to be one push along a connector):
        pulls straight at its middle until the walker is inside it, or cannot get any closer (a valve turned it away
        at the box's edge), or the time is up. Returns the state then. The checks that inherit this keep calling it
        hop()."""
        end, best, stuck = time.time() + limit_s, None, 0
        # first to the middle of the box it stands in, so it leaves through the channel's mouth (a pull straight at the
        # target from off to one side meets the bank: dry land stops the drop since 2026-11-15)
        st = self.j(self.WALK) or {}
        if st.get("at"):
            self.last_box = st["at"]
        here = st.get("at") or getattr(self, "last_box", "")   # in no box (turned away at a valve): back to the last one
        if here and here != target:
            for _ in range(30):
                st = self.j(self.WALK) or {}
                mid, spot = (st.get("mids") or {}).get(here), st.get("spot")
                if not mid or not spot or not self.tp:
                    break
                dx, dy = mid[0] - spot[0], mid[1] - spot[1]
                n = math.hypot(dx, dy)
                if n < 6:
                    break
                self.pull(self.tp, dx / n * self.reach, dy / n * self.reach, max(0.05, min(0.3, n / 175.0)))
        while time.time() < end:
            st = self.j(self.WALK) or {}
            to, spot = (st.get("mids") or {}).get(target), st.get("spot")
            if st.get("at") == target or not to or not spot or not self.tp:
                break
            dx, dy = to[0] - spot[0], to[1] - spot[1]
            n = math.hypot(dx, dy) or 1
            stuck = stuck + 1 if best is not None and n > best - 2 else 0
            if stuck >= 6:   # the camera can still be gliding after a level change: a few looks that seem stuck are not
                break
            best = n if best is None else min(best, n)
            self.pull(self.tp, dx / n * self.reach, dy / n * self.reach, max(0.08, min(0.4, n / 175.0)))
        time.sleep(0.3)
        st = self.state()
        if st.get("at"):
            self.last_box = st["at"]
        return st

    def step_off(self, toward, limit_s=6):
        """Short pulls toward box `toward` until the walker is in no box (data-at ''), or the time is up."""
        end = time.time() + limit_s
        while time.time() < end:
            st = self.j(self.WALK) or {}
            to, spot = (st.get("mids") or {}).get(toward), st.get("spot")
            if st.get("at") == "" or not to or not spot or not self.tp:
                break
            dx, dy = to[0] - spot[0], to[1] - spot[1]
            n = math.hypot(dx, dy) or 1
            self.pull(self.tp, dx / n * self.reach, dy / n * self.reach, 0.08)
        return self.state()

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        time.sleep(0.6)   # the camera glides in
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        st = self.state()
        home = "user:" + wc.m2.ME   # your own pool is your own place (Brandon 2026-11-15), not the shared mainstream
        self.check("I1 the map loaded and the walker stands in your own pool: the instance is user:<you>",
                   ok and bool(self.tp) and st.get("at") == "you" and not st.get("hidden") and st.get("instance") == home,
                   {k: st.get(k) for k in ("at", "hidden", "instance")})
        # the profile boxes are behind the friends box: walk there, open it (Enter), then walk onto a friend
        self.hop("mine:friends")
        self.open_here()
        self.wait("!!document.querySelector('#lgm-nodes .lgn-person')", 5)
        time.sleep(0.8)
        people = [b["id"] for b in self.state().get("boxes", []) if b.get("person")]
        st = self.hop(people[0]) if people else self.state()
        self.check("I2 walk onto a profile box: the instance is that box's id",
                   bool(people) and st.get("at") == people[0] and st.get("instance") == people[0],
                   {"at": st.get("at"), "instance": st.get("instance"), "people": people})
        off = self.hop("mine:friends")       # off the profile box, onto the box it hangs from
        # one world (2026-11-15): the you box never leaves the map, so you swim back to it (no Back needed; Back would
        # also step back through the pools you opened by staying in them)
        time.sleep(0.8)
        time.sleep(0.6)   # the camera settles after going back
        st = self.hop("you")
        self.check("I3 walk off it: the instance leaves the profile box, and back in your own pool it is user:<you> again",
                   bool(people) and off.get("instance") == "mine:friends" and st.get("at") == "you" and st.get("instance") == home,
                   {"off": {k: off.get(k) for k in ("at", "instance")}, "you": {k: st.get(k) for k in ("at", "instance")}})
        time.sleep(0.6)
        before = self.hop("mine:friends")     # stand in a box again, then step out of it onto the open map
        st = self.step_off("you")
        self.check("I4 between boxes (in no box) the walker is still shown and the instance is mainstream",
                   before.get("instance") == "mine:friends" and st.get("at") == "" and not st.get("hidden")
                   and st.get("instance") == "mainstream",
                   {"before": before.get("instance"), "at": st.get("at"), "hidden": st.get("hidden"), "instance": st.get("instance")})


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
