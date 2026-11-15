"""The map's Hidden mode (Brandon 2026-11-11: "there should be a 'hidden' toggle that reveals all of the boxes that you
have hidden as translucent boxes that you can select to unhide or select a non-hidden box in that mode to hide it").
Same browser, phone and sample data as map_bar_check.py (reuses its BarRun helpers).

Cases (Bbottin signed in, phone_serve.sh sample data):
  H1 a Hidden button on the bar; tapping it turns Hidden mode on (pressed, says Done) and the note says what to tap
  H2 in Hidden mode a tap on a box hides it: it stays drawn but faded (lgn-hidden), the trail does not move (no
     navigation), and the server still lists it (hiding is never deleting)
  H3 Done: the faded box disappears, the button counts it ("Hidden (1)"), and at You its chip shows off too
  H4 after a reload it is still hidden
  H5 Hidden mode again: the hidden box is back, faded; tapping it shows it; Done leaves it shown
  H6 a whole kind hidden by its chip (Groups' groups): in Hidden mode a tap on one faded group shows only that one;
     the others stay hidden
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

    def toggle(self):
        return self.j("(() => { const b = document.getElementById('lgm-hidetoggle'); if (!b) return JSON.stringify(null);"
                      " return JSON.stringify({label: b.textContent.trim(), on: b.getAttribute('aria-pressed') === 'true'}); })()")

    def note(self):
        return self.j("(() => { const n = document.getElementById('lgm-filter-note'); return JSON.stringify(n && !n.hidden ? n.textContent : ''); })()")

    def cases(self):
        self.goto("/map", settle=3)
        self.close_tips()
        t0 = self.toggle()
        self.check("H1 the bar has a Hidden button, off", isinstance(t0, dict) and t0.get("label", "").startswith("Hidden") and not t0["on"], t0)
        self.tap_sel("#lgm-hidetoggle")
        t1, n1 = self.toggle(), self.note()
        self.check("H1 tapping it turns Hidden mode on (pressed, says Done)", isinstance(t1, dict) and t1["on"] and t1["label"] == "Done", t1)
        self.check("H1 ...and the note says what to tap", isinstance(n1, str) and "tap" in n1.lower(), n1)
        trail0 = self.trail()
        self.tap_sel('#lgm-nodes .lgn[data-id="mine:friends"]')
        s2 = self.box_state("mine:friends")
        self.check("H2 in Hidden mode a tap hides the box: still drawn, faded", isinstance(s2, dict) and s2["shown"] and s2["faded"], s2)
        self.check("H2 ...and does not move you (trail unchanged)", self.trail() == trail0, {"before": trail0, "after": self.trail()})
        srv = self.j("fetch('/map/node?n=you', {credentials: 'same-origin'}).then(r => r.json())"
                     ".then(d => JSON.stringify(d.links.map(l => l.node.id)))")
        self.check("H2 hiding is not deleting: the server still lists it", isinstance(srv, list) and "mine:friends" in srv, srv)
        self.shot("mode-on")
        self.tap_sel("#lgm-hidetoggle")
        s3, t3, chips3 = self.box_state("mine:friends"), self.toggle(), self.chips()
        fchip = [c for c in chips3 if c["key"] == "mine:friends"] if isinstance(chips3, list) else []
        self.check("H3 Done: the box is gone", isinstance(s3, dict) and not s3["shown"], s3)
        self.check("H3 ...the button counts it (Hidden (1))", isinstance(t3, dict) and t3["label"] == "Hidden (1)" and not t3["on"], t3)
        self.check("H3 ...and its chip shows off", fchip and not fchip[0]["on"], chips3)
        self.shot("done")
        self.goto("/map", settle=3)
        self.close_tips()
        s4 = self.box_state("mine:friends")
        self.check("H4 after a reload it is still hidden", isinstance(s4, dict) and not s4["shown"], s4)
        self.tap_sel("#lgm-hidetoggle")
        s5 = self.box_state("mine:friends")
        self.check("H5 Hidden mode shows it again, faded", isinstance(s5, dict) and s5["shown"] and s5["faded"], s5)
        self.tap_sel('#lgm-nodes .lgn[data-id="mine:friends"]')
        s5b = self.box_state("mine:friends")
        self.check("H5 tapping the faded box shows it", isinstance(s5b, dict) and s5b["shown"] and not s5b["faded"], s5b)
        self.tap_sel("#lgm-hidetoggle")
        s5c = self.box_state("mine:friends")
        self.check("H5 Done leaves it shown", isinstance(s5c, dict) and s5c["shown"] and not s5c["faded"], s5c)

        self.tap_sel('#lgm-nodes .lgn[data-id="mine:groups"]')
        time.sleep(1)
        kids = self.j("JSON.stringify([...document.querySelectorAll('#lgm-nodes .lgn-group')].map(g => g.getAttribute('data-id')))")
        self.check("H6 precondition: Groups is the center with at least 2 groups", self.trail() == ["You", "Groups"] and
                   isinstance(kids, list) and len(kids) >= 2, {"trail": self.trail(), "groups": kids})
        kids = kids if isinstance(kids, list) else []
        self.tap_sel('#lgm-filter .lgm-chip[data-key="group"]')
        gone = [self.box_state(k) for k in kids]
        self.check("H6 precondition: the Groups chip hid every group", gone and all(isinstance(s, dict) and not s["shown"] for s in gone), gone)
        self.tap_sel("#lgm-hidetoggle")
        if kids:
            self.tap_sel('#lgm-nodes .lgn[data-id="%s"]' % kids[0])
        self.tap_sel("#lgm-hidetoggle")
        after = {k: self.box_state(k) for k in kids}
        first = after.get(kids[0]) if kids else None
        rest = [after[k] for k in kids[1:]]
        self.check("H6 one tapped group comes back; the others stay hidden",
                   isinstance(first, dict) and first["shown"] and rest and all(isinstance(s, dict) and not s["shown"] for s in rest), after)
        self.shot("one-group")
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
