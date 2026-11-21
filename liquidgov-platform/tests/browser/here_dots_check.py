"""Dots on profile pages, in a real browser (Brandon 2026-11-12: "walk around using a d pad ... walk up to a button or
text field and select it"). New file: no committed check is edited.

Cases (Bbottin signed in, phone_serve.sh sample data), at phone width (390) and desktop width (1280):
  D0 precondition: Bbottin chose Yes; /u/stranger has the dots script; Bbottin's own dot appears (round portrait + handle)
  D1 the dot layer sits below the site header (the middle of the header still hits the header)
  D2 Walk shows the D-pad; arrow keys move your dot (its box moves right)
  D3 walking up to "Send friend request" marks it (lg-here-near), and A presses it: the request is sent (stranger is
     then listed as pending on /friends); positive control first: before the walk nothing is marked
  D4 tapping still works: a plain click on a link navigates (walk mode never replaces clicking)
Writes <out>/phone-heredots-<label>.json and <out>/phone-heredots-<label>-*.png (look at them).
"""
import json
import os
import time
import traceback
import urllib.parse

import cdp
import phone_check

KEYCODES = {"ArrowRight": 39, "ArrowLeft": 37, "ArrowUp": 38, "ArrowDown": 40}


class DotsRun(phone_check.Run):
    def post(self, path, fields):
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v, safe="")) for k, v in fields.items())
        return self.j("fetch(%s, {method: 'POST', credentials: 'same-origin', redirect: 'manual',"
                      " headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: %s})"
                      ".then(r => JSON.stringify({status: r.status, type: r.type}))" % (json.dumps(path), json.dumps(body)))

    def width(self, w, h, mobile):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=2 if mobile else 1, mobile=mobile)

    def key(self, name, hold_s):
        self.ws.call("Input.dispatchKeyEvent", type="keyDown", key=name, code=name, windowsVirtualKeyCode=KEYCODES[name])
        time.sleep(hold_s)
        self.ws.call("Input.dispatchKeyEvent", type="keyUp", key=name, code=name, windowsVirtualKeyCode=KEYCODES[name])

    def state(self):
        return self.j("(() => { const d = document.querySelector('#lg-here .lg-here-dot.me');"
                      " const b = Array.from(document.querySelectorAll('form[action=\"/friends\"] button'))[0];"
                      " const n = document.querySelector('.lg-here-near');"
                      " const dr = d && d.getBoundingClientRect(); const br = b && b.getBoundingClientRect();"
                      " return JSON.stringify({dot: dr ? [dr.left + dr.width / 2 + scrollX, dr.top + 19 + scrollY] : null,"
                      " btn: br ? [br.left + br.width / 2 + scrollX, br.top + br.height / 2 + scrollY] : null,"
                      " near: n ? n.tagName + ':' + n.textContent.trim().slice(0, 30) : null,"
                      " name: d && d.querySelector('.lg-here-name') ? d.querySelector('.lg-here-name').textContent : null, img: !!(d && d.querySelector('img,.lg-here-ph')),"
                      " pad: !!document.querySelector('#lg-here-pad .lg-here-keys:not([hidden])')}); })()")

    def hit(self, selector):
        return self.j("(() => { const e = document.querySelector(%s); if (!e) return JSON.stringify(null);"
                      " const r = e.getBoundingClientRect(); const h = document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2);"
                      " return JSON.stringify({ok: !!h && (h === e || e.contains(h)), hit: h ? h.tagName + '.' + h.className : null}); })()"
                      % json.dumps(selector))

    def walk_to_button(self, limit_s=25):
        end = time.time() + limit_s
        while time.time() < end:
            st = self.state() or {}
            if st.get("near", "") and "friend request" in (st.get("near") or "").lower():
                return st
            if not st.get("dot") or not st.get("btn"):
                return st
            dx, dy = st["btn"][0] - st["dot"][0], st["btn"][1] - st["dot"][1]
            if abs(dy) > 12:
                self.key("ArrowDown" if dy > 0 else "ArrowUp", min(0.6, abs(dy) / 175.0))
            elif abs(dx) > 12:
                self.key("ArrowRight" if dx > 0 else "ArrowLeft", min(0.6, abs(dx) / 175.0))
            else:
                self.key("ArrowDown", 0.05)
            time.sleep(0.08)
        return self.state()

    def cases(self):
        self.goto("/settings", settle=0.5)
        chosen = self.post("/here/choice", {"choice": "1", "back": "/settings"})
        self.check("D0 Bbottin chose Yes", isinstance(chosen, dict) and chosen.get("status") in (0, 302, 303), chosen)
        for w, h, mobile, tag in ((390, 844, True, "phone"), (1280, 900, False, "desktop")):
            self.width(w, h, mobile)
            self.goto("/u/stranger", settle=2.5)
            st = self.state() or {}
            self.check("D0 %s: Bbottin's own dot is on the page with his handle and a picture" % tag,
                       st.get("name") == "Bbottin" and st.get("img"), st)
            self.check("D1 %s: the middle of the site header still hits the header" % tag,
                       (self.hit("header.top") or {}).get("ok"), self.hit("header.top"))
            self.check("D3 %s: positive control, nothing is marked before walking" % tag, not st.get("near"), st)
            before = (self.state() or {}).get("dot")   # walking is always on (2026-11-15): nothing to click first
            self.key("ArrowRight", 0.4)
            time.sleep(0.2)
            after = (self.state() or {}).get("dot")
            self.check("D2 %s: the pad is on the page (walking is always on)" % tag, (self.state() or {}).get("pad"), self.state())
            self.check("D2 %s: an arrow key moves the dot right" % tag,
                       bool(before and after and after[0] > before[0] + 20), (before, after))
            self.shot(tag + "-walk")
            if tag == "phone":
                st = self.walk_to_button()
                self.check("D3 phone: walking up to Send friend request marks it", "friend request" in (st.get("near") or "").lower(), st)
                self.shot("phone-near")
                # A is a quick tap on the round joystick since q64 (no A button): press and let go in place
                pressed = self.j("(() => { const n = document.querySelector('.lg-here-near');"
                                 " const t = document.querySelector('#lg-here-pad .lg-here-touchpad'), r = t.getBoundingClientRect();"
                                 " const o = {pointerId: 7, clientX: r.left + r.width / 2, clientY: r.top + r.height / 2, bubbles: true};"
                                 " t.dispatchEvent(new PointerEvent('pointerdown', o)); t.dispatchEvent(new PointerEvent('pointerup', o));"
                                 " return JSON.stringify(n ? n.textContent.trim() : null); })()")
                landed = None
                for _ in range(30):   # wait for the form's POST and redirect to land (up to 6 s)
                    time.sleep(0.2)
                    landed = self.j("JSON.stringify(location.pathname)")
                    if landed and landed != "/u/stranger":
                        break
                self.check("D3 phone: A pressed the marked button (the page moved on)", landed == "/friends", (pressed, landed))
                self.goto("/friends", settle=1)
                # the Outgoing card on /friends lists sent requests (people you may know is a different card)
                out = self.j("(() => { const h = Array.from(document.querySelectorAll('h2')).find(e => e.textContent.trim() === 'Outgoing');"
                             " return JSON.stringify(h ? h.parentElement.innerText : null); })()") or ""
                self.check("D3 phone: A pressed it: stranger is in Outgoing requests on /friends", "stranger" in out, out[:300])
        # tapping still works: a plain click on the header's brand link goes home
        self.goto("/u/stranger", settle=1.5)
        self.j("(() => { document.querySelector('a.brand').click(); return JSON.stringify(1); })()")
        time.sleep(1.5)
        loc = self.j("JSON.stringify(location.pathname)")
        self.check("D4 a plain click still navigates (left the profile page)", loc and loc != "/u/stranger", loc)


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    r = DotsRun(base, "heredots-" + label, out)
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
        path = os.path.join(out, "phone-heredots-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT heredots-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
