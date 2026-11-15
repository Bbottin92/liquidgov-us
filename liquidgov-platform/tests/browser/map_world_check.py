"""The map is one world (Brandon 2026-11-15: pools and gates, "lets just get it all built now"; "rings outward from
wherever you are, each thing exists once": "okay"; the side panel abandoned for the portal). Replaces the checks of the
old model, where opening a box cleared the map and laid out a fresh ring that fit the frame, with a side panel for
pages: map_live_check, map_live_check2, map_big_level_check, map_you_ring_all_kinds_check and map_view_only_check
(retired 2026-11-15, their cases tested exactly what changed). What they checked that still holds is kept here.

Real Chromium, the real lgmap.js and lgmap.css and the map page's markup, map_live_check2's fake graph (the real
server's ids and links) with map_big_level_check's 41-member group; no C++ server, no walker (the map as it is with the
droplet hidden in Settings). A box is opened the way a keyboard does it (Space: its connections grow, no page opens).

Cases, at 1600x1000 and at a narrow 755 px window (the droplet hidden: boxes), and at 1600x1000 as pools (the
droplet on: big oval pools, a pool grows within a limit and never into another, "+N more" for what does not fit):
  O1  the first view: you and your 7 kinds, each joined to you; every name drawn at >= 11 px
  O2  opening Groups: the trail is You > Groups, its groups are added, and every box already on the map stays exactly
      where it was (one world: nothing is laid out again)
  O3  each thing once: opening The Gov, then samyraine (who links back to The Gov and to Bbottin, both on the map
      already): no id is drawn twice, and her link to The Gov is a connector between the two boxes
  O4  no two boxes overlap (their regions, in map units)
  O5  a big level (41 links): a "+N more" box, and what is shown + N = 41; opening "+N more" shows the next ones
  O6  Back (the phone's back gesture) steps the center one back (the trail one shorter); nothing is removed
  O7  a click on a box with a page goes to that page (no side panel: the page itself)

Usage: LG_BROWSER=$(command -v google-chrome) python3 map_world_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT map-world: ALL-PASS" or the failed cases. Exit 1 on a FAIL."""
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
import map_big_level_check as mb  # noqa: E402  (the big graph; through it map_live_check2, which sets LG_BROWSER)
import map_live_check2 as m2  # noqa: E402
import cdp  # noqa: E402

KINDS = ["mine:events", "mine:feeds", "mine:friends", "mine:groups", "mine:interests", "mine:places", "mine:politics"]
# every drawn box: id, its region (map units) and whether it is drawn twice
REGIONS = ("[...document.querySelectorAll('#lgm-nodes > g.lgn')].map(g => { const id = g.getAttribute('data-id'),"
           " r = LG_MAP.instance(id) && LG_MAP.instance(id).region; return [id, r ? [r.cx, r.cy, r.hw, r.hh, !!r.pool] : null]; })")
EDGES = "[...document.querySelectorAll('#lgm-edges .lge')].map(e => [e.getAttribute('data-a'), e.getAttribute('data-b')])"


class Run(mb.BigRun):
    @property
    def n(self):
        return len(self.rep["checks"])

    def regions(self):
        return self.j(REGIONS) or []

    def where(self):
        return {i: r for i, r in self.regions() if r}

    def edges(self):
        return self.j(EDGES) or []

    def trail(self):
        return self.j("__m.trail()") or []

    def case_set(self, tag, w, h, base):
        self.base = base
        self.size(w, h)
        self.load()
        ids = [i for i, _ in self.regions()]
        es = {tuple(sorted(e)) for e in self.edges()}
        small, names = self.small_names()
        self.check("O1 %s: you and your 7 kinds, each joined to you; names >= 11 px" % tag,
                   sorted(ids) == sorted(["you"] + KINDS) and all(tuple(sorted(("you", k))) in es for k in KINDS)
                   and names and not small, {"ids": ids, "small": small[:5]})

        before = self.where()
        self.click_box("mine:groups", ["You", "Groups"])
        after = self.where()
        moved = [i for i in before if after.get(i, [None])[:2] != before[i][:2]]
        new = sorted(set(after) - set(before))
        groups = sorted(t for t in mb.LINKS["mine:groups"] if t != "you")
        self.check("O2 %s: opening Groups adds its groups, and nothing already on the map moves" % tag,
                   self.trail() == ["You", "Groups"] and new == groups and not moved, {"new": new, "moved": moved})

        self.click_box("group:gov", ["You", "Groups", "The Gov"])
        self.click_box("user:samyraine", ["You", "Groups", "The Gov", "samyraine"])
        ids = [i for i, _ in self.regions()]
        twice = sorted({i for i in ids if ids.count(i) > 1})
        es = {tuple(sorted(e)) for e in self.edges()}
        self.check("O3 %s: each thing once (no id drawn twice); samyraine's link back to The Gov is a connector" % tag,
                   not twice and ("group:gov", "user:samyraine") in es and ids.count("user:Bbottin") == 1,
                   {"twice": twice, "gov-samyraine": ("group:gov", "user:samyraine") in es})

        self.click_box("mine:groups", ["You", "Groups"])   # back to Groups' box (already open: only selected), then the big one
        before = set(self.where())
        self.click_box("group:big", ["You", "Groups", "Roseville Neighbors"])
        after = self.where()
        new = sorted(set(after) - before)
        more = [i for i in new if i.startswith("more:")]
        n_more = 0
        for b in self.more_box():
            if b["id"] in more:
                try:
                    n_more = int(b["label"].strip("+").split()[0])
                except (ValueError, IndexError):
                    pass
        already = len(set(mb.LINKS["group:big"]) & before)
        self.check("O5 %s: the big level shows a \"+N more\" box, and what it added + already there + N = 41" % tag,
                   len(more) == 1 and len(new) - 1 + already + n_more == len(mb.LINKS["group:big"]),
                   {"added": len(new) - 1, "already": already, "more": more, "N": n_more})
        if more:
            before2 = set(self.where())
            label = next((b["label"] for b in self.more_box() if b["id"] == more[0]), "")
            self.click_box(more[0], ["You", "Groups", "Roseville Neighbors", label])
            self.check("O5 %s: opening \"+N more\" shows the next ones" % tag, len(set(self.where()) - before2) >= 1,
                       len(set(self.where()) - before2))

        regs = self.regions()
        hits = []
        for i in range(len(regs)):
            for k in range(i + 1, len(regs)):
                a, b = regs[i][1], regs[k][1]
                if not a or not b:
                    continue
                if a[4] and b[4]:   # pools are ovals: compared as circles round them (a little bigger than the ovals)
                    if math.hypot(a[0] - b[0], a[1] - b[1]) < max(a[2], a[3]) + max(b[2], b[3]) - 1:
                        hits.append((regs[i][0], regs[k][0]))
                elif abs(a[0] - b[0]) < a[2] + b[2] - 1 and abs(a[1] - b[1]) < a[3] + b[3] - 1:
                    hits.append((regs[i][0], regs[k][0]))
        self.check("O4 %s: no two boxes overlap" % tag, not hits, hits[:6])

        t0, n0 = self.trail(), len(self.regions())
        cdp.js(self.ws, "history.back(), 1")
        time.sleep(1.2)
        self.ready("after Back", None)
        t1 = self.trail()
        self.check("O6 %s: Back steps the center one back, and nothing is removed" % tag,
                   len(t1) == len(t0) - 1 and t1 == t0[:len(t1)] and len(self.regions()) == n0, {"before": t0, "after": t1})

    def cases(self):
        self.case_set("desktop", 1600, 1000, self.box_base)
        self.case_set("narrow", 755, 900, self.box_base)
        # the pools' layout (with the droplet on, lgmap.js lays the map out as pools; the walker is not needed for that)
        self.case_set("pools desktop", 1600, 1000, self.pool_base)
        # O7: a click on a box with a page opens that page itself
        self.base = self.box_base
        self.size(1600, 1000)
        self.load()
        b = self.j("__m.boxes().find(b => b.id === 'mine:friends') || null")
        if isinstance(b, dict):
            self.click_xy((b["l"] + b["r"]) / 2, (b["t"] + b["b"]) / 2)
        end, path = time.time() + 6, ""
        while time.time() < end:
            path = cdp.js(self.ws, "location.pathname") or ""
            if path == "/friends":
                break
            time.sleep(0.2)
        self.check("O7 a click on a box with a page goes to that page (/friends)", path == "/friends", path)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), m2.make_handler(platform, mb.NODES, mb.LINKS))
    psrv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), m2.make_handler(platform, mb.NODES, mb.LINKS, pools=True))
    for x in (srv, psrv):
        threading.Thread(target=x.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    r.box_base, r.pool_base = r.base, "http://127.0.0.1:%d" % psrv.server_address[1]
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-map-world-")
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
        psrv.shutdown()
        print("VERDICT map-world: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
