"""The floating Juketube button must not cover page content at narrow widths (Grok Bot's site pass 2026-11-11: at a
~768 px wide window it sat on the /communities search field and on the profile text of /u/<name>; screenshots
narrow-groups-250.webp and narrow-profile-250.webp). Checked in a real Chromium with the REAL site stylesheet (css()'s
string literals, cut from src/http_server.cpp), the real assets/setup.css and assets/juketube.css, and the button's own
markup as juketube.js builds it. NO C++ server: a small stdlib server here serves a page shaped like the two that
showed the fault (a full-width search field in a card, a profile card with a long meta line, long bare text outside any
card, a centered button) inside the real <main class="wrap"> and <footer class="site">. New file; the committed
browser checks are not edited.

Cases, at widths 390 (a phone), 768 (Grok Bot's look), 1000, 1280 and 1600:
  J1  the page is scrolled top to bottom in steps; at no step does the button's box intersect a line of text, a form
      field, a button or a link chip on the page (content may pass under it only where there is none: the margins)
  J2  the button stays on screen and stays tappable (at least 44 px tall and 32 px wide)
  J3  the end of the page (the footer's last line) scrolls clear above the button (what phone_check case 1 asks, kept)
Usage: LG_BROWSER=$(command -v google-chrome) python3 juketube_overlap_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT juketube-overlap: ALL-PASS" or the failed cases; writes <out>/juketube-overlap.json and screenshots.
Exit 1 on a FAIL."""
import base64
import http.server
import json
import os
import re
import sys
import threading
import time
import traceback
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
if os.name != "nt" and not os.environ.get("LG_BROWSER"):
    for _c in ("/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser"):
        if os.path.exists(_c):
            os.environ["LG_BROWSER"] = _c
            break
import cdp  # noqa: E402

WIDTHS = [390, 768, 1000, 1280, 1600]
HEIGHT = 800

LONG = ("Roseville, CA &middot; from Roseville, CA &middot; Artist, Inventor, Web Designer &middot; Northwest Lineman "
        "College &middot; and a line long enough to reach the right edge of any column it is put in, on any width ")


def site_css(platform):
    """css()'s string literals, in order: the stylesheet every page inlines."""
    src = __import__("server_source").read(platform)  # whole server source (http_server.cpp + its src/site pieces)
    a = src.index("std::string css() {")
    b = src.index("\n}\n", a)
    body = "\n".join(l for l in src[a:b].splitlines() if not l.strip().startswith("//"))
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
    return "".join(p.encode().decode("unicode_escape") for p in parts)


def page(platform):
    # the button as juketube.js builds it (assets/juketube.js: '<button id="jbtoggle" ...>' + ICON)
    toggle = ('<div id="juketube-root"><button id="jbtoggle" type="button" aria-label="Juketube" aria-expanded="false">'
              '<svg viewBox="0 0 24 24"><rect x="4" y="2" width="16" height="20" rx="3" fill="currentColor"/></svg>'
              '</button></div>')
    main = (
        '<p class="row"><a class="btn ghost" href="#">Turn on alerts</a></p>'
        '<p style="text-align:center"><a class="btn" href="#">Edit profile</a></p>'
        '<p class="bare">' + LONG * 2 + '</p>'
        '<div class="card"><h2>Cities</h2><p class="muted">Groups that name the same city are listed together, so people '
        'there can find each other. <a href="#">See them on the MAINstream map</a>.</p>'
        '<p><a class="chip" href="#">Roseville &middot; 1</a></p></div>'
        '<div class="card"><h2>Find a group</h2>'
        '<input id="commfilter" type="search" placeholder="Search by name or address">'
        '<details open><summary>Policy &amp; Government &middot; 2</summary><p>' + LONG + '</p></details></div>'
        '<div class="card"><h1>Bbottin</h1><p class="muted">' + LONG + '</p><p>I made this website, I hope it\'s '
        'received well. ' + LONG + '</p><p class="row"><button type="button">Add friend</button>'
        '<button type="button" class="ghost">Message</button></p></div>'
    ) + ''.join('<div class="card"><h2>Section %d</h2><p>%s</p><input type="text" placeholder="a field"></div>'
                % (i, LONG) for i in range(4))
    footer = ('<footer class="site"><a href="#">Genesis</a> &middot; <a href="#">Laws</a> &middot; '
              '<a href="#">Privacy</a><br>Not a government. A platform account is not a citizen identity. '
              'LiquidGov.US is in Beta &mdash; some of what\'s here is still being built.</footer>')
    return ('<!doctype html><html lang="en"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">'
            '<style>%s</style><link rel="stylesheet" href="/assets/setup.css">'
            '<link rel="stylesheet" href="/assets/juketube.css"></head><body>'
            '<header class="top"><div class="topinner"><a class="brand" href="#">LiquidGov</a></div></header>'
            '<main class="wrap">%s</main>%s%s</body></html>' % (site_css(platform), main, footer, toggle))


def make_handler(platform):
    html = page(platform).encode()

    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def send(self, code, ctype, body):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            u = urllib.parse.urlsplit(self.path)
            if u.path in ("/assets/juketube.css", "/assets/setup.css"):
                with open(os.path.join(platform, "assets", os.path.basename(u.path)), "rb") as f:
                    return self.send(200, "text/css", f.read())
            if u.path == "/page":
                return self.send(200, "text/html; charset=utf-8", html)
            return self.send(404, "text/plain", b"no")

    return H


# Everything on the page a person reads or taps, on screen: each line of text (a Range's client rects), each field,
# button and chip. Then the ones the Juketube button's box intersects (1 px of slack each way).
HELPERS = r"""
window.__j = {
  toggle() { const r = document.getElementById('jbtoggle').getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom}; },
  content() {
    const out = [];
    for (const root of [document.querySelector('main'), document.querySelector('footer.site')]) {
      const w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
      for (let n = w.nextNode(); n; n = w.nextNode()) {
        if (!n.textContent.trim()) continue;
        const rg = document.createRange(); rg.selectNodeContents(n);
        for (const r of rg.getClientRects()) if (r.width > 0 && r.height > 0)
          out.push({what: n.textContent.trim().slice(0, 30), l: r.left, t: r.top, r: r.right, b: r.bottom});
      }
      for (const e of root.querySelectorAll('input,button,select,textarea,a.btn,a.chip,summary')) {
        const r = e.getBoundingClientRect();
        if (r.width > 0) out.push({what: '<' + e.tagName.toLowerCase() + '> ' + (e.placeholder || e.textContent.trim()).slice(0, 24), l: r.left, t: r.top, r: r.right, b: r.bottom});
      }
    }
    return out;
  },
  hits() {
    const t = this.toggle();
    return this.content().filter(c => c.l < t.r - 1 && c.r > t.l + 1 && c.t < t.b - 1 && c.b > t.t + 1)
      .map(c => c.what + ' @' + Math.round(c.l) + '-' + Math.round(c.r));
  },
  lastText() {
    const f = document.querySelector('footer.site'); const rg = document.createRange(); rg.selectNodeContents(f);
    const rs = [...rg.getClientRects()].filter(x => x.height > 0); return Math.max(...rs.map(x => x.bottom));
  },
};
1
"""


class Run:
    def __init__(self, base, out):
        self.base, self.out = base, out
        self.rep = {"checks": {}}
        self.fails = []
        self.ws = None

    def check(self, name, ok, detail):
        self.rep["checks"][name] = {"ok": bool(ok), "detail": detail}
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:600]), flush=True)
        if not ok:
            self.fails.append(name)

    def j(self, expr):
        v = cdp.js(self.ws, "JSON.stringify(%s)" % expr)
        try:
            return json.loads(v) if isinstance(v, str) else v
        except ValueError:  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def shot(self, name):
        with open(os.path.join(self.out, "juketube-overlap-%s.png" % name), "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", format="png")["data"]))

    def width(self, w):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=HEIGHT, deviceScaleFactor=1, mobile=w < 500)
        self.ws.call("Page.navigate", url=self.base + "/page")
        for _ in range(80):
            if cdp.js(self.ws, "document.readyState") == "complete":
                break
            time.sleep(0.25)
        time.sleep(0.6)
        cdp.js(self.ws, HELPERS)

    def cases(self):
        for w in WIDTHS:
            self.width(w)
            total = self.j("document.documentElement.scrollHeight") or 0
            hits, worst_shot, y = {}, None, 0
            while True:
                cdp.js(self.ws, "window.scrollTo(0, %d)" % y)
                time.sleep(0.08)
                h = self.j("__j.hits()")
                if h:
                    hits[y] = h[:6]
                    if worst_shot is None:
                        worst_shot = y
                        self.shot("%d-hit" % w)
                if y >= total - HEIGHT:
                    break
                y = min(y + HEIGHT // 4, max(total - HEIGHT, 0))
            self.check("J1 width %d: the Juketube button covers no text, field or button while scrolling" % w,
                       not hits, {"hits by scroll y": hits})
            t = self.j("__j.toggle()")
            ok = (isinstance(t, dict) and t["l"] >= 0 and t["r"] <= w + 0.5 and t["t"] >= 0 and t["b"] <= HEIGHT
                  and t["b"] - t["t"] >= 44 and t["r"] - t["l"] >= 32)
            self.check("J2 width %d: the button is on screen and tappable (>= 44 tall, >= 32 wide)" % w, ok, t)
            cdp.js(self.ws, "window.scrollTo(0, document.documentElement.scrollHeight)")
            time.sleep(0.15)
            r = self.j("({last: __j.lastText(), toggle: __j.toggle()})")
            ok = isinstance(r, dict) and r.get("toggle") and r["last"] <= r["toggle"]["t"]
            self.check("J3 width %d: the page's last line scrolls clear above the button" % w, ok, r)
            self.shot("%d-end" % w)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = argv[1] if len(argv) > 1 else os.path.join("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."),
                                                     "lg-juketube-overlap")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(platform))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-juketube-overlap-")
        r.rep["browser"] = r.ws.browser
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        r.rep["verdict"] = "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails)
        path = os.path.join(out, "juketube-overlap.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT juketube-overlap: %s  (%d checks; report %s)" % (r.rep["verdict"], len(r.rep["checks"]), path),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
