"""Phone browser check for liquidgov (Windows, headless Edge emulating an iPhone: 390x844, touch, iOS UA).

CI and the e2e suites only see the server's HTML; they cannot tell how the map reacts to a finger. This check does,
so run it (through run_phone_check.py) before and after any change to assets/lgmap.js, assets/lgmap.css or the
map page, and on the old build too: a new case must fail there. Moved into the repo 2026-11-09 from
scratch/2026-11-09-claude-site-sweep/browser_check.py (side-agent note: CI can't see tap/hold/double-tap/screen reader).

Cases (logged in as the operator Bbottin, sample data from phone_serve.sh):
  0  precondition: the server serves the very lgmap.js under test (byte-equal), so a stale server can't pass
  1  /admin/departments scrolled to the end: the footer's text ends above the Juketube button
  2  /map first view: zoomed in around you (you whole and in the middle of the frame, no boxes overlapping)
  2b zoomed out by you (the wheel; a pinch does the same): the whole ring fits the frame, you in the middle
     (cases 3, 4c and 4d zoom out first, then tap a box as before)
  3  quick tap (Chromium's own tap gesture, checked < 200 ms in page time): connections grow, no page sheet
  4  press and hold 750 ms: the box grows on top of its neighbours (4b), then the page sheet opens
  4c double tap, two taps stamped 150 ms apart: the page sheet opens. Each DevTools call waits for the page, which
     is busy ~350 ms after a tap, so unstamped taps land too far apart; the precondition checks the gap the page saw.
     A real phone stamps a touch when the finger lands - assumed, confirm on an iPhone after map timing changes.
  4d two taps 600 ms apart: no page sheet
  5  /settings on a phone: the section index is visible (9 links); plus screenshots of /proposals, /laws/2, /u/pal
  6  screen readers: an "Open <name>" role=button right after each box with a page (6a), at most 2x2 px (6b), in
     the accessibility tree (6c), and its click - what VoiceOver/TalkBack send on a double tap - opens the sheet (6d)
Writes <out>/phone-<label>.json and <out>/phone-<label>-*.png (look at the screenshots of any UI change).
"""
import base64
import hashlib
import json
import os
import time
import traceback
import urllib.request

import cdp


class Run:
    def __init__(self, base, label, out):
        self.base, self.label, self.out = base.rstrip("/"), label, out
        self.rep = {"label": label, "base": self.base, "checks": {}}
        self.fails = []
        self.ws = None

    def check(self, name, ok, detail):
        self.rep["checks"][name] = {"ok": bool(ok), "detail": detail}
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:400]), flush=True)
        if not ok:
            self.fails.append(name)

    def j(self, expr):
        v = cdp.js(self.ws, expr)
        try:
            return json.loads(v) if isinstance(v, str) else v
        except ValueError:  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def shot(self, name):
        with open(os.path.join(self.out, "phone-%s-%s.png" % (self.label, name)), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))

    def goto(self, path, settle=1.5):
        self.ws.call("Page.navigate", url=self.base + path)
        for _ in range(80):
            if cdp.js(self.ws, "document.readyState") == "complete":
                break
            time.sleep(0.25)
        else:
            self.check("page load: %s reached readyState complete" % path, False, "still loading after 20 s")
        time.sleep(settle)

    def sheet(self):
        """Did the box's page open? Since 2026-11-15 a page opens as the page itself, through the portal (Brandon: the
        side panel "should be abandoned"), so "the page sheet opens" means the browser left the map for that page.
        Both keys say the same (the cases were written for the sheet and its panel)."""
        left = cdp.js(self.ws, "location.pathname") not in ("/map", "/", None)
        return {"sheet": left, "panel": left, "path": cdp.js(self.ws, "location.pathname")}

    def map_zoom_out(self):
        """Zoom the map out to the whole ring, the way a person does (2026-11-15, Brandon: "yes update the phone check,
        keep it zoomed in"): the map now starts zoomed in around your drop, with the far boxes off the frame, so the
        cases that tap a box by its place zoom out first. The wheel over the top of the map; a pinch does the same
        (walker_controls_check Z2). Returns the scale before and after."""
        f = self.j("(() => { const b = document.getElementById('lgm-svg').getBoundingClientRect();"
                   " return JSON.stringify({x: (b.left + b.right) / 2, y: Math.max(b.top, 0) + 40, s: window.LG_MAP ? LG_MAP.scale() : null}); })()")
        if not isinstance(f, dict):
            return None
        # until the whole map fits (the scale reaches LG_MAP.fitScale()), at most 40 notches: pools are big, so more are needed
        for _ in range(40):
            now = self.j("JSON.stringify([LG_MAP.scale(), LG_MAP.fitScale()])")
            if isinstance(now, list) and now[0] <= now[1] * 1.02:
                break
            self.ws.call("Input.dispatchMouseEvent", type="mouseWheel", x=f["x"], y=f["y"], deltaX=0, deltaY=100)
            time.sleep(0.05)
        time.sleep(0.8)
        return [f.get("s"), self.j("JSON.stringify(window.LG_MAP ? LG_MAP.scale() : null)")]

    def box(self, index, skip_root=False):
        """Centre of the index-th map box fully inside the map frame."""
        root = " && !/lgn-root/.test(o.g.getAttribute('class'))" if skip_root else ""
        return self.j("(() => { const fb = document.getElementById('lgm-svg').getBoundingClientRect();"
                      " const vis = [...document.querySelectorAll('#lgm-nodes .lgn')].map(g => ({g: g, b: g.getBoundingClientRect()}))"
                      "  .filter(o => o.b.left >= fb.left && o.b.right <= fb.right && o.b.top >= fb.top && o.b.bottom <= fb.bottom" + root + ");"
                      " const o = vis[%d]; if (!o) return JSON.stringify({none: true, visible: vis.length});"
                      " return JSON.stringify({x: (o.b.left + o.b.right) / 2, y: (o.b.top + o.b.bottom) / 2,"
                      "  label: o.g.querySelector('title').textContent}); })()" % index)

    def two_taps(self, x, y, gap_s):
        """Two quick taps with explicit input timestamps; returns (pointerup times the page saw, sheet state)."""
        # kept in sessionStorage: the double tap leaves the map for the page, and a window variable would go with it
        cdp.js(self.ws, "(() => { sessionStorage.setItem('__up', '[]'); document.addEventListener('pointerup', e => {"
                        " const a = JSON.parse(sessionStorage.getItem('__up')); a.push(e.timeStamp); sessionStorage.setItem('__up', JSON.stringify(a)); },"
                        " {capture: true}); return 1; })()")
        t0 = time.time()
        pt = [{"x": x, "y": y, "id": 1}]
        for typ, dt in (("touchStart", 0), ("touchEnd", 0.05), ("touchStart", gap_s), ("touchEnd", gap_s + 0.05)):
            self.ws.call("Input.dispatchTouchEvent", type=typ, touchPoints=pt if typ == "touchStart" else [], timestamp=t0 + dt)
        time.sleep(2)
        return self.j("JSON.stringify(JSON.parse(sessionStorage.getItem('__up') || '[]').map(Math.round))"), self.sheet()

    # ---- the cases ----
    def case0_fresh(self, expected_js):
        try:
            served = urllib.request.urlopen(self.base + "/assets/lgmap.js", timeout=10).read()
        except OSError as e:
            self.check("0 precondition: the server serves the lgmap.js under test", False, "fetch failed: %s" % e)
            return False
        want = open(expected_js, "rb").read()
        same = hashlib.sha1(served).hexdigest() == hashlib.sha1(want).hexdigest()
        self.check("0 precondition: the server serves the lgmap.js under test", same,
                   {"served_bytes": len(served), "file": expected_js, "file_bytes": len(want)})
        return same

    def case1_page_end(self):
        self.goto("/admin/departments")
        r = self.j("(() => { window.scrollTo(0, document.documentElement.scrollHeight); return new Promise(res => setTimeout(() => {"
                   " const f = document.querySelector('footer.site') || document.querySelector('main');"
                   " const t = document.getElementById('jbtoggle'); const a = f && f.getBoundingClientRect(), b = t && t.getBoundingClientRect();"
                   " const range = document.createRange(); range.selectNodeContents(f); const rs = [...range.getClientRects()].filter(x => x.height > 0);"
                   " const textBottom = rs.length ? Math.max(...rs.map(x => x.bottom)) : (a && a.bottom);"
                   " res(JSON.stringify({textBottom: textBottom, toggle: b && [b.left, b.top, b.right, b.bottom]}));"
                   " }, 400)); })()")
        self.shot("departments-bottom")
        ok = isinstance(r, dict) and r.get("toggle") and r.get("textBottom") is not None and r["textBottom"] <= r["toggle"][1]
        self.check("1 page end (footer text) scrolls clear of the Juketube button", ok, r)

    def case2_first_view(self):
        self.goto("/map", settle=3)
        # Edited 2026-11-11 with Brandon's OK ("yes"): the first view is a ring around you (M1.1 look), not a column
        # starting at the frame top. Every box fully inside the frame, you in the middle third, and no two boxes
        # overlapping (the 2026-11-11 screenshots showed side boxes covering you; nothing here caught it).
        # Edited 2026-11-15 with Brandon's OK ("yes update the phone check, keep it zoomed in"): walking is always on,
        # so the first view is zoomed in around you (the ring's far side is off the frame); zoomed out by you, the
        # whole ring fits the frame as before. Both are checked: 2 (first view) and 2b (zoomed out).
        look = ("(() => { const fb = document.getElementById('lgm-svg').getBoundingClientRect();"
                " const els = [...document.querySelectorAll('#lgm-nodes > g.lgn')].filter(g => getComputedStyle(g).visibility !== 'hidden');"
                " const bx = g => g.querySelector('.lgs').getBoundingClientRect();"
                " const boxes = els.map(bx).filter(b => b.width > 0);"
                " const you = document.querySelector('#lgm-nodes > g.lgn-you'); const yb = you && bx(you);"
                " let overlaps = 0; for (let i = 0; i < boxes.length; i++) for (let k = i + 1; k < boxes.length; k++) {"
                "  const a = boxes[i], b = boxes[k]; if (a.left < b.right - 1 && b.left < a.right - 1 && a.top < b.bottom - 1 && b.top < a.bottom - 1) overlaps++; }"
                " return JSON.stringify({frame: [fb.left, fb.top, fb.right, fb.bottom], n: boxes.length, overlaps: overlaps,"
                "  left: Math.min(...boxes.map(b => b.left)), right: Math.max(...boxes.map(b => b.right)),"
                "  top: Math.min(...boxes.map(b => b.top)), bottom: Math.max(...boxes.map(b => b.bottom)),"
                "  youMid: yb ? (yb.left + yb.right) / 2 : null, youTop: yb ? yb.top : null, youBottom: yb ? yb.bottom : null,"
                "  scale: window.LG_MAP ? LG_MAP.scale() : null, fit: window.LG_MAP && LG_MAP.fitScale ? LG_MAP.fitScale() : null}); })()")
        r = self.j(look)
        self.shot("map-first-view")
        third = lambda q: q["frame"][0] + (q["frame"][2] - q["frame"][0]) / 3 <= q["youMid"] <= q["frame"][2] - (q["frame"][2] - q["frame"][0]) / 3
        ok = (isinstance(r, dict) and r.get("n", 0) >= 3 and r["overlaps"] == 0 and r["youMid"] is not None and third(r)
              and r["youTop"] >= r["frame"][1] - 0.5 and r["youBottom"] <= min(r["frame"][3], 844) + 0.5
              and bool(r.get("fit")) and r["scale"] > r["fit"] * 1.2)
        self.check("2 map: the first view is zoomed in around you (you whole and in the middle, no boxes overlapping)", ok, r)
        z = self.map_zoom_out()
        r = self.j(look)
        self.shot("map-zoomed-out")
        ok = (isinstance(r, dict) and r.get("n", 0) >= 3 and r["overlaps"] == 0
              and r["left"] >= r["frame"][0] - 0.5 and r["right"] <= r["frame"][2] + 0.5
              and r["top"] >= r["frame"][1] - 0.5 and r["bottom"] <= r["frame"][3] + 0.5
              and r["youMid"] is not None and third(r))
        self.check("2b map, zoomed out by you: the ring around you fits the frame, you in the middle, no boxes overlapping", ok,
                   {"scales": z, "view": r})

    def case3_quick_tap(self):
        name = "3 quick tap: connections grow, no page sheet"
        self.goto("/map", settle=3)
        self.map_zoom_out()
        target = self.j("(() => { const fb = document.getElementById('lgm-svg').getBoundingClientRect();"
                        " const vis = [...document.querySelectorAll('#lgm-nodes .lgn')].filter(g => !g.classList.contains('lgn-open'))"
                        "  .map(g => ({g: g, b: g.getBoundingClientRect()}))"
                        "  .filter(o => o.b.left >= fb.left && o.b.right <= fb.right && o.b.top >= fb.top && o.b.bottom <= fb.bottom);"
                        " const o = vis[0]; if (!o) return JSON.stringify({none: true}); o.g.setAttribute('data-pc', '1');"
                        " return JSON.stringify({x: (o.b.left + o.b.right) / 2, y: (o.b.top + o.b.bottom) / 2}); })()")
        if not (isinstance(target, dict) and "x" in target):
            return self.check(name, False, {"target": target})
        # Chromium's own tap gesture keeps a real tap's duration (two dispatchTouchEvent calls took ~500 ms of page
        # time); the page measures it, by the same event timestamps lgmap.js uses to tell a tap from a hold.
        cdp.js(self.ws, "(() => { window.__tap = {}; document.addEventListener('pointerdown', e => window.__tap.d = e.timeStamp, {capture: true, once: true});"
                        " document.addEventListener('pointerup', e => window.__tap.u = e.timeStamp, {capture: true, once: true}); return 1; })()")
        self.ws.call("Input.synthesizeTapGesture", x=target["x"], y=target["y"], duration=60, tapCount=1, gestureSourceType="touch")
        time.sleep(2.5)
        tap_ms = self.j("JSON.stringify(window.__tap.u && window.__tap.d ? Math.round(window.__tap.u - window.__tap.d) : null)")
        after = self.j("JSON.stringify({sheet: location.pathname !== '/map', panel: location.pathname !== '/map',"
                       " open: !!document.querySelector('[data-pc]') && document.querySelector('[data-pc]').classList.contains('lgn-open')})")
        self.shot("after-quick-tap")
        self.check("3 precondition: the tap really was quick in page time (< 200 ms)", isinstance(tap_ms, int) and tap_ms < 200, tap_ms)
        self.check(name, isinstance(after, dict) and after.get("open") and not after.get("sheet") and not after.get("panel"),
                   {"target": target, "after": after})

    def case4_hold(self):
        name = "4 press and hold: the box grows, then the page sheet opens"
        self.goto("/map", settle=3)
        # Zoomed out like 3, 4c and 4d, and a box your drop is not standing on (2026-11-14: your drop takes taps now,
        # for its menu, so a hold at the middle of the box under it reaches the drop, not the box)
        self.map_zoom_out()
        target = self.box(1)
        if not (isinstance(target, dict) and "x" in target):
            return self.check(name, False, {"target": target})
        self.ws.call("Input.dispatchTouchEvent", type="touchStart", touchPoints=[{"x": target["x"], "y": target["y"], "id": 1}])
        time.sleep(0.6)
        mid = self.j("(() => { const g = document.querySelector('#lgm-nodes .lgn.lgn-hold');"
                     " return JSON.stringify({holding: !!g, ready: !!document.querySelector('.lgn-ready'),"
                     "  onTop: !!g && g === [...document.querySelectorAll('#lgm-nodes .lgn')].pop()}); })()")
        self.check("4b the held box is drawn on top of its neighbours", isinstance(mid, dict) and mid.get("onTop"), mid)
        self.shot("while-holding")
        time.sleep(0.15)
        self.ws.call("Input.dispatchTouchEvent", type="touchEnd", touchPoints=[])
        time.sleep(2)
        after = self.sheet()
        self.shot("after-hold")
        self.check(name, isinstance(after, dict) and after.get("sheet") and after.get("panel") and isinstance(mid, dict) and mid.get("ready"),
                   {"target": target, "mid": mid, "after": after})

    def case4cd_double_tap(self):
        for case, gap_s, lo, hi, want_open in (("4c", 0.15, 100, 250, True), ("4d", 0.6, 500, 10 ** 6, False)):
            name = ("4c double tap: the page sheet opens" if want_open else "4d slow second tap: no page sheet")
            self.goto("/map", settle=3)
            self.map_zoom_out()
            target = self.box(1)
            if not (isinstance(target, dict) and "x" in target):
                self.check(name, False, {"target": target})
                continue
            ups, after = self.two_taps(target["x"], target["y"], gap_s)
            gap = ups[1] - ups[0] if isinstance(ups, list) and len(ups) == 2 else None
            self.check("%s precondition: the page saw two taps %d-%s ms apart" % (case, lo, hi if hi < 10 ** 6 else "+"),
                       gap is not None and lo <= gap <= hi, ups)
            if want_open:
                self.shot("after-double-tap")
            opened = isinstance(after, dict) and after.get("sheet") and after.get("panel")
            closed = isinstance(after, dict) and not after.get("sheet") and not after.get("panel")
            self.check(name, opened if want_open else closed, {"target": target, "after": after})

    def case6_screen_reader(self):
        self.goto("/map", settle=3)
        sr = self.j("(() => { const out = []; [...document.querySelectorAll('#lgm-nodes .lgn')].forEach(g => {"
                    " const nx = g.nextElementSibling; const s = nx && nx.classList.contains('lgn-sr') ? nx : null; const b = s && s.getBoundingClientRect();"
                    " out.push({label: g.querySelector('title').textContent, sr: s ? s.getAttribute('aria-label') : null,"
                    "  role: s && s.getAttribute('role'), w: b ? b.width : null, h: b ? b.height : null}); });"
                    " return JSON.stringify({boxes: out, srTotal: document.querySelectorAll('#lgm-nodes .lgn-sr').length}); })()")
        boxes = sr.get("boxes", []) if isinstance(sr, dict) else []
        with_sr = [b for b in boxes if b.get("sr")]
        self.check("6 precondition: the map has boxes", len(boxes) >= 3, len(boxes))
        self.check("6a an 'Open <name>' button right after each box with one (none stray)",
                   len(with_sr) >= 2 and len(with_sr) == sr.get("srTotal")
                   and all(b["sr"] == "Open " + b["label"] and b["role"] == "button" for b in with_sr), sr)
        self.check("6b the buttons are not visible (at most 2x2 px)",
                   bool(with_sr) and all((b["w"] or 0) <= 2 and (b["h"] or 0) <= 2 for b in with_sr),
                   [(b["sr"], b["w"], b["h"]) for b in with_sr])
        ax = self.ws.call("Accessibility.getFullAXTree")
        names = [((n.get("role") or {}).get("value"), (n.get("name") or {}).get("value"))
                 for n in (ax or {}).get("nodes", []) if not n.get("ignored")]
        ax_open = sorted(nm for r, nm in names if r == "button" and isinstance(nm, str) and nm.startswith("Open "))
        self.check("6c screen readers see them: buttons named 'Open ...' in the accessibility tree",
                   len(ax_open) == len(with_sr) and len(ax_open) >= 2, ax_open)
        name = "6d clicking 'Open <name>' (a screen reader's double tap) opens the page sheet"
        pick = with_sr[1] if len(with_sr) > 1 else (with_sr[0] if with_sr else None)  # below the root if it has one
        if not pick:
            return self.check(name, False, "no button found")
        self.j("(() => { const s = [...document.querySelectorAll('#lgm-nodes .lgn-sr')].find(e => e.getAttribute('aria-label') === "
               + json.dumps(pick["sr"]) + "); s.dispatchEvent(new MouseEvent('click', {bubbles: true})); return '1'; })()")
        time.sleep(2)
        after = self.sheet()
        self.shot("after-sr-open")
        self.check(name, isinstance(after, dict) and after.get("sheet") and after.get("panel"), {"pick": pick["sr"], "after": after})

    def case5_pages(self):
        # the Settings index must really be visible on a phone (the phone CSS hides every <nav>; the e2e check only
        # saw it in the HTML)
        for path, name in (("/settings", "settings"), ("/proposals", "proposals"), ("/laws/2", "laws2"), ("/u/pal", "profile-pal")):
            self.goto(path)
            self.shot(name)
            if name == "settings":
                r = self.j("(() => { const e = document.querySelector('.setidx'); const b = e && e.getBoundingClientRect();"
                           " return JSON.stringify({there: !!e, h: b ? Math.round(b.height) : 0, links: e ? e.querySelectorAll('a').length : 0}); })()")
                self.check("5 settings index visible on a phone (height > 0, 9 links)",
                           isinstance(r, dict) and r.get("h", 0) > 0 and r.get("links") == 9, r)


def run(base, session, expected_js, label, out):
    """Run every case; returns the list of failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    r = Run(base, label, out)
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
        r.case1_page_end()
        r.case2_first_view()
        r.case3_quick_tap()
        r.case4_hold()
        r.case4cd_double_tap()
        r.case6_screen_reader()
        r.case5_pages()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if edge is not None:
            cdp.stop_edge(edge, r.ws.profile if r.ws else None)
        r.rep["verdict"] = "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails)
        with open(os.path.join(out, "phone-%s.json" % label), "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT %s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]),
                                                          os.path.join(out, "phone-%s.json" % label)), flush=True)
    return r.fails
