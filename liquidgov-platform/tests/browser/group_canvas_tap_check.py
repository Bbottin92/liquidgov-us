"""Tapping the group post box's canvas link opens the canvas, never the photo picker (Brandon 2026-11-12: "when i click
draw instead in a group page's post section, it just brings up camera option, pictures, or files. not the canvas").
Why it happened: the photo button is an invisible <input type=file> (font-size 4rem) inside a 2.4rem label that did not
clip it. iPhone Safari (WebKit) sizes that absolutely positioned input to its own width instead of stretching it to the
label, so it spilled out over the link just below, and a tap there opened the file picker. Chromium stretches it, so the
check first makes this Chromium page size the input the WebKit way (inset/size back to auto), shows that the input
then really is bigger than its label (positive control), and only then taps the canvas link. New file: no committed
check is edited.

Cases (Bbottin signed in, phone_serve.sh sample data), at phone width (390) and desktop width (1280):
  T0 precondition: Bbottin's group page has the post box, its photo input and the canvas link
  T1 positive control: with WebKit sizing the photo input is larger than its label (the bug's condition)
  T2 the point in the middle of the canvas link hits the link, not the photo input
  T3 a real tap (mouse event) there goes to the group's canvas page
Writes <out>/phone-groupcanvas-<label>.json and <out>/phone-groupcanvas-<label>-*.png (look at them).
"""
import json
import os
import time
import traceback
import urllib.parse

import cdp
import phone_check

LINK = "a[href$='/c/canvastap/canvas']"
WEBKIT_SIZING = ("(() => { const i = document.querySelector('.composerbar input[type=file]'); if (!i) return JSON.stringify(null);"
                 " Object.assign(i.style, {right: 'auto', bottom: 'auto', width: 'auto', height: 'auto'});"
                 " const ir = i.getBoundingClientRect(), lr = i.parentElement.getBoundingClientRect();"
                 " return JSON.stringify({input: [ir.width, ir.height], label: [lr.width, lr.height]}); })()")


class TapRun(phone_check.Run):
    def post(self, path, fields):
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v, safe="")) for k, v in fields.items())
        return self.j("fetch(%s, {method: 'POST', credentials: 'same-origin', redirect: 'manual',"
                      " headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: %s})"
                      ".then(r => JSON.stringify({status: r.status, type: r.type}))" % (json.dumps(path), json.dumps(body)))

    def width(self, w, h, mobile):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=2 if mobile else 1, mobile=mobile)

    def link_point(self):
        return self.j("(() => { const a = document.querySelector(%s); if (!a) return JSON.stringify(null);"
                      " a.scrollIntoView({block: 'center'}); const r = a.getBoundingClientRect();"
                      " const x = r.left + r.width / 2, y = r.top + r.height / 2; const h = document.elementFromPoint(x, y);"
                      " return JSON.stringify({x, y, ok: !!h && (h === a || a.contains(h)), hit: h ? h.outerHTML.slice(0, 90) : null}); })()"
                      % json.dumps(LINK))

    def cases(self):
        self.goto("/feed", settle=0.5)
        made = self.post("/communities", {"slug": "canvastap", "name": "Canvas Tap", "category": "3", "subcategory": "x", "about": "x"})
        self.check("T0 the group was made", isinstance(made, dict) and made.get("status") in (0, 302, 303), made)
        for w, h, mobile, tag in ((390, 844, True, "phone"), (1280, 900, False, "desktop")):
            self.width(w, h, mobile)
            self.goto("/c/canvastap", settle=1.5)
            have = self.j("JSON.stringify({input: !!document.querySelector('.composerbar input[type=file]'),"
                          " link: !!document.querySelector(%s)})" % json.dumps(LINK)) or {}
            self.check("T0 %s: the post box has its photo input and a canvas link" % tag, have.get("input") and have.get("link"), have)
            sz = self.j(WEBKIT_SIZING) or {}
            big = isinstance(sz, dict) and sz.get("input") and (sz["input"][0] > sz["label"][0] + 5 or sz["input"][1] > sz["label"][1] + 5)
            self.check("T1 %s: positive control, WebKit-sized the photo input is bigger than its label" % tag, big, sz)
            pt = self.link_point() or {}
            self.check("T2 %s: the middle of the canvas link hits the link, not the photo input" % tag, pt.get("ok"), pt)
            self.shot(tag)
            if pt.get("x") is not None:
                for t in ("mousePressed", "mouseReleased"):
                    self.ws.call("Input.dispatchMouseEvent", type=t, x=pt["x"], y=pt["y"], button="left", clickCount=1)
                landed = None
                for _ in range(25):
                    time.sleep(0.2)
                    landed = self.j("JSON.stringify(location.pathname)")
                    if landed and landed != "/c/canvastap":
                        break
                self.check("T3 %s: a tap there opens the group's canvas" % tag, landed == "/c/canvastap/canvas", landed)
            else:
                self.check("T3 %s: a tap there opens the group's canvas" % tag, False, "no link")


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    r = TapRun(base, "groupcanvas-" + label, out)
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
        path = os.path.join(out, "phone-groupcanvas-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT groupcanvas-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
