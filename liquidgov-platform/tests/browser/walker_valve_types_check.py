"""Spatial step 7 (SPATIAL_EVOLUTION.md): the valve types on the map walker's entry path. Three focused cases
(Open, Private, Conduit) and the inheritance rule. New file: no committed check is edited.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph,
and walker_instance_check.py's touchpad walk. The valves come in through the graph's data (the fake graph's
nodes are edited in this process before each page load), the way a real box's data would carry one.
The meanings (Brandon 2026-11-14): open: anyone; closed: no one; private: only the owner; invite: only people on the
list; request: turned away, recorded for the owner; conduit: passed through to the next box, never stood on. A new
box copies its parent's valve unless its own data names one.

  Y0  precondition: the map loaded; boxes with no valve in their data are Open
  Y1  Open: the walker enters an Open box
  Y2  Private: a box private to someone else is not entered; private to you, it is entered
  Y3  inheritance: the boxes opened from that private-to-you box carry a copy of its valve (private, same owner)
  Y4  Conduit: with a conduit box between you and the box beyond it (one box and connector this check adds to the page,
      since the stand-in map never shows two in a row), the walker is carried through it onto the box beyond and
      never stands on the conduit; positive control: as an Open box the same push stops on it

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

    ROW = ("(() => { const f = document.querySelector('#lgm-nodes .lgn[data-id=\"mine:friends\"]'),"
           " y = document.querySelector('#lgm-nodes .lgn[data-id=\"you\"]');"
           " const mid = e => { const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2]; };"
           " const c = f.cloneNode(true); c.setAttribute('data-id', 'beyond'); c.setAttribute('data-valve', 'open'); f.parentNode.appendChild(c);"
           " const base = f.getAttribute('transform'); c.setAttribute('transform', 'translate(100,0) ' + base);"
           " const m0 = mid(f), m1 = mid(c), k = (m1[0] - m0[0]) / 100, yy = mid(y);"
           " c.setAttribute('transform', 'translate(' + (m0[0] - yy[0]) / k + ',' + (m0[1] - yy[1]) / k + ') ' + base);"
           " const e0 = document.querySelector('#lgm-edges .lge'), e = e0.cloneNode(true);"
           " e.setAttribute('data-a', 'mine:friends'); e.setAttribute('data-b', 'beyond'); e0.parentNode.appendChild(e);"
           " return '1'; })()")

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
        self.hop("you")
        entered = self.hop("mine:friends").get("at")
        self.check("Y2 Private: a box private to someone else is not entered; private to you, it is entered",
                   refused == "you" and entered == "mine:friends", {"private to someone else -> at": refused, "private to you -> at": entered})
        self.key("enter")
        self.wait("!!document.querySelector('#lgm-nodes .lgn-person')", 5)
        time.sleep(0.8)
        kids = {i: v for i, v in (self.j(ATTRS) or {}).items() if i.startswith("user:")}
        self.check("Y3 inheritance: the boxes opened from a private box carry a copy of its valve (private, same owner)",
                   len(kids) == 3 and all(v == ["private", me] for v in kids.values()), kids)

        # The stand-in map shows one box and its neighbors, so no two boxes sit in a row. This check adds one box
        # ("beyond") and its connector to the page by hand, in line past the friends box: you -> friends -> beyond.
        nodes["mine:friends"]["valve"] = {"type": "conduit"}
        self.load()
        self.j(self.ROW)
        through = self.hop("mine:friends")
        self.set_valve("mine:friends", "open")
        self.hop("you")
        stops = self.hop("mine:friends")
        self.check("Y4 Conduit: the walker is carried through the conduit box onto the box beyond it, never standing on it;"
                   " as an Open box it stops there",
                   through.get("at") == "beyond" and through.get("instance") == "beyond" and stops.get("at") == "mine:friends",
                   {"conduit -> at": through.get("at"), "open -> at": stops.get("at")})
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
