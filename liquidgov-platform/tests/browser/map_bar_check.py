"""Map SPEC phase 1, the bars (scratch/2026-11-11-grokbot-map-spec/SPEC.md sections 8-10; Brandon 2026-11-11 "yes, build
phase 1"): a filter bar per level, Back (the phone's back gesture too) and Zoom out, sticky on a phone.
Same browser, phone and sample data as phone_check.py (it reuses phone_check.Run). New file: phone_check.py is frozen.

Cases (Bbottin signed in, phone_serve.sh sample data). The filter chips (old B1-B4, B5b) went with the map's Hidden
mode (Brandon 2026-11-14: "i want to get rid of the hidden objects on the mainstream map"); map_hidden_check.py checks
they are gone.
  B0 precondition: the server serves the very lgmap.js under test
  B1 precondition: the ring around You has at least 3 boxes
  B5 tapping a kind (Groups) makes it the center; the browser's Back (history.back, what the phone's back gesture
     does) returns to You (B5c)
  B6 the trail's previous box (Back; Brandon 2026-11-13: no Zoom out button) from a kind goes up one level, to You
  (B7, the MAINstream pill above the map, is gone: the map is fullscreen, 2026-11-15)
  B8 on a phone the trail stays at the top (position: sticky)
Run: ci_map_bar_check.py (GitHub CI, a report-only step next to the phone check; this laptop rarely has the RAM).
Writes <out>/phone-bar-<label>.json and <out>/phone-bar-<label>-*.png (look at them).
"""
import json
import os
import time
import traceback

import cdp
import phone_check


class BarRun(phone_check.Run):
    def ring(self):
        """Labels and ids of the boxes around the center, and which are shown."""
        return self.j("JSON.stringify([...document.querySelectorAll('#lgm-nodes .lgn')].filter(g => !g.classList.contains('lgn-you'))"
                      ".map(g => ({id: g.getAttribute('data-id'), label: g.querySelector('title').textContent,"
                      " shown: getComputedStyle(g).display !== 'none' && g.getBoundingClientRect().width > 0})))")

    def goto(self, path, settle=1.5):
        """The map starts zoomed in around your drop now (2026-11-15, Brandon: "keep it zoomed in"), with the far boxes
        off the frame; this check taps boxes by their place, so on the map it zooms out first (phone_check's)."""
        super().goto(path, settle)
        if path == "/map":
            self.map_zoom_out()

    def tap_sel(self, selector):
        """On a phone the walker's pad is always over the bottom of the screen now, and the map's frame runs under it:
        a box down there is scrolled up the page first, as a person would, then tapped; and one that the scrolling put
        under the map's sticky bars is scrolled back down; anything else off the screen (the MAINstream pill above the map,
        after such a scroll) is scrolled into view (2026-11-15)."""
        self.j("(() => { const e = document.querySelector(%s); if (!e) return '0'; const b = e.getBoundingClientRect(),"
               " y = (b.top + b.bottom) / 2; if (e.closest('#lgm-nodes') && (y > 560 || y < 160)) window.scrollBy(0, y - 450);"
               " else if (!e.closest('#lgm-nodes') && (b.top < 0 || b.bottom > innerHeight)) e.scrollIntoView({block: 'center'}); return '1'; })()" % json.dumps(selector))
        time.sleep(0.2)
        return self._tap_sel(selector)

    def _tap_sel(self, selector):
        """A real tap on the element's center. Records (rep["taps"]) what was under that point and whether a click
        reached the element, so a tap that does nothing says why (2026-11-11: chip and Zoom out taps did nothing)."""
        r = self.j("(() => { const e = document.querySelector(%s); if (!e) return JSON.stringify({none: true});"
                   " const b = e.getBoundingClientRect(); let x = (b.left + b.right) / 2, y = (b.top + b.bottom) / 2;"
                   # your drop takes taps (its menu, 2026-11-14): a box under it is tapped where the drop is not
                   " const free = (px, py) => { const h = document.elementFromPoint(px, py); return !(h && h.closest('#lgm-walker')); };"
                   " if (!free(x, y)) for (const [fx, fy] of [[0.2, 0.5], [0.8, 0.5], [0.5, 0.2], [0.5, 0.8]]) {"
                   "  const px = b.left + fx * b.width, py = b.top + fy * b.height; if (free(px, py)) { x = px; y = py; break; } }"
                   " const top = document.elementFromPoint(x, y);"
                   " window.__clicked = false; e.addEventListener('click', () => { window.__clicked = true; }, {once: true, capture: true});"
                   " return JSON.stringify({x: x, y: y, under: top ? (top.id || top.tagName + '.' + (top.getAttribute('class') || '')) : null,"
                   "  inside: !!top && (top === e || e.contains(top))}); })()" % json.dumps(selector))
        if not isinstance(r, dict) or r.get("none"):
            self.rep.setdefault("taps", []).append({"sel": selector, "found": False})
            return False
        # A finger down and up (what Puppeteer's tap sends). CI run 43091618237: synthesizeTapGesture landed on the
        # right element (inside=True) but this headless Chrome sent no click after it, on any element; a phone does.
        pt = [{"x": r["x"], "y": r["y"], "id": 1}]
        self.ws.call("Input.dispatchTouchEvent", type="touchStart", touchPoints=pt)
        self.ws.call("Input.dispatchTouchEvent", type="touchEnd", touchPoints=[])
        time.sleep(1.5)
        r["clicked"] = self.j("JSON.stringify(window.__clicked === true)")
        r["sel"] = selector
        self.rep.setdefault("taps", []).append(r)
        print("tap %s: under=%s inside=%s clicked=%s" % (selector, r.get("under"), r.get("inside"), r.get("clicked")), flush=True)
        return True

    def close_tips(self):
        """Close the Juketube tip a first-time visitor sees, with its own close button, as a person would: it sits
        over the lower part of the map on a phone, so a tap there would land on the tip instead."""
        cdp.js(self.ws, "(() => { const c = document.getElementById('jbhintclose'); if (c && c.offsetParent) c.click(); return 1; })()")
        time.sleep(0.5)

    def trail(self):
        return self.j("JSON.stringify([...document.querySelectorAll('#lgm-trail .lgm-trail-step')].map(b => b.textContent.trim()))")

    def cases(self):
        # Each case runs even when an earlier one failed: a missing precondition fails that case and moves on (lesson
        # 2026-11-11: an early return left B3-B8 never run on the old build).
        self.goto("/map", settle=3)
        self.close_tips()
        ring4 = self.ring()
        self.check("B1 precondition: the ring has at least 3 boxes", isinstance(ring4, list) and len(ring4) >= 3, ring4)
        self.shot("you")

        if not self.tap_sel('#lgm-nodes .lgn[data-id="mine:groups"]'):
            self.check("B5 precondition: Groups is on the ring", False, ring4)
        time.sleep(1)
        t5 = self.trail()
        self.check("B5 tapping Groups makes it the center (trail You > Groups)", t5 == ["You", "Groups"], t5)
        self.shot("groups")
        cdp.js(self.ws, "history.back(), 1")
        time.sleep(2)
        t5c = self.trail()
        self.check("B5c Back (the phone's back gesture) returns to You", t5c == ["You"], t5c)
        self.tap_sel('#lgm-nodes .lgn[data-id="mine:groups"]')
        time.sleep(1)
        before = self.trail()
        self.tap_sel("#lgm-trail .lgm-trail-prev")
        t6 = self.trail()
        self.check("B6 the previous box (Back) from Groups goes up to You", before == ["You", "Groups"] and t6 == ["You"], {"before": before, "after": t6})
        # B7 (the MAINstream pill from You) went with the strip above the map (Brandon 2026-11-15: the map is fullscreen
        # under the site header; nothing else around it)
        self.shot("mainstream")
        st = self.j("(() => { const b = document.getElementById('lgm-bars'); return JSON.stringify(b ? getComputedStyle(b).position : null); })()")
        self.check("B8 on a phone the trail stays at the top (sticky)", st == "sticky", st)


def run(base, session, expected_js, label, out):
    """Run every case; returns the list of failed check names (empty = all pass). Same shape as phone_check.run."""
    os.makedirs(out, exist_ok=True)
    r = BarRun(base, "bar-" + label, out)
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
        path = os.path.join(out, "phone-bar-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT bar-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
