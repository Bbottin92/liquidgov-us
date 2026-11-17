"""The kind of pool a channel leads to is drawn at its mouth (Brandon 2026-11-16: "that feels right"; follows
walker_signs_check.py and walker_signs_spread_check.py, which stay as committed).

Real headless Chromium at phone size, NO C++ server: walker_controls_check.py's stdlib server and fake graph, the walker
on (pools). At each channel mouth (.lge-sign) a symbol (.lge-mark, no box) of the pool that channel leads to, in that
kind's colour: person gold, three heads violet, ticked box coral, speech bubble teal, map pin green, star pink, house
white. Your own pool ('you') names every mouth as well; other pools show the symbol only, except a person's mouth, which
keeps its name. The pool you stand in is bigger on screen than before (it filled 96% of the width, now about 111%).

  K0  precondition: the map loaded and the walker is walking
  K1  your own pool: Friends = person (gold), Groups = heads (violet), Places = pin (green), Politics = ticked box
      (coral), Feeds = speech bubble (teal), Interests = star (pink), every one with its name beside it
  K2  Groups pool (other pool): every mouth has a symbol; the way back to you is a house (white); no mouth shows a name
  K3  Friends pool: the mouths to people keep their names (symbol and name)
  K4  the pool you stand in is wider than the screen's view by at least 5% (before: 96% of it)

Usage: python3 walker_kind_marks_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-kind-marks: ALL-PASS" or the failures; exit 1 on a FAIL."""
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

COLOURS = {"person": "#f2c94c", "heads": "#b392f0", "ticked": "#ff7f6e", "bubble": "#3fd0c4", "pin": "#5fd068",
           "star": "#ff7eb6", "house": "#ffffff"}
YOU_WANT = {"mine:friends": "person", "mine:groups": "heads", "mine:places": "pin", "mine:politics": "ticked",
            "mine:feeds": "bubble", "mine:interests": "star"}

# every mouth of pool `%s`: where it leads (from the key), its symbol and the symbol's colour, its name and whether shown
PROBE = ("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]'); if (!g) return null;"
         " return JSON.stringify([...g.querySelectorAll('.lge-sign')].map(t => { const f = t.getAttribute('data-for'),"
         " k = g.querySelector('.lge-mark[data-for=\"' + f + '\"]'), b = k ? k.querySelector('.lge-mark-body') : null;"
         " return {to: f.split('|')[0] === '%s' ? f.split('|')[1].split('>')[0] : f.split('|')[0], sym: k ? k.getAttribute('data-sym') : '',"
         " fill: b ? b.getAttribute('fill') : '', name: t.textContent, nameShown: t.style.display !== 'none' && t.textContent !== '',"
         " markShown: !!k && k.style.display !== 'none'}; })); })()")
# the pool you are in, against the screen's view: its width on the screen over the width of what you see
WIDTH = ("(() => { const s = document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"] .lgs'); if (!s) return null;"
         " const r = s.getBoundingClientRect(), f = LG_MAP.frame(); return JSON.stringify({pool: r.width, view: Math.min(f.right, innerWidth) - Math.max(f.left, 0)}); })()")


class Run(wi.Run):
    def mouths(self, pool):
        return self.j(PROBE % (pool, pool)) or []

    def cases(self):
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        self.check("K0 precondition: the map loaded and the walker is walking", ok, {"loaded": ok})
        if not ok:
            return
        self.j("LG_MAP.goTo('you')")
        time.sleep(2.0)
        you = {m["to"]: m for m in self.mouths("you")}
        self.shot("kind-marks-you")
        bad = {k: (you.get(k) or {}) for k, v in YOU_WANT.items()
               if not you.get(k) or you[k]["sym"] != v or you[k]["fill"] != COLOURS[v] or not you[k]["nameShown"]}
        self.check("K1 your own pool: each kind has its symbol in its colour, with its name beside it",
                   not bad, {"wrong or missing": bad, "mouths": sorted(you)})
        self.j("LG_MAP.goTo('mine:groups')")
        time.sleep(2.5)
        grp = self.mouths("mine:groups")
        self.shot("kind-marks-groups")
        back = [m for m in grp if m["to"] == "you"]
        noname = [m["to"] for m in grp if m["nameShown"]]
        nosym = [m["to"] for m in grp if m["to"] != "mine:events" and not m["sym"]]
        self.check("K2 Groups pool: every mouth has a symbol, the way back to you is a house, and no mouth shows a name",
                   bool(grp) and not noname and not nosym and bool(back) and back[0]["sym"] == "house",
                   {"named": noname, "no symbol": nosym, "back": back, "mouths": len(grp)})
        self.j("LG_MAP.goTo('mine:friends')")
        time.sleep(2.5)
        fr = self.mouths("mine:friends")
        self.shot("kind-marks-friends")
        people = [m for m in fr if m["to"].startswith(("person:", "user:"))]
        self.check("K3 Friends pool: the mouths to people keep their names, with the person symbol",
                   len(people) >= 2 and all(m["nameShown"] and m["sym"] == "person" for m in people),
                   {"people": people})
        w = self.j(WIDTH % "mine:friends") or {}
        ratio = (w.get("pool") or 0) / (w.get("view") or 1)
        self.check("K4 the pool you stand in is at least 5% wider than the view (before: 96% of it)", ratio >= 1.05, {"ratio": round(ratio, 3), **w})


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
        proc, r.ws = cdp.launch_edge("lg-walker-kindmarks-")
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
        print("VERDICT walker-kind-marks: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
