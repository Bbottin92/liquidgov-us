"""One table says what each kind of pool looks like (Brandon 2026-11-16, "ok for all of those"): its symbol, its colour and
whether its mouth shows a name. The mouth symbols, the shore colours and the name rule all read that one table
(lgmap_walk.js, the block between KINDS-BEGIN and KINDS-END, plain JSON); this check reads the SAME block, so it holds no
copy of its own. Follows walker_kind_marks_check.py and walker_shore_colour_check.py, which stay as committed (they still
hold the colours as copies; changing them needs Brandon's OK).

  T0  the table is there: the block parses as JSON and names colours, kinds, the lists under "mine" and the name rule
  T1  one place: each colour of the table is written exactly once in lgmap_walk.js (in the table) and not at all in lgmap.css
  T2  your own pool ('you', the home pool): every mouth carries the symbol and the colour the table gives its kind, with its name
  T3  the Groups pool (another pool): every mouth's symbol and colour is the table's; no name is shown on a mouth whose kind the
      table does not list as always named
  T4  the Friends pool: the mouths to people show their names (the table lists person as always named), with the table's colour
  T5  every pool's shore colour (--kind) is the table's colour for its kind; a kind the table has no symbol for has none

Usage: python3 walker_kind_table_check.py [<liquidgov-platform dir> [<out dir>]]
Prints PASS/FAIL lines and "VERDICT walker-kind-table: ALL-PASS" or the failures; exit 1 on a FAIL."""
import http.server
import json
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
import walker_kind_marks_check as wkm  # noqa: E402  (its mouth probe)

BLOCK = re.compile(r"/\*KINDS-BEGIN\*/(.*?)/\*KINDS-END\*/", re.S)
# every pool's shore colour variable and class, to find a pool's own mark
SHORES = ("(() => { const out = {}; document.querySelectorAll('#lgm-nodes .lgn[data-id]').forEach(g => { out[g.getAttribute('data-id')] ="
          " {kind: ((g.getAttribute('class') || '').match(/lgn-(you|root|mine|person|group|prop|sitewide|pain|feed|post|comment|message|city|interest|dept|more)\\b/) || [])[1] || '',"
          " colour: g.style.getPropertyValue('--kind') || ''}; }); return JSON.stringify(out); })()")


def load_table(platform):
    src = open(os.path.join(platform, "assets", "lgmap_walk.js"), encoding="utf-8").read()
    m = BLOCK.search(src)
    if not m:
        return None, src
    try:
        return json.loads(m.group(1)), src
    except ValueError:  # err-ok: reported by T0 with the position
        return None, src


def mark_of(table, pool_id, kind):
    if kind == "mine":
        return table["mine"].get(pool_id[5:], "")
    return table["kinds"].get(kind, "")


class Run(wi.Run):
    platform = ""

    def cases(self):
        table, src = load_table(self.platform)
        ok_table = bool(table) and all(k in table for k in ("colours", "kinds", "mine", "named"))
        self.check("T0 the kinds table is there: JSON between KINDS-BEGIN and KINDS-END with colours, kinds, mine and named",
                   ok_table, {"found": table is not None})
        if not ok_table:
            return
        css = open(os.path.join(self.platform, "assets", "lgmap.css"), encoding="utf-8").read()
        bad = {c: (src.lower().count(c.lower()), css.lower().count(c.lower())) for c in table["colours"].values()
               if src.lower().count(c.lower()) != 1 or css.lower().count(c.lower()) != 0}
        self.check("T1 each colour of the table is written once in lgmap_walk.js and nowhere in lgmap.css", not bad,
                   {"colour: (in js, in css)": bad})
        self.size(390, 844, True)
        ok = self.map_load()
        self.map_walking()
        always = set(table["named"]["always"])
        shown = {}
        for pool in ("you", "mine:groups", "mine:friends"):
            self.j("LG_MAP.goTo('%s')" % pool)
            time.sleep(2.3)
            shown[pool] = {m["to"]: m for m in self.j(wkm.PROBE % (pool, pool)) or []}
        kinds = self.j(SHORES) or {}

        def want(pool_id):
            k = (kinds.get(pool_id) or {}).get("kind", "")
            return mark_of(table, pool_id, k)
        wrong = {}
        for to, m in shown["you"].items():
            w = want(to)
            if w and (m["sym"] != w or m["fill"] != table["colours"][w] or not m["nameShown"]):
                wrong[to] = {"want": w, "got": m["sym"], "fill": m["fill"], "named": m["nameShown"]}
        self.check("T2 your own pool: each mouth has the table's symbol and colour, with its name",
                   ok and bool(shown["you"]) and not wrong, {"wrong": wrong, "mouths": len(shown["you"])})
        wrong = {}
        for to, m in shown["mine:groups"].items():
            w, k = want(to), (kinds.get(to) or {}).get("kind", "")
            if (w and (m["sym"] != w or m["fill"] != table["colours"][w])) or (m["nameShown"] and k not in always):
                wrong[to] = {"want": w, "got": m["sym"], "named": m["nameShown"], "kind": k}
        self.check("T3 the Groups pool: the table's symbols and colours, and no name on a mouth the table does not name",
                   bool(shown["mine:groups"]) and not wrong, {"wrong": wrong, "mouths": len(shown["mine:groups"])})
        people = {to: m for to, m in shown["mine:friends"].items() if (kinds.get(to) or {}).get("kind") in always}
        wrong = {to: m for to, m in people.items() if not m["nameShown"] or m["fill"] != table["colours"][want(to)]}
        self.check("T4 the Friends pool: the mouths to people show their names, in the table's colour",
                   len(people) >= 2 and not wrong, {"people": sorted(people), "wrong": wrong})
        bad = {}
        for pid, d in kinds.items():
            w = mark_of(table, pid, d["kind"])
            col = table["colours"][w] if w else ""
            if d["colour"] != col:
                bad[pid] = {"kind": d["kind"], "want": col, "got": d["colour"]}
        self.check("T5 every pool's shore colour is the table's colour for its kind (none where the table has no symbol)",
                   bool(kinds) and not bad, bad)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), wc.make_handler(os.path.join(platform, "assets"),
                                                                           wc.mlc.page_markup(platform), wc.home_markup(platform)))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    r.platform = platform
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-walker-kindtable-")
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
        print("VERDICT walker-kind-table: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
