"""Post drawings are sharp at full width (Brandon 2026-11-11 5:49 PM, Brief 6 item 5): the post canvas
(/feed/canvas, /c/<slug>/canvas) draws on a 1280 x 960 bitmap (4:3), while a brush, shape, smudge
or text box of a given size still looks the same size on screen as on the old 640 x 480 bitmap; the server takes the
bigger PNG, even a worst case (every pixel random), and drawings already stored at 640 x 480 still show full width.

Real headless Chromium against the REAL server (needs the build, like ci_phone_check.py, whose server start/stop it
reuses: phone_serve.sh on port 18140, signed in as Bbottin). A 390 px phone at devicePixelRatio 3, like an iPhone.
  R1  /feed/canvas: all four canvas layers are 1280 x 960
  R2  the bitmap has at least one pixel per device pixel at full width (width >= drawn CSS width x devicePixelRatio)
  R3  a size-8 pen stroke is 8/640 of the drawing's width thick on screen (+-25%), as on the 640 px canvas
  R4  the text tool's default box (a click, no drag) has a 24/640-of-the-width font on screen (+-10%)
  R5  Post: the server takes it (lands on /feed, no err=) and the stored image is 1280 x 960
  R5b worst case: a drawing of random pixels on every layer (largest PNG) posts too, stored 1280 x 960
  R6  a 640 x 480 drawing stored before this change still shows at the post's full width
Usage: LG_BROWSER=$(command -v google-chrome) python3 ci_canvas_resolution_check.py <build dir> <liquidgov-platform dir> <label> <out dir>
Prints "VERDICT canvas-resolution-<label>: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed
checks unchanged (ci_phone_check.py is reused as is, its phone_check.run swapped for run below)."""
import base64
import json
import os
import struct
import sys
import time
import traceback
import urllib.request
import uuid
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ci_phone_check  # noqa: E402
import cdp  # noqa: E402


def png(w, h):
    """A plain w x h PNG (a black diagonal on white), made here so the check needs no image library."""
    rows = b"".join(b"\x00" + b"".join(b"\x00\x00\x00" if x == y * w // h else b"\xff\xff\xff" for x in range(w))
                    for y in range(h))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def post_png(base, session, text, data):
    """POST /feed with a PNG as the composer's media, the way an older canvas post was stored; returns the Location."""
    b = uuid.uuid4().hex
    body = (("--%s\r\nContent-Disposition: form-data; name=\"text\"\r\n\r\n%s\r\n" % (b, text)).encode() +
            ("--%s\r\nContent-Disposition: form-data; name=\"media\"; filename=\"old.png\"\r\n"
             "Content-Type: image/png\r\n\r\n" % b).encode() + data + ("\r\n--%s--\r\n" % b).encode())
    req = urllib.request.Request(base + "/feed", data=body, method="POST", headers={
        "Content-Type": "multipart/form-data; boundary=" + b, "Cookie": "lg_session=" + session})

    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *a, **k):
            return None
    try:
        r = urllib.request.build_opener(NoRedirect).open(req, timeout=20)
        return r.headers.get("Location", "")
    except urllib.error.HTTPError as e:  # err-ok: a 303 arrives here when redirects are not followed
        return e.headers.get("Location", "") if e.code in (301, 302, 303) else "HTTP %d" % e.code


GEOM = r"""(() => { const w = document.getElementById('ccanvaswrap'), r = w.getBoundingClientRect();
  return {layers: [...w.querySelectorAll('canvas')].map(c => [c.width, c.height]), cssW: r.width, cssH: r.height,
          x: r.left, y: r.top, dpr: devicePixelRatio}; })()"""

# the thickest run of drawn pixels down the column at the stroke's middle, across the layers (bitmap px)
THICK = r"""(() => { let best = 0;
  document.querySelectorAll('#ccanvaswrap canvas[id^=clayer]').forEach(c => {
    const x = Math.round(c.width / 2), d = c.getContext('2d').getImageData(x, 0, 1, c.height).data;
    let run = 0; for (let y = 0; y < c.height; y++) { if (d[y * 4 + 3] > 128) { run++; best = Math.max(best, run); } else run = 0; }
  }); return best; })()"""

POSTED = r"""(async () => { const imgs = [...document.querySelectorAll('article.post img')].filter(i => /%s/.test(i.closest('article').textContent));
  if (!imgs.length) return {found: false, url: location.href};
  const i = imgs[0]; if (!i.complete) await new Promise(ok => { i.onload = ok; i.onerror = ok; });
  const a = i.closest('article').getBoundingClientRect(), r = i.getBoundingClientRect();
  return {found: true, url: location.href, nw: i.naturalWidth, nh: i.naturalHeight, w: r.width, articleW: a.width}; })()"""


class Run:
    def __init__(self, base, label, out):
        self.base, self.label, self.out, self.fails, self.n, self.ws = base, label, out, [], 0, None

    def check(self, name, ok, detail):
        self.n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:500]), flush=True)
        if not ok:
            self.fails.append(name)

    def j(self, expr):
        v = cdp.js(self.ws, "Promise.resolve(%s).then(v => JSON.stringify(v))" % expr)
        try:
            return json.loads(v)
        except (TypeError, ValueError):  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def until(self, what, expr, limit=15.0):
        end = time.time() + limit
        while time.time() < end:
            if cdp.js(self.ws, expr) is True:
                return True
            time.sleep(0.05)
        print("  waited %.0f s for %s" % (limit, what), flush=True)
        return False

    def open_canvas(self):
        self.ws.call("Page.navigate", url=self.base + "/feed/canvas")
        return self.until("the canvas page", "document.readyState==='complete' && !!document.getElementById('coverlay')"
                                             " && !!document.getElementById('csave')")

    def mouse(self, kind, x, y):
        self.ws.call("Input.dispatchMouseEvent", type=kind, x=x, y=y, button="left", buttons=1 if kind != "mouseReleased" else 0,
                     clickCount=1)

    def shot(self, name):
        with open(os.path.join(self.out, "canvas-res-%s-%s.png" % (self.label, name)), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))

    def post_and_find(self, caption):
        cdp.js(self.ws, "document.getElementById('cpcaption').value=%s, 1" % json.dumps(caption))
        cdp.js(self.ws, "window.__old = 1; document.getElementById('csave').click(), 1")
        if not self.until("the feed after Post", "!window.__old && document.readyState==='complete'"
                                                 " && location.pathname==='/feed'", 30):
            return {"found": False, "url": cdp.js(self.ws, "location.href"), "status": cdp.js(
                self.ws, "(document.getElementById('cstatus')||{}).textContent")}
        return self.j(POSTED % caption)

    def cases(self, session):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=390, height=844, deviceScaleFactor=3, mobile=True)
        self.ws.call("Network.setCookie", name="lg_session", value=session, domain=self.base.split("//")[1].split(":")[0], path="/")
        if not self.open_canvas():
            self.check("the canvas page loads", False, cdp.js(self.ws, "location.href"))
            return
        g = self.j(GEOM)
        self.check("R1 all four canvas layers are 1280 x 960", g.get("layers") == [[1280, 960]] * 4, g)
        bw = (g.get("layers") or [[0, 0]])[0][0]
        self.check("R2 at least one bitmap pixel per device pixel at full width",
                   bw >= g.get("cssW", 1e9) * g.get("dpr", 1) - 1, {"bitmap": bw, "cssW": g.get("cssW"), "dpr": g.get("dpr")})
        cdp.js(self.ws, "document.querySelector('[data-tool=pen]') && document.querySelector('[data-tool=pen]').click(), 1")
        cdp.js(self.ws, "document.getElementById('csize').value='8', 1")
        y = g["y"] + g["cssH"] / 2
        self.mouse("mousePressed", g["x"] + g["cssW"] * 0.2, y)
        for k in range(1, 11):
            self.mouse("mouseMoved", g["x"] + g["cssW"] * (0.2 + 0.06 * k), y)
        self.mouse("mouseReleased", g["x"] + g["cssW"] * 0.8, y)
        thick = self.j(THICK)
        on_screen = thick * g["cssW"] / bw if isinstance(thick, (int, float)) and bw else None
        want = 8 * g["cssW"] / 640
        self.check("R3 a size-8 stroke is as thick on screen as on the 640 px canvas (+-25%)",
                   on_screen is not None and abs(on_screen - want) <= want * 0.25,
                   {"bitmap_px": thick, "on_screen": on_screen, "want": want})
        cdp.js(self.ws, "document.querySelector('[data-tool=text]').click(), 1")
        g = self.j(GEOM)  # the text tool opens its own row of controls above, so the drawing moves down: measure again
        tx, ty = g["x"] + g["cssW"] * 0.1, g["y"] + g["cssH"] * 0.1
        under = cdp.js(self.ws, "(() => { const e = document.elementFromPoint(%f, %f); return e ? e.tagName + '#' + e.id + "
                                "' in #' + ((e.closest('[id]') || {}).id || '') : null; })()" % (tx, ty))
        self.mouse("mousePressed", tx, ty)
        self.mouse("mouseReleased", tx, ty)
        self.until("the text box", "!!document.querySelector('#ccanvaswrap textarea')", 5)
        fs = cdp.js(self.ws, "(() => { const t = document.querySelector('#ccanvaswrap textarea');"
                             " return t ? parseFloat(t.style.fontSize) : null; })()")
        want = 24 * g["cssW"] / 640
        self.check("R4 the text tool's default font is 24/640 of the width on screen (+-10%)",
                   isinstance(fs, (int, float)) and abs(fs - want) <= want * 0.1, {"font_css_px": fs, "want": want, "under_click": under,
                                                                                    "tool": cdp.js(self.ws, "document.querySelector('[data-tool=text]').style.background")})
        cdp.js(self.ws, "(() => { const t = document.querySelector('#ccanvaswrap textarea'); if (t) { t.value = '';"
                        " t.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true})); t.blur(); } return 1; })()")
        self.shot("drawn")
        p = self.post_and_find("res check plain")
        self.check("R5 Post lands on /feed and the stored image is 1280 x 960",
                   p.get("found") and "err=" not in p.get("url", "") and [p.get("nw"), p.get("nh")] == [1280, 960], p)
        if not self.open_canvas():
            self.check("the canvas page loads again", False, cdp.js(self.ws, "location.href"))
            return
        cdp.js(self.ws, """(() => { document.querySelectorAll('#ccanvaswrap canvas[id^=clayer]').forEach(c => {
            const x = c.getContext('2d'), d = x.createImageData(c.width, c.height);
            for (let i = 0; i < d.data.length; i += 4) { d.data[i] = Math.random() * 256; d.data[i + 1] = Math.random() * 256;
              d.data[i + 2] = Math.random() * 256; d.data[i + 3] = 255; }
            x.putImageData(d, 0, 0); }); return 1; })()""")
        p = self.post_and_find("res check noise")
        self.check("R5b a worst-case drawing (random pixels) posts too, stored 1280 x 960",
                   p.get("found") and "err=" not in p.get("url", "") and [p.get("nw"), p.get("nh")] == [1280, 960], p)
        loc = post_png(self.base, session, "res check old 640", png(640, 480))
        self.ws.call("Page.navigate", url=self.base + "/feed")
        self.until("the feed", "document.readyState==='complete'")
        p = self.j(POSTED % "res check old 640")
        self.check("R6 a 640 x 480 drawing stored the old way still shows at the post's full width",
                   p.get("found") and [p.get("nw"), p.get("nh")] == [640, 480] and p.get("w", 0) >= p.get("articleW", 1e9) * 0.8,
                   {"post": loc, "img": p})
        self.shot("feed")


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (ci_phone_check calls it); expected_js is not used here."""
    os.makedirs(out, exist_ok=True)
    r = Run(base, label, out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-canvas-res-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases(session)
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        print("VERDICT canvas-resolution-%s: %s  (%d checks)" % (
            label, "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n), flush=True)
    return r.fails


if __name__ == "__main__":
    ci_phone_check.phone_check.run = run
    sys.exit(ci_phone_check.main(sys.argv[1:]))
