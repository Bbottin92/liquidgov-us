"""The map's bars stay at the top of a phone screen WHILE THE PAGE SCROLLS (Grok Bot's check of wip/32, should-fix 5;
lessons L229/L270: check the position on screen, not the computed style). map_bar_check.py B8 reads only
getComputedStyle(...).position === 'sticky', which stays true when an ancestor traps the sticky box (an overflow:hidden
canvas does exactly that), so it passes without ever scrolling. map_bar_check.py is committed and not edited.

Cases (Bbottin signed in, phone_serve.sh sample data, the 390x844 phone of phone_check.py):
  S1 precondition: the page really scrolls past the bars' own place (the map frame's top is above the screen's top).
     If the page is too short to scroll that far, a tall empty block is added AFTER the map body (outside the frame,
     so no ancestor of the bars changes) and that is recorded in the report.
  S2 control: with the bars made position:static for a moment, the same scroll moves them off the top of the screen
     (so S3 can't pass merely because nothing moved). Restored straight after.
  S3 the bars' top edge is at the screen's top (within 2 px), with the same height as before the scroll
  S4 what a finger hits there is the bars: elementFromPoint at the center of the bars' first filter button is that
     button (nothing, e.g. a fixed site header, covers them). Updated 2026-11-15 with Brandon's OK: it used the
     Drop/Scroll switch, which is gone (walking is always on).
Not covered: iOS before 16 (no overflow:clip, falls back to hidden, which traps sticky): look on a real old iPhone.
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

    def scroll_past(self):
        """Scroll so the map frame's top is 80 px above the screen's top. Returns the geometry after."""
        cdp.js(self.ws, "(() => { const c = document.querySelector('.lgmap-canvas'); if (!c) return 0;"
                        " window.scrollTo(0, window.scrollY + c.getBoundingClientRect().top + 80); return 1; })()")
        time.sleep(0.8)
        return self.geo()

    def cases(self):
        self.goto("/map", settle=3)
        self.close_tips()
        g0 = self.geo()
        self.rep["before_scroll"] = g0
        g = self.scroll_past()
        if isinstance(g, dict) and g["canvas_top"] > -40:
            # the page is too short to scroll that far: make it longer below the map body, outside the frame
            cdp.js(self.ws, "(() => { const s = document.createElement('div'); s.id = 'lgm-test-spacer';"
                            " s.style.height = '1600px'; document.body.appendChild(s); return 1; })()")
            self.rep["spacer_added"] = True
            g = self.scroll_past()
        self.rep["geometry"] = g
        self.check("S1 precondition: the page scrolled past the bars' own place (frame top above the screen)",
                   isinstance(g, dict) and g["canvas_top"] <= -40 and g["canvas_bottom"] > 120, g)
        ctl = self.j("(() => { const b = document.getElementById('lgm-bars'); if (!b) return JSON.stringify(null);"
                     " const was = b.style.position; b.style.position = 'static'; const t = b.getBoundingClientRect().top;"
                     " b.style.position = was; return JSON.stringify(t); })()")
        self.check("S2 control: without sticky the same scroll moves the bars off the top", isinstance(ctl, (int, float))
                   and ctl < -30, ctl)
        g2 = self.geo()
        self.check("S3 scrolled: the bars' top edge is at the screen's top (within 2 px), same height as unscrolled",
                   isinstance(g2, dict) and isinstance(g0, dict) and abs(g2["bars_top"]) <= 2 and g2["bars_h"] > 0
                   and abs(g2["bars_h"] - g0["bars_h"]) <= 1, {"scrolled": g2, "before": g0})
        hit = self.j("(() => { const z = document.querySelector('#lgm-filter button'); if (!z) return JSON.stringify(null);"
                     " const r = z.getBoundingClientRect(), e = document.elementFromPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2);"
                     " return JSON.stringify({top: r.top, hit: e ? (e.id || e.tagName) : null, ok: !!e && (e === z || z.contains(e))}); })()")
        self.check("S4 scrolled: a finger on the bars' first filter button hits it (nothing covers the bars)",
                   isinstance(hit, dict) and hit["ok"] and 0 <= hit["top"] < 120, hit)
        self.shot("scrolled")


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
