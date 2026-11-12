"""Profile CSS on the real page can't cover the site header or the plain-view link (Brandon 2026-11-12: profile CSS
covers the whole profile page "except the title banner thing"; decisions.md). A deliberately hostile style puts a
red full-screen fixed box with the top z-index over everything; .lg-profile-outer (contain:paint) must keep it inside
the profile area. New file: phone_check.py and the other committed checks are not edited.

Cases (Bbottin signed in, phone_serve.sh sample data), at phone width (390) and desktop width (1280):
  C0 precondition: the hostile CSS saved, and the browser applied it (positive controls: the ::before box is
     position:fixed, and the h1 has the CSS's color), so a pass below is not a style that simply never loaded
  C1 the point in the middle of the site header still hits the header (nothing painted over it)
  C2 a nav link in the header still hits that link (it can be clicked)
  C3 the plain-view link still hits itself
  C4 the red box stays inside the profile area (its box ends where .lg-profile-outer ends)
  C5 ?plain=1 drops the style: no red box, default h1 color
Writes <out>/phone-csscontain-<label>.json and <out>/phone-csscontain-<label>-*.png (look at them).
"""
import json
import os
import traceback
import urllib.parse

import cdp
import phone_check

RED = "rgb(255, 0, 0)"
H1 = "rgb(162, 59, 59)"
HOSTILE = (".lg-theme::before{content:\"FAKE LOGIN\";position:fixed;top:0;left:0;right:0;bottom:0;"
           "z-index:2147483647;background:#f00} .lg-theme h1{color:#a23b3b}")


class ContainRun(phone_check.Run):
    def hit(self, selector):
        """What element the point in the middle of selector's box hits, and whether that is selector itself or inside it."""
        return self.j("(() => { const e = document.querySelector(%s); if (!e) return JSON.stringify(null);"
                      " const r = e.getBoundingClientRect(); if (!r.width || !r.height) return JSON.stringify({zero: true});"
                      " const x = r.left + r.width / 2, y = r.top + r.height / 2;"
                      " const h = document.elementFromPoint(x, y);"
                      " return JSON.stringify({x, y, hit: h ? h.tagName + '.' + h.className : null, ok: !!h && (h === e || e.contains(h))}); })()"
                      % json.dumps(selector))

    def styles(self):
        return self.j("(() => { const t = document.querySelector('.lg-profile-scope .lg-theme'); const o = document.querySelector('.lg-profile-outer');"
                      " const h = document.querySelector('.lg-profile-scope h1');"
                      " if (!t || !o || !h) return JSON.stringify({missing: [!!t, !!o, !!h]});"
                      " const b = getComputedStyle(t, '::before'); const orr = o.getBoundingClientRect();"
                      " return JSON.stringify({before_pos: b.position, before_bg: b.backgroundColor, h1: getComputedStyle(h).color,"
                      " outer: [orr.top, orr.bottom, orr.left, orr.right], header_bottom: document.querySelector('header.top').getBoundingClientRect().bottom,"
                      " vh: innerHeight}); })()")

    def save_css(self, css):
        body = "css=" + urllib.parse.quote(css, safe="")
        return self.j("fetch('/me/css', {method: 'POST', credentials: 'same-origin', redirect: 'manual',"
                      " headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: %s})"
                      ".then(r => JSON.stringify({status: r.status, type: r.type}))" % json.dumps(body))

    def width(self, w, h, mobile):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=2 if mobile else 1, mobile=mobile)

    def cases(self):
        self.goto("/me", settle=1)
        saved = self.save_css(HOSTILE)
        self.check("C0 the hostile CSS was saved", isinstance(saved, dict) and saved.get("status") in (0, 302, 303), saved)
        for w, h, mobile, tag in ((390, 844, True, "phone"), (1280, 800, False, "desktop")):
            self.width(w, h, mobile)
            self.goto("/u/Bbottin", settle=1.5)
            st = self.styles()
            self.check("C0 %s: the browser applied the hostile style (::before is fixed and red)" % tag,
                       isinstance(st, dict) and st.get("before_pos") == "fixed" and st.get("before_bg") == RED, st)
            self.check("C0 %s: positive control, the h1 has the CSS's color" % tag, isinstance(st, dict) and st.get("h1") == H1, st)
            self.check("C1 %s: the middle of the site header hits the header" % tag,
                       (self.hit("header.top") or {}).get("ok"), self.hit("header.top"))
            self.check("C2 %s: a header link is still hit (clickable)" % tag,
                       (self.hit("header.top a") or {}).get("ok"), self.hit("header.top a"))
            self.check("C3 %s: the plain-view link is still hit" % tag,
                       (self.hit(".lg-plain-view a") or {}).get("ok"), self.hit(".lg-plain-view a"))
            ok4 = isinstance(st, dict) and "outer" in st and st["outer"][0] >= st["header_bottom"] - 1
            self.check("C4 %s: the profile area starts below the header (the red box can't reach it)" % tag, ok4, st)
            self.shot(tag)
            self.goto("/u/Bbottin?plain=1", settle=1)
            st5 = self.styles()
            self.check("C5 %s: plain view drops the style" % tag,
                       isinstance(st5, dict) and st5.get("before_bg") != RED and st5.get("h1") != H1, st5)
            self.shot(tag + "-plain")
        self.goto("/me", settle=0.5)
        self.save_css("")  # leave no state for other runs


def run(base, session, expected_js, label, out):
    """Run every case; returns the list of failed check names (empty = all pass). Same shape as phone_check.run
    (expected_js is unused: this check does not read lgmap.js)."""
    os.makedirs(out, exist_ok=True)
    r = ContainRun(base, "csscontain-" + label, out)
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
        path = os.path.join(out, "phone-csscontain-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT csscontain-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
