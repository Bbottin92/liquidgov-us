"""The You ring shows all your kinds, never "+N more" (Brandon 2026-11-12: on his tablet, portrait at about 640 CSS px,
under the 760 px phone cutoff, the You start view showed only Politics and "+6 more"; capRing from 17e7b56 capped the
You ring by what fits at the readable zoom floor).
Real Chromium, the real lgmap.js/lgmap.css and map_page_body's markup, no C++ server; the fake graph (its "you" links
are the seven kinds) and the readiness waits are map_live_check2.py's.

Cases, the You start view at each size:
  Y1  every one of your kinds (map_live_check2's LINKS["you"]) is drawn
  Y2  no "+N more" box on the You level
  Y3  (640x1024 tablet portrait and 390x844 phone only) every box is inside the map frame
Sizes: 640x1024 and 390x844 (Y1-Y3); 360x640, 760x1024, 1024x640, 844x390, 1280x800 (Y1-Y2).
Usage: LG_BROWSER=$(command -v google-chrome) python3 map_you_ring_all_kinds_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT you-ring: ALL-PASS" or the failed cases; writes <out>/map-you-ring.json and screenshots. Exit 1 on a FAIL."""
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

KINDS = sorted(m2.LINKS["you"])
FRAMED = [(640, 1024), (390, 844)]
OTHER = [(360, 640), (760, 1024), (1024, 640), (844, 390), (1280, 800)]


class YouRun(m2.Run2):
    def shot(self, name):
        with open(os.path.join(self.out, "map-you-ring-%s.png" % name), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))

    def view(self, w, h, framed):
        tag = "%dx%d" % (w, h)
        self.size(w, h)
        self.load()
        self.shot(tag)
        ids = [b["id"] for b in self.j("__m.boxes()") or []]
        self.check("Y1 %s: all %d of your kinds are drawn" % (tag, len(KINDS)), sorted(i for i in ids if i in KINDS) == KINDS,
                   {"missing": [k for k in KINDS if k not in ids], "drawn": ids})
        more = [i for i in ids if i.startswith("more:")]
        self.check("Y2 %s: no \"+N more\" box on the You level" % tag, more == [], more)
        if framed:
            out = self.j("__m.outside()")
            self.check("Y3 %s: every box inside the map frame" % tag, out == [], {"outside": out, "frame": self.j("__m.frame()")})

    def cases(self):
        for w, h in FRAMED:
            self.view(w, h, True)
        for w, h in OTHER:
            self.view(w, h, False)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = argv[1] if len(argv) > 1 else os.path.join("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."), "lg-map-you-ring")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), m2.make_handler(platform, m2.NODES, m2.LINKS))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = YouRun("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-map-you-ring-")
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
        path = os.path.join(out, "map-you-ring.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT you-ring: %s  (%d checks; report %s)" % (r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))