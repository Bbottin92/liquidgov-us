"""The style library's previews can't touch the page or each other (Brandon 2026-11-12, profile CSS slice 2: "the preview
must not let one style affect the page or the other previews (no style escapes its own area)"). Each preview is a
sandboxed iframe (no scripts, no shared origin), so the browser keeps every style in its own document. A deliberately
hostile style (a red full-screen fixed box with the top z-index, red html/body) is published next to a plain one; the
page's header and buttons and the plain preview must be untouched. New file: no committed check is edited.

Cases (Bbottin signed in, phone_serve.sh sample data), at phone width (390) and desktop width (1280):
  P0 precondition: both styles published (each publish lands on /styles), and opened on its own the hostile preview
     really paints its red fixed box (positive control: a pass below is not a style that never loaded)
  P1 the point in the middle of the site header still hits the header
  P2 a header link is still hit (clickable)
  P3 every "Use this style" button is hit (none covered)
  P4 the plain preview's pixels have no red (the hostile style did not reach it); positive control: the hostile
     preview's own pixels ARE red
  P5 the page outside the previews has no red pixels in the header strip
Writes <out>/phone-stylespreview-<label>.json and <out>/phone-stylespreview-<label>-*.png (look at them).
"""
import base64
import json
import os
import struct
import traceback
import urllib.parse
import zlib

import cdp
import phone_check

RED = "rgb(255, 0, 0)"
HOSTILE = (".lg-theme::before{content:\"FAKE LOGIN\";position:fixed;top:0;left:0;right:0;bottom:0;"
           "z-index:2147483647;background:#f00} html,body{background:#f00} header{display:none}")
PLAIN = ".lg-theme h1{color:#2b5d8a}"


def png_pixels(data):
    """(width, height, rows of RGB tuples) from an 8-bit RGB/RGBA PNG (what Chromium writes); no PIL needed."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    i, idat, w = 8, b"", 0
    while i < len(data):
        n, kind = struct.unpack(">I4s", data[i:i + 8])
        chunk = data[i + 8:i + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", chunk[:10])
            assert depth == 8 and ctype in (2, 6), (depth, ctype)
            bpp = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += chunk
        i += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x:x + 3]) for x in range(0, stride, bpp)])
        prev = line
    return w, h, rows


def red_share(rows):
    px = [p for row in rows for p in row]
    red = sum(1 for r, g, b in px if r > 220 and g < 40 and b < 40)
    return red / max(1, len(px))


class PreviewRun(phone_check.Run):
    def hit(self, selector, index=0):
        return self.j("(() => { const all = document.querySelectorAll(%s); const e = all[%d]; if (!e) return JSON.stringify(null);"
                      " e.scrollIntoView({block: 'center'});"
                      " const r = e.getBoundingClientRect(); if (!r.width || !r.height) return JSON.stringify({zero: true});"
                      " const x = r.left + r.width / 2, y = r.top + r.height / 2;"
                      " const h = document.elementFromPoint(x, y);"
                      " return JSON.stringify({x, y, n: all.length, hit: h ? h.tagName + '.' + h.className : null, ok: !!h && (h === e || e.contains(h))}); })()"
                      % (json.dumps(selector), index))

    def post(self, path, fields):
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v, safe="")) for k, v in fields.items())
        return self.j("fetch(%s, {method: 'POST', credentials: 'same-origin', redirect: 'manual',"
                      " headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: %s})"
                      ".then(r => JSON.stringify({status: r.status, type: r.type}))" % (json.dumps(path), json.dumps(body)))

    def width(self, w, h, mobile):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=2 if mobile else 1, mobile=mobile)

    def clip_pixels(self, selector):
        """The rendered pixels of the element's box (scrolled into view first)."""
        r = self.j("(() => { const e = document.querySelector(%s); if (!e) return JSON.stringify(null); e.scrollIntoView({block: 'center'});"
                   " const b = e.getBoundingClientRect(); return JSON.stringify({x: b.left + scrollX, y: b.top + scrollY, w: b.width, h: b.height}); })()"
                   % json.dumps(selector))
        if not isinstance(r, dict) or not r.get("w"):
            return None
        shot = self.ws.call("Page.captureScreenshot", format="png",
                            clip={"x": r["x"] + 2, "y": r["y"] + 2, "width": max(1, r["w"] - 4), "height": max(1, r["h"] - 4), "scale": 1})
        return png_pixels(base64.b64decode(shot["data"]))[2]

    def ids(self):
        return self.j("JSON.stringify(Array.from(document.querySelectorAll('[data-style-name]')).map(e => [e.getAttribute('data-style-name'), e.id]))")

    def cases(self):
        self.goto("/me", settle=1)
        published = []
        for name, css in (("Hostile box", HOSTILE), ("Plain blue", PLAIN)):
            self.post("/me/css", {"css": css})
            published.append(self.post("/styles/publish", {"name": name}))
        self.post("/me/css", {"css": ""})
        self.check("P0 both styles published", all(isinstance(p, dict) and p.get("status") in (0, 302, 303) for p in published), published)
        self.goto("/styles", settle=1)
        cards = dict(self.ids() or [])
        self.check("P0 /styles lists both styles", "Hostile box" in cards and "Plain blue" in cards, cards)
        hostile, plain = "#" + cards.get("Hostile box", "none"), "#" + cards.get("Plain blue", "none")
        for w, h, mobile, tag in ((390, 844, True, "phone"), (1280, 800, False, "desktop")):
            self.width(w, h, mobile)
            self.goto("/styles", settle=2)
            self.check("P1 %s: the middle of the site header hits the header" % tag,
                       (self.hit("header.top") or {}).get("ok"), self.hit("header.top"))
            self.check("P2 %s: a header link is still hit (clickable)" % tag,
                       (self.hit("header.top a") or {}).get("ok"), self.hit("header.top a"))
            uses = self.hit("form[action='/styles/use'] button") or {}
            n = uses.get("n", 0)
            self.check("P3 %s: every Use button is hit (%d)" % (tag, n),
                       n >= 2 and all((self.hit("form[action='/styles/use'] button", k) or {}).get("ok") for k in range(n)), uses)
            hp = self.clip_pixels(hostile + " iframe")
            pp = self.clip_pixels(plain + " iframe")
            self.check("P4 %s: positive control, the hostile preview is red inside its own frame" % tag,
                       hp is not None and red_share(hp) > 0.5, hp and red_share(hp))
            self.check("P4 %s: the plain preview has no red" % tag, pp is not None and red_share(pp) < 0.001, pp and red_share(pp))
            self.goto("/styles", settle=1)
            hdr = self.clip_pixels("header.top")
            self.check("P5 %s: the site header has no red" % tag, hdr is not None and red_share(hdr) < 0.001, hdr and red_share(hdr))
            self.shot(tag)
        # the positive control for the style itself: opened on its own (top level) it paints the red fixed box
        self.goto("/styles/%s/preview" % cards.get("Hostile box", "none").replace("style-", ""), settle=1)
        st = self.j("(() => { const t = document.querySelector('.lg-theme'); if (!t) return JSON.stringify(null);"
                    " const b = getComputedStyle(t, '::before'); return JSON.stringify({pos: b.position, bg: b.backgroundColor}); })()")
        self.check("P0 the hostile preview on its own applies its style (fixed, red)",
                   isinstance(st, dict) and st.get("pos") == "fixed" and st.get("bg") == RED, st)


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    r = PreviewRun(base, "stylespreview-" + label, out)
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
        path = os.path.join(out, "phone-stylespreview-%s.json" % label)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT stylespreview-%s: %s  (%d checks; report %s)" % (label, r.rep["verdict"], len(r.rep["checks"]), path), flush=True)
    return r.fails
