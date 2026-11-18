"""Map clarity (Brandon 2026-11-17, from the two AI map reviews, "take the best parts"): four small changes so a first-time
person can read the map. New file: no committed check is edited.

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, and
walker_instance_check.py's touchpad walk. A valve is put on a box by editing the fake graph in this process before the page
loads, the way walker_valve_types_check.py does.

  C0  precondition: the map loaded and the walker is walking
  C1  about 10 distinct symbols: lgmap_walk.js's KINDS block (read from the file, no copy here) gives proposals, site-wide
      votes, problems, messages and offices a symbol each of their own (before: proposal = site-wide = problem and
      message = feed, office = interest), every symbol its own colour, and feeds, posts and comments still share the bubble
  C2  drawn: the mouth to the site-wide pool shows the site-wide symbol, not the proposal's
  C3  refused entry says so: a closed, a private (someone else's), an invite and a request box each put a sentence naming the
      box and the reason in the map's status line when the walker is turned away at its edge (before: nothing at all)
  C4  a refused box wears a lock mark; an open one, and a private one that is yours, do not; it follows the valve (0.4 s tick)
  C5  a "?" key button is reachable in the full-screen map (the legend under the map is hidden there); it opens a key with one
      row per symbol in the table, drawn from the same drawings, plus the lock; it closes again
  C6  the MAINstream pool carries its subtitle "everything site-wide"

Usage: python3 walker_map_clarity_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-map-clarity: ALL-PASS" or the failures; exit 1 on a FAIL."""
import http.server
import json
import math
import os
import re
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import walker_controls_check as wc  # noqa: E402
import walker_instance_check as wi  # noqa: E402

STATUS = "(() => { const s = document.getElementById('lgm-status'); return JSON.stringify({t: s ? s.textContent : null}); })()"
LOCK = ("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]'); const l = g ? g.querySelector('.lgn-lock') : null;"
        " return JSON.stringify({lock: !!l, shown: !!l && l.style.display !== 'none'}); })()")
MOUTHS = ("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]'); if (!g) return null;"
          " return JSON.stringify([...g.querySelectorAll('.lge-mark')].map(k => { const f = k.getAttribute('data-for'), p = f.split('>')[0].split('|'),"
          " to = p[0] === '%s' ? p[1] : p[0]; return {to: to, sym: k.getAttribute('data-sym')}; })); })()")
KEY = ("(() => { const b = document.getElementById('lgm-keybtn'), p = document.getElementById('lgm-key');"
       " const vis = e => { if (!e) return false; const r = e.getBoundingClientRect(), c = getComputedStyle(e);"
       " return r.width > 0 && r.height > 0 && c.display !== 'none' && c.visibility !== 'hidden'; };"
       " return JSON.stringify({btn: vis(b), panel: vis(p), expanded: b ? b.getAttribute('aria-expanded') : null,"
       " rows: p ? p.querySelectorAll('.lgm-key-row').length : 0, syms: p ? [...p.querySelectorAll('.lgm-key-row svg')].length : 0,"
       " lock: p ? !!p.querySelector('.lgm-key-lock') : false, text: p ? p.textContent : ''}); })()")
SUB = ("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=\"root\"]'); const t = g ? g.querySelector('.lgn-pool-sub') : null;"
       " return JSON.stringify({node: !!g, sub: t ? t.textContent : null}); })()")


def kinds_table(platform):
    """the KINDS block of lgmap_walk.js, parsed (the one place the table lives)"""
    src = open(os.path.join(platform, "assets", "lgmap_walk.js"), encoding="utf-8").read()
    m = re.search(r"/\*KINDS-BEGIN\*/(.*?)/\*KINDS-END\*/", src, re.S)
    return json.loads(m.group(1)) if m else None


class Run(wi.Run):
    def jj(self, expr, default):
        """self.j already parses a JSON string; an empty or failed read gives the default"""
        v = self.j(expr)
        if isinstance(v, str):
            try:
                v = json.loads(v)
            except ValueError:
                v = None
        return default if v is None else v

    def hold(self, dx, dy):
        """One short key hold toward (dx, dy) on the screen: one key, or two for a slant (as walker_page_symbol_check does)."""
        keys = {"up": "w", "down": "s", "left": "a", "right": "d"}
        n = max(abs(dx), abs(dy)) or 1
        ks = []
        if abs(dx) > 0.4 * n:
            ks.append(keys["right" if dx > 0 else "left"])
        if abs(dy) > 0.4 * n:
            ks.append(keys["down" if dy > 0 else "up"])
        for k in ks:
            self.ws.call("Input.dispatchKeyEvent", type="keyDown", key=k, windowsVirtualKeyCode=wc.VK.get(k, ord(k.upper())))
        time.sleep(max(0.04, min(0.2, n / 300.0)))
        for k in ks:
            self.ws.call("Input.dispatchKeyEvent", type="keyUp", key=k, windowsVirtualKeyCode=wc.VK.get(k, ord(k.upper())))
        time.sleep(0.08)

    def push_at(self, target, limit_s=25):
        """Key holds toward box `target` until the walker is in it, or has stopped getting closer (turned away at its edge).
        (The touchpad walk of walker_instance_check.hop does not work on this PC, committed Y1-Y4 fail the same on master.)"""
        end, best, stuck, st = time.time() + limit_s, None, 0, {}
        while time.time() < end:
            st = self.j(self.WALK) or {}
            to, spot = (st.get("mids") or {}).get(target), st.get("spot")
            if st.get("at") == target or not to or not spot:
                break
            dx, dy = to[0] - spot[0], to[1] - spot[1]
            n = math.hypot(dx, dy) or 1
            stuck = stuck + 1 if best is not None and n > best - 2 else 0
            if stuck >= 10:
                break
            best = n if best is None else min(best, n)
            self.hold(dx, dy)
        time.sleep(0.3)
        return self.state() or st

    def load(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        time.sleep(0.6)
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        return ok

    def cases(self, table):
        nodes = wc.m2.NODES
        me = wc.m2.ME
        for n in nodes.values():
            n.pop("valve", None)
        ok = self.load()
        self.check("C0 precondition: the map loaded and the walker is walking; the KINDS table was read from lgmap_walk.js",
                   bool(ok) and bool(table), {"loaded": ok, "table": bool(table)})
        if not (ok and table):
            return
        k, mine, col = table["kinds"], table["mine"], table["colours"]
        syms = set(k.values()) | set(mine.values())
        self.check("C1 about 10 distinct symbols: proposal, site-wide, problem, message and office each have their own; every symbol its own colour;"
                   " feed, post and comment still share the bubble",
                   len({k["prop"], k["sitewide"], k["pain"]}) == 3 and k["message"] != k["feed"] and k["dept"] != k["interest"]
                   and mine["problems"] != mine["politics"] and mine["problems"] == k["pain"] and len(syms) >= 10
                   and len(set(col.values())) == len(col) and set(col) == syms and k["feed"] == k["post"] == k["comment"],
                   {"symbols": sorted(syms), "kinds": k, "mine": mine})

        # C2: the mouth to the site-wide pool, and to a proposal, seen from whichever pool leads to them
        seen = {}
        for pool in ("you", "mine:politics", "mine:groups", "mine:friends", "mine:feeds", "mine:places", "group:gov", "sitewide"):
            self.j("LG_MAP.goTo('%s')" % pool)
            time.sleep(1.6)
            for m in self.jj(MOUTHS % (pool, pool), []):
                seen.setdefault(m["to"], m["sym"])
        self.shot("map-clarity-mouths")
        self.check("C2 drawn: the mouth to the site-wide pool shows the site-wide symbol and the proposal's mouth the proposal symbol (not the same)",
                   seen.get("sitewide") == k["sitewide"] and seen.get("prop:7") == k["prop"] and k["sitewide"] != k["prop"],
                   {"sitewide": seen.get("sitewide"), "prop:7": seen.get("prop:7"), "all seen": seen})

        # C3 + C4: a box that turns you away says why, and wears a lock
        cases = [("closed", {"type": "closed"}, "closed"), ("private", {"type": "private", "owner": "someone-else"}, "private"),
                 ("invite", {"type": "invite", "invite": ["someone-else"]}, "invite"),
                 ("request", {"type": "request"}, "ask")]
        for n in nodes.values():
            n.pop("valve", None)
        self.load()
        free = self.push_at("mine:politics")
        self.check("C2b precondition: with no valve the same key walk enters the Politics pool (so a refusal below is the valve, not the walk)",
                   free.get("at") == "mine:politics", {"at": free.get("at")})
        said, locks = {}, {}
        for name, valve, word in cases:
            for n in nodes.values():
                n.pop("valve", None)
            nodes["mine:politics"]["valve"] = valve
            self.load()
            turned = self.push_at("mine:politics")
            time.sleep(0.5)
            if turned.get("at") == "mine:politics":   # the valve did not stop the walk: say so, never pass quietly
                self.check("C3-%s precondition: the walker was turned away" % name, False, {"at": turned.get("at")})
            said[name] = (self.jj(STATUS, {}).get("t") or "", word)
            locks[name] = self.jj(LOCK % "mine:politics", {})
            if name == "closed":
                self.shot("map-clarity-refused")
        bad = {n: v[0] for n, v in said.items() if not (v[0] and "Politics" in v[0] and v[1] in v[0].lower())}
        self.check("C3 turned away at a closed, private, invite or request box: the status line names the box and says why", not bad,
                   {"wrong or empty": bad, "said": {n: v[0] for n, v in said.items()}})
        for n in nodes.values():
            n.pop("valve", None)
        nodes["mine:friends"]["valve"] = {"type": "private", "owner": me}
        self.load()
        time.sleep(0.9)
        mine_lock = self.jj(LOCK % "mine:friends", {})
        open_lock = self.jj(LOCK % "mine:feeds", {})
        self.check("C4 a refused box wears a lock mark; an open box and a private box that is yours do not",
                   all(v.get("lock") and v.get("shown") for v in locks.values()) and not mine_lock.get("lock") and not open_lock.get("lock"),
                   {"refused": locks, "private to me": mine_lock, "open": open_lock})
        for n in nodes.values():
            n.pop("valve", None)

        # C5: the "?" key in the full-screen map
        self.load()
        before = self.jj(KEY, {})
        self.j("(() => { const b = document.getElementById('lgm-keybtn'); if (b) b.click(); return '1'; })()")
        time.sleep(0.4)
        opened = self.jj(KEY, {})
        self.shot("map-clarity-key")
        self.j("(() => { const b = document.getElementById('lgm-keybtn'); if (b) b.click(); return '1'; })()")
        time.sleep(0.3)
        closed = self.jj(KEY, {})
        want = len(set(col))
        self.check("C5 a \"?\" key button shows in the full-screen map, opens a key (a row and a drawing per symbol, and the lock), and closes again",
                   before.get("btn") and not before.get("panel") and opened.get("panel") and opened.get("expanded") == "true"
                   and opened.get("rows", 0) >= want and opened.get("syms", 0) >= want and opened.get("lock")
                   and not closed.get("panel") and closed.get("expanded") == "false",
                   {"before": {x: before.get(x) for x in ("btn", "panel")}, "opened": {x: opened.get(x) for x in ("panel", "expanded", "rows", "syms", "lock")},
                    "closed": {x: closed.get(x) for x in ("panel", "expanded")}, "symbols in table": want})

        # C6: the MAINstream subtitle
        root = self.jj(SUB, {})
        if not root.get("node"):
            self.j("LG_MAP.goTo('root')")
            time.sleep(2.0)
            root = self.jj(SUB, {})
        self.shot("map-clarity-mainstream")
        self.check("C6 the MAINstream pool carries its subtitle \"everything site-wide\"", root.get("sub") == "everything site-wide", root)


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
        proc, r.ws = cdp.launch_edge("lg-walker-clarity-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases(kinds_table(platform))
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT walker-map-clarity: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
