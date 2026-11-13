"""Map SPEC phase 1, the bars (scratch/2026-11-11-grokbot-map-spec/SPEC.md sections 8-10; Brandon 2026-11-11 "yes, build
phase 1"): a filter bar per level, Back (the phone's back gesture too) and Zoom out, sticky on a phone.
Same browser, phone and sample data as phone_check.py (it reuses phone_check.Run). New file: phone_check.py is frozen.

Cases (Bbottin signed in, phone_serve.sh sample data):
  B0 precondition: the server serves the very lgmap.js under test
  B1 the bar at You has one chip per box on the ring, same names, all on
  B2 tapping a chip hides that box and its line; the other boxes stay (B2b); the server still lists it (hiding is not
     deleting, B2c); the bar says so (B2d)
  B3 after a reload the chip is still off and the box still hidden (remembered on this device)
  B4 tapping the chip again shows the box
  B5 tapping a kind (Groups) makes it the center, the bar then holds that level's chips (B5b); the browser's Back
     (history.back, what the phone's back gesture does) returns to You (B5c)
  B6 the trail's previous box (Back; Brandon 2026-11-13: no Zoom out button) from a kind goes up one level, to You
  B7 the MAINstream pill from You goes to the MAINstream
  B8 on a phone the trail and the bar stay at the top (position: sticky)
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

    def chips(self):
        return self.j("JSON.stringify([...document.querySelectorAll('#lgm-filter .lgm-chip')].map(b => ({key: b.getAttribute('data-key'),"
                      " label: b.textContent.trim(), on: b.getAttribute('aria-pressed') === 'true'})))")

    def tap_sel(self, selector):
        """A real tap on the element's center. Records (rep["taps"]) what was under that point and whether a click
        reached the element, so a tap that does nothing says why (2026-11-11: chip and Zoom out taps did nothing)."""
        r = self.j("(() => { const e = document.querySelector(%s); if (!e) return JSON.stringify({none: true});"
                   " const b = e.getBoundingClientRect(), x = (b.left + b.right) / 2, y = (b.top + b.bottom) / 2;"
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
        ring, chips = self.ring(), self.chips()
        names = sorted(r["label"] for r in ring) if isinstance(ring, list) else None
        self.check("B1 precondition: the ring has at least 3 boxes", isinstance(ring, list) and len(ring) >= 3, ring)
        self.check("B1 the bar has one chip per ring box, same names, all on",
                   isinstance(chips, list) and names == sorted(c["label"] for c in chips) and all(c["on"] for c in chips),
                   {"ring": names, "chips": chips})
        self.shot("you")
        target = next((c for c in chips if c["key"] == "mine:friends"), chips[0] if chips else None) if isinstance(chips, list) else None
        if not target:
            self.check("B2 precondition: a chip to tap", False, chips)
            target = {"key": "mine:friends"}  # the cases below still run, and fail on what they find
        self.tap_sel('#lgm-filter .lgm-chip[data-key="%s"]' % target["key"])
        ring2 = self.ring()
        gone = [r for r in ring2 if r["id"] == target["key"]] if isinstance(ring2, list) else None
        self.check("B2 tapping a chip hides that box", gone is not None and gone and not gone[0]["shown"], ring2)
        line = self.j("(() => { const p = [...document.querySelectorAll('#lgm-edges .lge')].filter(e => e.getAttribute('data-a') === %s"
                      " || e.getAttribute('data-b') === %s); return JSON.stringify(p.map(e => getComputedStyle(e).display)); })()"
                      % (json.dumps(target["key"]), json.dumps(target["key"])))
        self.check("B2 ...and its line", isinstance(line, list) and line and all(d == "none" for d in line), line)
        others = [r for r in ring2 if r["id"] != target["key"]] if isinstance(ring2, list) else []
        self.check("B2b the other ring boxes stay", others and all(r["shown"] for r in others), ring2)
        srv = self.j("fetch('/map/node?n=you', {credentials: 'same-origin'}).then(r => r.json())"
                     ".then(d => JSON.stringify(d.links.map(l => l.node.id)))")
        self.check("B2c hiding is not deleting: the server still lists it", isinstance(srv, list) and target["key"] in srv, srv)
        note = self.j("(() => { const n = document.getElementById('lgm-filter-note'); return JSON.stringify(n && !n.hidden ? n.textContent : ''); })()")
        self.check("B2d the bar says hiding only changes this view", isinstance(note, str) and "only" in note.lower(), note)
        self.shot("hidden")
        self.goto("/map", settle=3)
        self.close_tips()
        ring3, chips3 = self.ring(), self.chips()
        still = [r for r in ring3 if r["id"] == target["key"]] if isinstance(ring3, list) else None
        c3 = [c for c in chips3 if c["key"] == target["key"]] if isinstance(chips3, list) else None
        self.check("B3 after a reload the chip is still off and the box hidden",
                   still and not still[0]["shown"] and c3 and not c3[0]["on"], {"ring": ring3, "chips": chips3})
        self.tap_sel('#lgm-filter .lgm-chip[data-key="%s"]' % target["key"])
        ring4 = self.ring()
        back = [r for r in ring4 if r["id"] == target["key"]] if isinstance(ring4, list) else None
        self.check("B4 tapping the chip again shows the box", back and back[0]["shown"], ring4)

        if not self.tap_sel('#lgm-nodes .lgn[data-id="mine:groups"]'):
            self.check("B5 precondition: Groups is on the ring", False, ring4)
        time.sleep(1)
        t5, chips5 = self.trail(), self.chips()
        self.check("B5 tapping Groups makes it the center (trail You > Groups)", t5 == ["You", "Groups"], t5)
        self.check("B5b the bar now holds Groups' own chips", isinstance(chips5, list) and chips5 and
                   not any(c["key"].startswith("mine:") for c in chips5), chips5)
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
        self.tap_sel("#lgm-main")
        time.sleep(1)
        t7 = self.trail()
        root = self.j("JSON.stringify(!!document.querySelector('#lgm-nodes .lgn-root'))")
        self.check("B7 the MAINstream pill from You goes to the MAINstream", root is True and t7 and t7[-1] == "MAINstream", {"trail": t7, "root": root})
        self.shot("mainstream")
        st = self.j("(() => { const b = document.getElementById('lgm-bars'); return JSON.stringify(b ? getComputedStyle(b).position : null); })()")
        self.check("B8 on a phone the trail and bar stay at the top (sticky)", st == "sticky", st)


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
