"""In a stream you stay in that stream, and it is drawn on top (Brandon 2026-11-16: "you shouldnt be able to jump from one
stream into a different crossing stream. and whatever stream youre on should be on the topmost layer").

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, the walker
on (pools), the pools of the fake graph opened so that their channels exist. A crossing is two channels (pool-to-pool
segments) that cut across each other away from any pool. The check finds one on the map (the fake graph has them: persons
and groups are linked from several sides), walks the drop along one channel with key holds up to the crossing, then pushes
along the OTHER channel for a while.

  X0  precondition: a crossing of two showing channels is found, the crossing point is in open water (in no pool), and the
      drop walked along the first channel to within 30 map units of it
  X1  pushing along the crossing channel does not take the drop onto it: it stays in the first channel (within its 42-unit
      half width), and is not in either end pool of the crossing channel
  X2  paint order: every one of the first channel's three drawings (shore, water, shimmer) has a copy painted after
      every drawing of every other channel, and pools (#lgm-nodes) are painted after all of them
  X3  leaving the channel (back in a pool) puts the order back: nothing of a channel is raised any more

Usage: python3 walker_stream_crossing_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-stream-crossing: ALL-PASS" or the failures; exit 1 on a FAIL."""
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
import walker_page_symbol_check as wps  # noqa: E402

CHANNEL = 42.0   # a channel's half width in map units (lgmap_walk.js CHANNEL, pools)

# every showing channel with its two pool middles and sizes (map units), and the walker's place in map units
EDGES = ("(() => { const out = []; document.querySelectorAll('#lgm-edges .lge').forEach(e => { if (e.style.display === 'none' || e.style.visibility === 'hidden') return;"
         " const a = e.getAttribute('data-a'), b = e.getAttribute('data-b'), ra = LG_MAP.instance(a), rb = LG_MAP.instance(b);"
         " if (!ra || !rb || !ra.region || !rb.region) return; out.push({a: a, b: b, ra: ra.region, rb: rb.region}); });"
         " const w = document.getElementById('lgm-walker');"
         " return JSON.stringify({edges: out, me: [+w.getAttribute('data-x'), +w.getAttribute('data-y')], at: w.getAttribute('data-at')}); })()")
# the paint order of everything that draws a channel: [kind, a|b, document index]; and where the pools' layer is
ORDER = ("(() => { const all = [...document.querySelectorAll('#lgmap *')], idx = n => all.indexOf(n), out = [];"
         " document.querySelectorAll('#lgmap .lge, #lgmap .lge-fill, #lgmap .lge-flow, #lgmap .lge-topshore').forEach(n => {"
         " const c = n.getAttribute('class') || '', key = n.getAttribute('data-for') || (n.getAttribute('data-a') + '|' + n.getAttribute('data-b'));"
         " const kind = /lge-fill/.test(c) ? 'fill' : /lge-flow/.test(c) ? 'flow' : 'shore'; out.push([kind, key, idx(n)]); });"
         " const nodes = document.getElementById('lgm-nodes'), first = [...nodes.querySelectorAll('.lgn')][0];"
         " return JSON.stringify({order: out, pools: first ? idx(first) : -1}); })()")


def inside(p, r, k=1.0):
    return ((p[0] - r["cx"]) / (r["hw"] * k)) ** 2 + ((p[1] - r["cy"]) / (r["hh"] * k)) ** 2 <= 1.0


def cross(e, f):
    """Where channels e and f (middle to middle) cut across each other, or None (sharing a pool, parallel or apart)."""
    if {e["a"], e["b"]} & {f["a"], f["b"]}:
        return None
    p, r = (e["ra"]["cx"], e["ra"]["cy"]), (e["rb"]["cx"] - e["ra"]["cx"], e["rb"]["cy"] - e["ra"]["cy"])
    q, s = (f["ra"]["cx"], f["ra"]["cy"]), (f["rb"]["cx"] - f["ra"]["cx"], f["rb"]["cy"] - f["ra"]["cy"])
    den = r[0] * s[1] - r[1] * s[0]
    if abs(den) < 1e-6:
        return None
    t = ((q[0] - p[0]) * s[1] - (q[1] - p[1]) * s[0]) / den
    u = ((q[0] - p[0]) * r[1] - (q[1] - p[1]) * r[0]) / den
    if not (0.05 < t < 0.95 and 0.05 < u < 0.95):
        return None
    return (p[0] + t * r[0], p[1] + t * r[1])


def seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    L = dx * dx + dy * dy
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / L)) if L else 0.0
    return math.hypot(a[0] + t * dx - p[0], a[1] + t * dy - p[1])


class Run(wps.Run):
    def edges(self):
        return self.j(EDGES) or {}

    def find_crossing(self, d):
        """(first channel, crossing channel, point) the farthest from both pools' ovals, or None."""
        best = None
        es = d.get("edges") or []
        pools = {}
        for e in es:
            pools[e["a"]], pools[e["b"]] = e["ra"], e["rb"]
        for i, e in enumerate(es):
            for f in es[i + 1:]:
                x = cross(e, f)
                if not x or any(inside(x, r, 1.15) for r in pools.values()):
                    continue
                # far from the first channel's own pools, so there is straight channel to walk before it
                lead = min(math.hypot(x[0] - e["ra"]["cx"], x[1] - e["ra"]["cy"]), math.hypot(x[0] - e["rb"]["cx"], x[1] - e["rb"]["cy"]))
                if best is None or lead > best[3]:
                    best = (e, f, x, lead)
        return best

    def add_crossing(self, d):
        """The fake graph is a star (every channel runs out of your pool), so no two channels cross. Add one channel to the
        page for this check only: a chord between two pools that are not joined, cutting across a channel in open water
        (the farthest such crossing). It is a real .lge element, so the walker treats it as any other channel. Returns
        the number of candidates tried, or 0 when no chord crosses in open water."""
        es = d.get("edges") or []
        pools, joined = {}, set()
        for e in es:
            pools[e["a"]], pools[e["b"]] = e["ra"], e["rb"]
            joined.add(frozenset((e["a"], e["b"])))
        best, tried = None, 0
        ids = sorted(pools)
        for i, a in enumerate(ids):
            for b in ids[i + 1:]:
                if frozenset((a, b)) in joined:
                    continue
                f = {"a": a, "b": b, "ra": pools[a], "rb": pools[b]}
                for e in es:
                    x = cross(e, f)
                    tried += 1
                    if not x or any(inside(x, r, 1.15) for r in pools.values()):
                        continue
                    lead = min(math.hypot(x[0] - e["ra"]["cx"], x[1] - e["ra"]["cy"]), math.hypot(x[0] - e["rb"]["cx"], x[1] - e["rb"]["cy"]),
                               math.hypot(x[0] - pools[a]["cx"], x[1] - pools[a]["cy"]), math.hypot(x[0] - pools[b]["cx"], x[1] - pools[b]["cy"]))
                    if best is None or lead > best[0]:
                        best = (lead, f)
        if not best:
            return 0
        f = best[1]
        self.j("(() => { const l = document.getElementById('lgm-edges'), p = document.createElementNS('http://www.w3.org/2000/svg', 'path');"
               " p.setAttribute('class', 'lge lge-x'); p.setAttribute('data-a', '%s'); p.setAttribute('data-b', '%s');"
               " p.setAttribute('d', 'M%f %f L%f %f'); l.appendChild(p); return JSON.stringify(1); })()"
               % (f["a"], f["b"], f["ra"]["cx"], f["ra"]["cy"], f["rb"]["cx"], f["rb"]["cy"]))
        return tried

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        for pool in ("you", "mine:groups", "mine:friends", "mine:feeds", "mine:places", "mine:interests", "mine:politics"):
            self.j("LG_MAP.goTo('%s')" % pool)
            time.sleep(1.8)
        self.map_go("you")
        time.sleep(1.0)
        for _ in range(8):   # zoomed out, so every channel shows
            self.j("(() => { const s = document.querySelector('#lgmap svg'), f = LG_MAP.frame(); s.dispatchEvent(new WheelEvent('wheel', {deltaY: 240,"
                   " ctrlKey: true, clientX: f.left + f.width / 2, clientY: f.top + f.height / 2, bubbles: true, cancelable: true})); return 1; })()")
            time.sleep(0.15)
        time.sleep(1.0)
        d = self.edges()
        found = self.find_crossing(d)
        added = 0
        if not found:
            added = self.add_crossing(d)
            time.sleep(1.2)
            d = self.edges()
            found = self.find_crossing(d)
        self.shot("stream-crossing-map")
        if not found:
            self.check("X0 precondition: a crossing of two showing channels in open water is found", False,
                       {"channels": len(d.get("edges") or []), "chord added": bool(added)})
            return
        e1, e2, x, lead = found
        k1, a1, b1 = e1["a"] + "|" + e1["b"], (e1["ra"]["cx"], e1["ra"]["cy"]), (e1["rb"]["cx"], e1["rb"]["cy"])
        # to the pool at the near end of the first channel, then along it to the crossing
        near = e1["a"] if math.hypot(x[0] - a1[0], x[1] - a1[1]) <= math.hypot(x[0] - b1[0], x[1] - b1[1]) else e1["b"]
        far_pt = b1 if near == e1["a"] else a1
        self.map_go(near)
        time.sleep(1.0)

        def target():   # the crossing point on the screen now
            p = self.j("JSON.stringify(LG_MAP.toScreen(%f, %f))" % x) or {}
            return [p.get("x"), p.get("y")] if p else None
        self.walk_to(target, 8, limit_s=25)
        w = self.edges()
        me = w.get("me")
        dist = math.hypot(me[0] - x[0], me[1] - x[1]) if me else None
        on_first = me is not None and seg_dist(me, a1, b1) <= CHANNEL
        self.check("X0 precondition: a crossing in open water is found and the drop walked along the first channel up to it",
                   ok and dist is not None and dist <= 30 and on_first,
                   {"crossing": [round(x[0]), round(x[1])], "channel added by this check": bool(added), "first": k1, "second": e2["a"] + "|" + e2["b"], "drop": me, "distance": dist and round(dist, 1)})
        if dist is None or dist > 30 or not on_first:
            return
        self.shot("stream-crossing-at")
        # push along the other channel (toward its far end), a long while: the drop must not take that channel
        s2a, s2b = (e2["ra"]["cx"], e2["ra"]["cy"]), (e2["rb"]["cx"], e2["rb"]["cy"])
        dirs = [(s2b[0] - s2a[0], s2b[1] - s2a[1]), (s2a[0] - s2b[0], s2a[1] - s2b[1])]
        worst, hopped = 0.0, []
        for n, (dx, dy) in enumerate(dirs):
            if n:   # the second push starts from the crossing again (out by the pool's mouth, as a person does)
                self.map_go(near)
                time.sleep(1.0)
                self.walk_to(target, 8, limit_s=25)
            for _ in range(30):
                self.hold(dx, dy)
                w = self.edges()
                m = w.get("me") or me
                if w.get("at"):   # reached a pool: the channel's end, which is allowed (what the drop does inside a pool is not this check's)
                    if w.get("at") in ({e2["a"], e2["b"]} - {e1["a"], e1["b"]}):
                        hopped.append(w.get("at"))
                    break
                worst = max(worst, seg_dist(m, a1, b1))
        w = self.edges()
        m, at = w.get("me"), w.get("at")
        ends = {e2["a"], e2["b"]} - {e1["a"], e1["b"]}
        self.shot("stream-crossing-after")
        self.check("X1 pushing along the crossing channel does not take the drop onto it: it stays in the first channel",
                   worst <= CHANNEL + 1 and at not in ends and not hopped,
                   {"farthest from the first channel in open water": round(worst, 1), "half width": CHANNEL, "at": at,
                    "second channel's pools reached": hopped, "second channel's pools": sorted(ends)})
        # X2 / X3 from a walk back on the first channel (out of the pool the pushing ended in, by its mouth, as a person does)
        self.map_go(near)
        time.sleep(1.0)
        self.walk_to(target, 8, limit_s=25)
        time.sleep(0.6)
        o = self.j(ORDER) or {}
        order = o.get("order") or []
        others = [r[2] for r in order if r[1] != k1]
        last_other = max(others) if others else -1
        mine = {}
        for kind, key, i in order:
            if key == k1 and i > last_other:
                mine[kind] = i
        self.check("X2 paint order: the shore, water and shimmer of the channel you are in are painted after every other channel, pools after both",
                   len(others) >= 3 and set(mine) == {"shore", "fill", "flow"} and o.get("pools", -1) > max(mine.values()),
                   {"raised kinds": sorted(mine), "other drawings": len(others), "pools at": o.get("pools"), "raised at": mine,
                    "drop now": self.edges().get("me"), "in": self.edges().get("at"), "first channel": k1, "crossing": [round(x[0]), round(x[1])]})
        self.map_go(near)
        time.sleep(1.5)
        o = self.j(ORDER) or {}
        order = o.get("order") or []
        others = [r[2] for r in order if r[1] != k1]
        raised = [r for r in order if r[1] == k1 and others and r[2] > max(others)]
        self.check("X3 back in a pool the order is back as it was: no channel is raised", bool(order) and not raised and self.edges().get("at") == near,
                   {"raised": raised, "at": self.edges().get("at")})


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
        proc, r.ws = cdp.launch_edge("lg-walker-crossing-")
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
        print("VERDICT walker-stream-crossing: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
