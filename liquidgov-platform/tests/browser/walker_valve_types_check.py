"""Spatial step 7 (SPATIAL_EVOLUTION.md): the valve types on the map walker's entry path. Three focused cases
(Open, Private, Conduit) and the inheritance rule. New file: no committed check is edited.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph,
and walker_instance_check.py's touchpad walk. The valves come in through the graph's data (the fake graph's
nodes are edited in this process before each page load), the way a real box's data would carry one.
The meanings (Brandon 2026-11-14): open: anyone; closed: no one; private: only the owner; invite: only people on the
list; request: turned away, recorded for the owner; conduit: walked across, never stood IN. A new
box copies its parent's valve unless its own data names one.

  Y0  precondition: the map loaded; boxes with no valve in their data are Open
  Y1  Open: the walker enters an Open box
  Y2  Private: a box private to someone else is not entered; private to you, it is entered
  Y3  inheritance: the boxes opened from that private-to-you box carry a copy of its valve (private, same owner)
  Y4  Conduit: you walk across a conduit box but are never in it (standing over it you are in no box, mainstream);
      positive control: as an Open box, standing in the same place, you are in it

2026-11-15 (Brandon's OK): free walking. A refused box leaves you at its edge in no box (data-at '', it used to be the
box you came from), and a conduit is walked across where one push used to carry you through it to the box beyond.

Usage: python3 walker_valve_types_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-valve-types: ALL-PASS" or the failures; exit 1 on a FAIL."""
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

ATTRS = ("(() => JSON.stringify(Object.fromEntries([...document.querySelectorAll('#lgm-nodes .lgn')].map(b => [b.getAttribute('data-id'),"
         " [b.getAttribute('data-valve'), b.getAttribute('data-valve-owner')]]))))()")


class Run(wi.Run):
    def load(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        time.sleep(0.6)   # the camera glides in
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        return ok

    OVER = ("(() => { const w = document.getElementById('lgm-walker').getBoundingClientRect(),"
            " b = document.querySelector('#lgm-nodes .lgn[data-id=\"mine:friends\"]').getBoundingClientRect(),"
            " x = w.left + w.width / 2, y = w.top + w.height / 2;"
            " return JSON.stringify(x > b.left + 4 && x < b.right - 4 && y > b.top + 4 && y < b.bottom - 4); })()")

    def set_valve(self, box_id, kind):
        self.j("(() => { document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]').setAttribute('data-valve', '%s'); return '1'; })()"
               % (box_id, kind))

    def cases(self):
        nodes = wc.m2.NODES
        me = wc.m2.ME
        for n in nodes.values():
            n.pop("valve", None)
        ok = self.load()
        attrs = self.j(ATTRS) or {}
        self.check("Y0 precondition: the map loaded; boxes with no valve in their data are Open",
                   ok and len(attrs) == 8 and all(v[0] == "open" for v in attrs.values()), attrs)
        st = self.hop("mine:politics")
        self.check("Y1 Open: the walker enters an Open box", st.get("at") == "mine:politics", {"at": st.get("at")})

        nodes["mine:politics"]["valve"] = {"type": "private", "owner": "someone-else"}
        nodes["mine:friends"]["valve"] = {"type": "private", "owner": me}
        self.load()
        refused = self.hop("mine:politics").get("at")
        entered = self.hop("mine:friends").get("at")
        self.check("Y2 Private: a box private to someone else is not entered (you stop at its edge, in no box); private to you, it is entered",
                   refused == "" and entered == "mine:friends", {"private to someone else -> at": refused, "private to you -> at": entered})
        self.open_here()
        self.wait("!!document.querySelector('#lgm-nodes .lgn-person')", 5)
        time.sleep(0.8)
        kids = {i: v for i, v in (self.j(ATTRS) or {}).items() if i.startswith("user:")}
        self.check("Y3 inheritance: the boxes opened from a private box carry a copy of its valve (private, same owner)",
                   len(kids) == 3 and all(v == ["private", me] for v in kids.values()), kids)

        # Free walking (2026-11-15): a conduit box is walked across. You can stand over it, but you are never IN it.
        nodes["mine:friends"]["valve"] = {"type": "conduit"}
        self.load()
        through = self.hop("mine:friends")     # walks to its middle and stops getting closer there
        over = self.j(self.OVER)
        self.set_valve("mine:friends", "open")
        time.sleep(0.4)
        stops = self.state()
        self.check("Y4 Conduit: you walk across the conduit box but are never in it (over it, in no box, mainstream);"
                   " as an Open box, standing in the same place, you are in it",
                   over is True and through.get("at") == "" and through.get("instance") == "mainstream"
                   and stops.get("at") == "mine:friends" and stops.get("instance") == "mine:friends",
                   {"over the conduit box": over, "conduit -> at": through.get("at"), "conduit -> instance": through.get("instance"),
                    "open -> at": stops.get("at")})
        for n in nodes.values():
            n.pop("valve", None)


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
        proc, r.ws = cdp.launch_edge("lg-walker-valve-types-")
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
        print("VERDICT walker-valve-types: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
