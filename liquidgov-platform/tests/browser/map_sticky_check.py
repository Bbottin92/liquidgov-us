"""The map's bars stay at the top of a phone screen. Since 2026-11-15 the map is fullscreen under the site header
(Brandon: "the mainstream map should be like fullscreen besides the liquidgov title and notifications and mail and
profile pic and juketube"), so the page itself does not scroll at all and the bars cannot be scrolled away: the cases
check that, on the screen (lessons L229/L270: positions, not computed styles). The old cases scrolled the page past
the bars and are gone with the scrolling.

Cases (Bbottin signed in, phone_serve.sh sample data, the 390x844 phone of phone_check.py):
  S1 the map page does not scroll: its height is the screen's, and a scroll attempt leaves it at the top
  S2 the bars sit right under the site header (within 2 px) and stay there after the scroll attempt
  S3 the map frame runs from the bars to the bottom of the screen (within 2 px): fullscreen under the header
  S4 what a finger hits there is the bars: elementFromPoint at the center of the trail's first button is that button
Run: ci_map_sticky_check.py (GitHub CI, report-only like the others). Writes <out>/phone-sticky-<label>.json and
<out>/phone-sticky-<label>-*.png. Prints "VERDICT sticky-<label>: ..."."""
import json
import os
import time
import traceback

import cdp
import map_bar_check


class StickyRun(map_bar_check.BarRun):
    def geo(self):
        return self.j("(() => { const b = document.getElementById('lgm-bars'), c = document.querySelector('.lgmap-canvas');"
                      " if (!b || !c) return JSON.stringify(null); const r = b.getBoundingClientRect();"
                      " return JSON.stringify({bars_top: r.top, bars_h: r.height, canvas_top: c.getBoundingClientRect().top,"
                      " canvas_bottom: c.getBoundingClientRect().bottom, scroll_y: window.scrollY,"
                      " doc_h: document.documentElement.scrollHeight, view_h: window.innerHeight}); })()")

    def cases(self):
        self.goto("/map", settle=3)
        self.close_tips()
        g0 = self.geo()
        head = self.j("JSON.stringify((h => h ? h.getBoundingClientRect().bottom : null)(document.querySelector('header.top')))")
        cdp.js(self.ws, "(() => { window.scrollTo(0, 400); document.scrollingElement.scrollTop = 400; return 1; })()")
        time.sleep(0.6)
        g = self.geo()
        self.rep["geometry"] = {"before": g0, "after_scroll_attempt": g, "header_bottom": head}
        self.check("S1 the map page does not scroll (as tall as the screen; a scroll attempt leaves it at the top)",
                   isinstance(g, dict) and g["scroll_y"] == 0 and g["doc_h"] <= g["view_h"] + 1, g)
        self.check("S2 the bars sit right under the site header and stay there after the scroll attempt",
                   isinstance(g, dict) and isinstance(g0, dict) and isinstance(head, (int, float))
                   and abs(g["bars_top"] - head) <= 2 and abs(g0["bars_top"] - head) <= 2 and g["bars_h"] > 0,
                   {"bars": [g0 and g0.get("bars_top"), g and g.get("bars_top")], "header_bottom": head})
        self.check("S3 the map frame runs from the bars to the bottom of the screen",
                   isinstance(g, dict) and abs(g["canvas_bottom"] - g["view_h"]) <= 2 and g["canvas_top"] <= g["bars_top"] + 1, g)
        hit = self.j("(() => { const z = document.querySelector('#lgm-trail button'); if (!z) return JSON.stringify(null);"
                     " const r = z.getBoundingClientRect(), e = document.elementFromPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2);"
                     " return JSON.stringify({top: r.top, hit: e ? (e.id || e.tagName) : null, ok: !!e && (e === z || z.contains(e))}); })()")
        self.check("S4 a finger on the trail's first button hits it (nothing covers the bars)",
                   isinstance(hit, dict) and hit["ok"] and 0 <= hit["top"] < 160, hit)
        self.shot("fullscreen")


def run(base, session, expected_js, label, out):
    """Run every case; returns the list of failed check names (empty = all pass). Same shape as phone_check.run."""
    os.makedirs(out, exist_ok=True)
    r = StickyRun(base, "sticky-" + label, out)
    edge = None
    try:
        if not r.case0_fresh(expected_js):
            return r.fails
        edge, r.ws = cdp.launch_edge()
        r.rep["browser"] = r.ws.browser
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.ws.call("Emulation.setDeviceMetricsOverride", width=390, height=844, deviceScaleFactor=2, mobile=True)
        r.ws.call("Emulation.setUserAgentOverride", userAgent=cdp.UA, platform="iPhone")
        r.ws.call("Emulation.setTouchEmulationEnabled", enabled=True, maxTouchPoints=5)
        host = r.base.split("//", 1)[1].split("/")[0].split(":")[0]
        r.ws.call("Network.setCookie", name="lg_session", value=session, domain=host, path="/")
        r.cases()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if edge is not None:
            cdp.stop_edge(edge, r.ws.profile if r.ws else None)
        r.rep["verdict"] = "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails)
        path = os.path.join(out, "phone-sticky-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT sticky-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
