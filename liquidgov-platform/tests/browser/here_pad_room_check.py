"""The dots' Walk button and D-pad never leave page content stuck underneath them (Brandon 2026-11-12, "do the dpad
fix": on a phone the fixed D-pad sat over whatever was bottom-left, e.g. "Play in Juketube" and the footer). While the
pad is on screen the page gets that much room at its end, so scrolled to the bottom every bit of content clears it.
New file: no committed check is edited.

Cases (Bbottin signed in and chose Yes, phone_serve.sh sample data, /u/stranger), at phone (390) and desktop (1280):
  R0 precondition: the Walk button is on the page, fixed at the bottom-left (positive control: it really sits over the
     page area, so a pass below is not a pad that simply is not there)
  R1 not walking: scrolled to the end, the footer's last line of text ends above the Walk button
  R2 walking (D-pad open): scrolled to the end, the footer's last line of text ends above the D-pad
  R3 stop walking: the extra room shrinks back to the Walk button's size (no leftover gap the size of the D-pad)
Writes <out>/phone-herepad-<label>.json and <out>/phone-herepad-<label>-*.png (look at them).
"""
import json
import os
import time
import traceback
import urllib.parse

import cdp
import phone_check

MEASURE = ("(() => { window.scrollTo(0, document.documentElement.scrollHeight); return new Promise(res => setTimeout(() => {"
           " const f = document.querySelector('footer.site'); const pad = document.getElementById('lg-here-pad');"
           " if (!f || !pad) return res(JSON.stringify({missing: [!!f, !!pad]}));"
           " const range = document.createRange(); range.selectNodeContents(f);"
           " const rs = [...range.getClientRects()].filter(x => x.height > 0);"
           " const pr = pad.getBoundingClientRect();"
           " res(JSON.stringify({textBottom: rs.length ? Math.max(...rs.map(x => x.bottom)) : null, padTop: pr.top, padBottom: pr.bottom,"
           " padLeft: pr.left, vh: innerHeight, docH: document.documentElement.scrollHeight,"
           " room: parseFloat(getComputedStyle(document.body).paddingBottom) || 0}));"
           " }, 500)); })()")


class PadRun(phone_check.Run):
    def post(self, path, fields):
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v, safe="")) for k, v in fields.items())
        return self.j("fetch(%s, {method: 'POST', credentials: 'same-origin', redirect: 'manual',"
                      " headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: %s})"
                      ".then(r => JSON.stringify({status: r.status}))" % (json.dumps(path), json.dumps(body)))

    def width(self, w, h, mobile):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=2 if mobile else 1, mobile=mobile)

    def walk(self):
        self.j("(() => { document.querySelector('#lg-here-pad .lg-here-walk').click(); return JSON.stringify(1); })()")
        time.sleep(0.4)

    def cases(self):
        self.goto("/settings", settle=0.5)
        self.post("/here/choice", {"choice": "1", "back": "/settings"})
        for w, h, mobile, tag in ((390, 844, True, "phone"), (1280, 900, False, "desktop")):
            self.width(w, h, mobile)
            self.goto("/u/stranger", settle=2)
            m = self.j(MEASURE) or {}
            self.check("R0 %s: the Walk button is fixed at the bottom-left over the page" % tag,
                       isinstance(m, dict) and m.get("padBottom") and m["padBottom"] >= m["vh"] - 40 and m.get("padLeft", 99) < 40, m)
            self.check("R1 %s: not walking, the page's end clears the Walk button" % tag,
                       m.get("textBottom") is not None and m.get("padTop") is not None and m["textBottom"] <= m["padTop"], m)
            closed_room = m.get("room", 0)
            self.shot(tag + "-end")
            self.walk()
            m2 = self.j(MEASURE) or {}
            self.check("R2 %s: walking, the page's end clears the D-pad" % tag,
                       m2.get("textBottom") is not None and m2.get("padTop") is not None and m2["textBottom"] <= m2["padTop"], m2)
            self.shot(tag + "-walking-end")
            self.walk()   # stop walking
            m3 = self.j(MEASURE) or {}
            self.check("R3 %s: stop walking, the room shrinks back to the Walk button's size" % tag,
                       isinstance(m3, dict) and abs(m3.get("room", -1) - closed_room) <= 2 and m3.get("room", 0) < m2.get("room", 0),
                       (closed_room, m2.get("room"), m3.get("room")))


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    r = PadRun(base, "herepad-" + label, out)
    edge = None
    try:
        edge, r.ws = cdp.launch_edge()
        r.rep["browser"] = r.ws.browser
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
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
        path = os.path.join(out, "phone-herepad-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT herepad-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
