"""Big map levels (Grok Bot's audit AUDIT-CLAUDE.md on cb39901/44c3262, ranked fix 3; Brief 5 item 2): the server sends
up to 40 links of a kind (src/http_server.cpp map links, kMax = 40; a group's members are not capped at all), and the
ring grew until 16+ boxes ran off the frame while the 0.4x zoom floor drew 14 px labels at about 5.6 px.
Real Chromium, the real lgmap.js/lgmap.css and map_page_body's markup, no C++ server; the fake graph and the
readiness waits are map_live_check2.py's, plus a group with 40 members (user: ids) and Sitewide: 41 links.

Cases, on a desktop (1600x1000, the side panel open as on the live site) and a 390x844 phone:
  B0  control: a small level (The Gov, 5 links) shows all its boxes and no "+N more" box
  B1  the big group's level: every shown box is inside the map frame
  B2  every box's name is drawn at 11 px or more on screen (font size x the map's zoom); the you circle's initial and
      the small grey second lines (lgn-subt) are not names
  B3  a "+N more" box is shown and N is exactly what is left out: shown + N = 41
  B4  tapping "+N more" makes it the center (the trail gains it) with the rest on its ring, inside the frame, names
      >= 11 px; following "+N more" boxes to the end, every one of the 41 links is shown exactly once
  B5  Back from there returns to the big group, with the same boxes as before
  B6  ctrl+wheel out 12 notches at the big level: names stay >= 11 px (the zoom stops there)
  B7  the first view (You and your kinds) also draws every name at >= 11 px
Usage: LG_BROWSER=$(command -v google-chrome) python3 map_big_level_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT big-level: ALL-PASS" or the failed cases; writes <out>/map-big-level.json and screenshots. Exit 1 on a FAIL."""
import base64
import http.server
import json
import os
import sys
import threading
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import map_live_check2 as m2  # noqa: E402  (graph, handler, readiness; sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

N = m2.N
MEMBERS = ["member%02d" % i for i in range(1, 41)]
NODES = dict(m2.NODES)
NODES["group:big"] = N("group:big", "Roseville Neighbors", "group", "/c/big", 0.95)
for name in MEMBERS:
    NODES["user:" + name] = N("user:" + name, name, "person", "/u/" + name, 0.6)
LINKS = dict(m2.LINKS)
LINKS["mine:groups"] = LINKS["mine:groups"] + ["group:big"]
LINKS["group:big"] = ["sitewide"] + ["user:" + n for n in MEMBERS]
BIG = sorted(LINKS["group:big"])

# Each shown box's name: its on-screen font size (computed size x the text's screen scale) and its label.
NAMES = r"""JSON.stringify([...document.querySelectorAll('#lgm-nodes > g.lgn')].filter(g => getComputedStyle(g).display !== 'none')
  .map(g => { const t = g.querySelector('text:not(.lgn-subt):not(.lgn-initial)'); if (!t) return {id: g.getAttribute('data-id'), px: 0};
    const m = t.getScreenCTM(); return {id: g.getAttribute('data-id'), px: parseFloat(getComputedStyle(t).fontSize) * Math.hypot(m.a, m.b)}; }))"""


class BigRun(m2.Run2):
    def shot(self, name):
        with open(os.path.join(self.out, "map-big-%s.png" % name), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))

    def small_names(self):
        v = cdp.js(self.ws, NAMES)
        names = json.loads(v) if isinstance(v, str) else []
        return [n for n in names if n["px"] < 11 - 0.05], names

    def more_box(self):
        return [b for b in self.j("__m.boxes()") or [] if b["id"].startswith("more:")]

    def readable(self, name):
        small, names = self.small_names()
        self.check("B2 every box's name is drawn at >= 11 px: " + name, bool(names) and small == [],
                   {"small": small[:8], "smallest": min((n["px"] for n in names), default=None)})

    def level(self, tag, w, h):
        self.size(w, h)
        self.load()
        self.readable("%s first view (B7)" % tag)
        self.click_box("mine:groups", ["You", "Groups"])
        self.click_box("group:gov", ["You", "Groups", "The Gov"])
        got = self.ring_ids("group:gov")
        self.check("B0 %s: a small level shows all its boxes and no \"+N more\"" % tag,
                   got == sorted(LINKS["group:gov"]), got)
        self.click_sel("#lgm-trail .lgm-trail-step:nth-of-type(2)", ["You", "Groups"])
        self.click_box("group:big", ["You", "Groups", "Roseville Neighbors"])
        self.shot(tag + "-big")
        self.inside("B1 %s big group" % tag)
        self.readable(tag + " big group")
        first = [i for i in self.ring_ids("group:big") if not i.startswith("more:")]
        more = self.more_box()
        n_more = int(more[0]["label"].split()[0].lstrip("+")) if more and more[0]["label"].split()[0].lstrip("+").isdigit() else -1
        self.check("B3 %s: a \"+N more\" box, and shown + N = 41" % tag, len(more) == 1 and len(first) + n_more == 41,
                   {"shown": len(first), "more": [m["label"] for m in more]})
        seen, trail, pages = list(first), ["You", "Groups", "Roseville Neighbors"], 0
        while more and pages < 10:
            pages += 1
            label = more[0]["label"]
            trail = trail + [label]
            self.click_box(more[0]["id"], trail)
            self.shot("%s-more%d" % (tag, pages))
            self.inside("B4 %s \"%s\" page %d" % (tag, label, pages))
            self.readable("%s \"%s\" page %d" % (tag, label, pages))
            center = self.j("__m.boxes().find(b => (b.cls || '').includes('lgn-sel')) || null")
            items = [b["id"] for b in self.j("__m.boxes()") or []
                     if not b["id"].startswith("more:") and (not center or b["id"] != center["id"])]
            seen += items
            more = [b for b in self.more_box() if not center or b["id"] != center["id"]]
        self.check("B4 %s: following \"+N more\" shows every one of the 41 links exactly once" % tag,
                   sorted(seen) == BIG, {"seen": len(seen), "dupes": sorted({i for i in seen if seen.count(i) > 1}),
                                         "missing": sorted(set(BIG) - set(seen))})
        if pages:
            for _ in range(pages):
                trail = trail[:-1]
                self.click_sel("#lgm-back", trail)
            again = [i for i in self.ring_ids("group:big") if not i.startswith("more:")]
            self.check("B5 %s: Back returns to the big group with the same boxes" % tag, again == first,
                       {"before": len(first), "after": len(again)})
        f = self.j("__m.frame()")
        self.wheel((f["l"] + f["r"]) / 2, (f["t"] + f["b"]) / 2, 120, 12)
        self.shot(tag + "-zoomed-out")
        self.readable("%s after ctrl+wheel out 12 notches (B6)" % tag)

    def cases(self):
        self.level("desktop", 1600, 1000)
        self.level("phone", 390, 844)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = argv[1] if len(argv) > 1 else os.path.join("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."), "lg-map-big")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), m2.make_handler(platform, NODES, LINKS))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = BigRun("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-map-big-")
        r.rep["browser"] = r.ws.browser
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
        r.rep["missing_clicks"] = r.missing
        r.rep["verdict"] = "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails)
        path = os.path.join(out, "map-big-level.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT big-level: %s  (%d checks; report %s)" % (r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
