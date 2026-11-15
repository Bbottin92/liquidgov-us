"""The map has no hiding (Brandon 2026-11-14: "i want to get rid of the hidden objects on the mainstream map, that was a
bad idea and its causing clutter"). This file checked the Hidden mode added 2026-11-11; it now checks that it is gone
and that a box hidden with it before is shown again. Same browser, phone and sample data as map_bar_check.py.

Cases (Bbottin signed in, phone_serve.sh sample data):
  H0 no Hidden button, no kind chips, and the filter bar and its note are not shown
  H1 a box hidden with the old Hidden mode on this device (its localStorage entry, lgmap.filter.<you>) is shown, not
     faded, and so is a whole kind switched off with an old chip
  H2 a tap on a box does what a tap always does: it becomes the center (no tap-to-hide)
Writes <out>/phone-hidden-<label>.json and <out>/phone-hidden-<label>-*.png (look at them).
"""
import json
import os
import time
import traceback

import cdp
import map_bar_check


class HiddenRun(map_bar_check.BarRun):
    def box_state(self, box_id):
        return self.j("(() => { const g = document.querySelector('#lgm-nodes .lgn[data-id=%s]'); if (!g) return JSON.stringify(null);"
                      " return JSON.stringify({shown: getComputedStyle(g).display !== 'none' && g.getBoundingClientRect().width > 0,"
                      " faded: g.classList.contains('lgn-hidden')}); })()" % json.dumps(box_id))

    def cases(self):
        self.goto("/map", settle=3)
        self.close_tips()
        me = self.j("JSON.stringify(document.getElementById('lgmap').getAttribute('data-me'))") or ""
        gone = self.j("(() => { const f = document.getElementById('lgm-filter'), n = document.getElementById('lgm-filter-note');"
                      " const shown = e => !!e && getComputedStyle(e).display !== 'none' && e.getBoundingClientRect().height > 0;"
                      " return JSON.stringify({toggle: !!document.getElementById('lgm-hidetoggle'), chips: document.querySelectorAll('.lgm-chip').length,"
                      " bar: shown(f), note: shown(n)}); })()")
        self.check("H0 no Hidden button, no kind chips; the filter bar and its note are not shown",
                   gone == {"toggle": False, "chips": 0, "bar": False, "note": False}, gone)
        # what the old Hidden mode saved on this device: Friends hidden at You, every group hidden inside Groups
        self.j("(() => { localStorage.setItem('lgmap.filter.' + %s, JSON.stringify({you: ['mine:friends'], 'mine:groups': ['group']}));"
               " return '1'; })()" % json.dumps(me))
        self.goto("/map", settle=3)
        self.close_tips()
        s1 = self.box_state("mine:friends")
        self.tap_sel('#lgm-nodes .lgn[data-id="mine:groups"]')
        time.sleep(1)
        kids = self.j("JSON.stringify([...document.querySelectorAll('#lgm-nodes .lgn-group')].map(g => g.getAttribute('data-id')))") or []
        states = {k: self.box_state(k) for k in kids}
        self.check("H1 boxes hidden with the old Hidden mode on this device are shown, not faded (Friends at You, every group)",
                   isinstance(s1, dict) and s1["shown"] and not s1["faded"] and len(kids) >= 2
                   and all(isinstance(v, dict) and v["shown"] and not v["faded"] for v in states.values()),
                   {"friends": s1, "groups": states})
        self.check("H2 a tap on a box makes it the center, as always (Groups: trail You > Groups)", self.trail() == ["You", "Groups"],
                   self.trail())
        self.shot("groups")
        cdp.js(self.ws, "(() => { try { localStorage.clear(); } catch (e) {} return 1; })()")  # leave no state for other runs


def run(base, session, expected_js, label, out):
    """Run every case; returns the list of failed check names (empty = all pass). Same shape as phone_check.run."""
    os.makedirs(out, exist_ok=True)
    r = HiddenRun(base, "hidden-" + label, out)
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
        path = os.path.join(out, "phone-hidden-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT hidden-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
