"""Spatial step 6 (SPATIAL_EVOLUTION.md): every map box has a valve, Open by default, and the walker's step onto a box
(the entry path) goes through it. The regression check: every box on the map can still be entered.
New file: no committed check is edited.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph,
and walker_instance_check.py's touchpad walk, reused as they are.

  V0  precondition: the map loaded and every box carries an Open valve (data-valve="open")
  V1  every box around you can be entered: the walker walks onto each of them in turn (and back to you)
  V2  positive control, the entry path really asks the valve: a box whose valve is set to something other than
      open (in the page, by this check) is NOT entered; set back to open, it is entered again

Usage: python3 walker_valve_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-valve: ALL-PASS" or the failures; exit 1 on a FAIL."""
import http.server
import json
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

VALVES = ("JSON.stringify([...document.querySelectorAll('#lgm-nodes .lgn')].map(b => [b.getAttribute('data-id'),"
          " b.getAttribute('data-valve')]))")


class Run(wi.Run):
    def set_valve(self, box_id, kind):
        self.j("(() => { document.querySelector('#lgm-nodes .lgn[data-id=' + JSON.stringify(%s) + ']')"
               ".setAttribute('data-valve', %s); return '1'; })()" % (json.dumps(box_id), json.dumps(kind)))

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        valves = self.j(VALVES) or []
        self.check("V0 precondition: the map loaded and every box carries an Open valve",
                   ok and len(valves) == 8 and all(v == "open" for _, v in valves), valves)
        self.map_walking()
        time.sleep(0.6)   # the camera glides in
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        around = [i for i, _ in valves if i != "you"]
        entered = {}
        for box_id in around:
            entered[box_id] = self.hop(box_id).get("at")
            self.hop("you")
        self.check("V1 every box around you can be entered (%d boxes)" % len(around),
                   len(around) == 7 and all(entered[i] == i for i in around), entered)
        target = around[0] if around else "none"
        self.set_valve(target, "closed")
        shut = self.hop(target).get("at")
        self.hop("you")
        self.set_valve(target, "open")
        again = self.hop(target).get("at")
        self.check("V2 positive control: a box whose valve is not open is not entered; open again, it is",
                   shut == "you" and again == target, {"box": target, "valve closed -> at": shut, "valve open -> at": again})


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
        proc, r.ws = cdp.launch_edge("lg-walker-valve-")
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
        print("VERDICT walker-valve: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
