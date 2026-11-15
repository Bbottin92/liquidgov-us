"""The dots' pad never leaves page content stuck underneath it (Brandon 2026-11-12, "do the dpad fix": on a phone the
fixed D-pad sat over whatever was bottom-left, e.g. "Play in Juketube" and the footer). The page gets the pad's height
as room at its end, so scrolled to the bottom every bit of content clears it.
Updated 2026-11-15 with Brandon's OK: walking is always on (no Drop/Scroll switch), so the pad is always there; the
old "not walking" / "stop walking" cases (R1, R3) became R1 and R3 below.

Cases (Bbottin signed in and chose Yes, phone_serve.sh sample data, /u/stranger), at phone (390) and desktop (1280):
  R0 precondition: the pad is on the page, fixed at the bottom (positive control: it really sits over the page area,
     so a pass below is not a pad that simply is not there)
  R1 scrolled to the end, the footer's last line of text ends above the pad
  R2 after walking, scrolled to the end, it still does
  R3 the room is the pad's size, not more (no leftover gap: at most the pad's height + 40 px)
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
           " padLeft: pr.left, padH: pr.height, vh: innerHeight, docH: document.documentElement.scrollHeight,"
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
        for k in ("ArrowRight", "ArrowDown"):
            self.ws.call("Input.dispatchKeyEvent", type="keyDown", key=k, code=k, windowsVirtualKeyCode={"ArrowRight": 39, "ArrowDown": 40}[k])
            time.sleep(0.3)
            self.ws.call("Input.dispatchKeyEvent", type="keyUp", key=k, code=k, windowsVirtualKeyCode={"ArrowRight": 39, "ArrowDown": 40}[k])
        time.sleep(0.3)

    def cases(self):
        self.goto("/settings", settle=0.5)
        self.post("/here/choice", {"choice": "1", "back": "/settings"})
        for w, h, mobile, tag in ((390, 844, True, "phone"), (1280, 900, False, "desktop")):
            self.width(w, h, mobile)
            self.goto("/u/stranger", settle=2)
            m = self.j(MEASURE) or {}
            self.check("R0 %s: the pad is fixed at the bottom over the page" % tag,
                       isinstance(m, dict) and m.get("padBottom") and m["padBottom"] >= m["vh"] - 40 and m.get("padH", 0) > 44, m)
            self.check("R1 %s: the page's end clears the pad" % tag,
                       m.get("textBottom") is not None and m.get("padTop") is not None and m["textBottom"] <= m["padTop"], m)
            self.shot(tag + "-end")
            self.walk()
            m2 = self.j(MEASURE) or {}
            self.check("R2 %s: after walking, the page's end still clears the pad" % tag,
                       m2.get("textBottom") is not None and m2.get("padTop") is not None and m2["textBottom"] <= m2["padTop"], m2)
            self.shot(tag + "-walking-end")
            self.check("R3 %s: the room is the pad's size, not more" % tag,
                       isinstance(m2, dict) and m2.get("padH", 0) <= m2.get("room", 0) <= m2.get("padH", 0) + 40,
                       (m2.get("padH"), m2.get("room")))


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
