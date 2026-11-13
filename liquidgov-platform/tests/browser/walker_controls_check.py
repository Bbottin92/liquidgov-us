"""Walker controls (Brandon 2026-11-13, 12:28 AM: "the desktop would be wasd controls and enter to select and esc to go
back. also the button that turns dripping on should be at the top of the screen"; 12:30 AM: "d pad lower left, A and B
buttons lower right. right now theyre all scrunched in the lower left"). New file: no committed check is edited.

Real headless Chromium, NO C++ server (like site_hidden_check.py): a stdlib server serves a profile-like page (site
header, a button, a text field, the Juketube button, the footer) with the real assets/site.css, assets/juketube.css and
assets/here.js, and a stand-in for the dots WebSocket that echoes your own dot back. Every key and button name comes
from here.js's one controls map (window.LG_HERE_CONTROLS); this file keeps no copy of the bindings.

  K0  precondition: the controls map is on the page and binds every direction, select and back to keys and buttons
  Phone 390x844 (walking):
  P1  the D-pad's center is in the left half and the bottom quarter of the screen
  P2  A and B are each in the right half and the bottom quarter
  P3  no two controls overlap (D-pad, A, B, the Walk toggle, the Juketube button)
  P4  every control is at least 44x44 px
  P5  the Walk toggle ("dripping") is in the top 15% of the screen and does not cover the site header's middle
  Desktop 1280x800:
  D1  the Walk toggle is in the top 15% of the screen and does not cover the site header's middle
  D2  each movement key in the map (W/A/S/D and the arrows) moves your dot its way
  D3  select (Enter) uses the button you stand next to (positive control: nothing was used before)
  D4  keys typed in a text field stay in the field (the dot does not move); back (Esc) there leaves the field
  D5  back (Esc) goes back a page
Usage: LG_BROWSER=$(command -v google-chrome) python3 walker_controls_check.py [<liquidgov-platform dir> [<out dir>]]
Writes <out dir>/phone-walker-controls-*.png. Prints "VERDICT walker-controls: ALL-PASS" or the failed cases.
Exit 1 on a FAIL."""
import base64
import http.server
import json
import os
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import me_autosave_check as m  # noqa: E402  (sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

# Stand-in for /here/ws: you appear where the page says (next to #use), then every position you send is echoed back.
FAKE_WS = r"""<script>
window.LG_USED = 0;
window.WebSocket = function () {
  var ws = this; ws.readyState = 1;
  function tell(x, y) { if (ws.onmessage) ws.onmessage({data: JSON.stringify({people: [{u: 'Bbottin', x: x, y: y}]})}); }
  setTimeout(function () {
    var b = document.getElementById('use').getBoundingClientRect();
    var W = Math.max(document.documentElement.scrollWidth, document.body.scrollWidth);
    var H = Math.max(document.documentElement.scrollHeight, document.body.scrollHeight);
    if (ws.onopen) ws.onopen();
    tell((b.left + scrollX - 20) / W, (b.top + scrollY + b.height / 2) / H);
  }, 50);
  ws.send = function (s) { var p = JSON.parse(s); tell(p.x, p.y); };
};
window.LG_HERE = {me: 'Bbottin', page: '/u/stranger'};
</script>"""


def page(title):
    return ("<!doctype html><html lang=en><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'><title>%s</title>"
            "<link rel=stylesheet href=/assets/site.css><link rel=stylesheet href=/assets/juketube.css></head><body>"
            "<header class=top><div class=topinner><a class=brand href=/start><div><div class=word><strong>LiquidGov</strong>"
            "<span class=tld>.US</span></div><div class=tagline>Civic association</div></div></a>"
            "<label class=navbtn>Menu</label></div></header>"
            "<main class=wrap><h1>stranger</h1><p>A profile page.</p>"
            "<p style='margin:3rem 0 3rem 200px'><button type=button id=use onclick='window.LG_USED++'>Send friend request</button></p>"
            "<p><input id=field type=text aria-label=Note></p><div style='height:900px'></div></main>"
            "<footer class=site>Not a government.</footer><button id=jbtoggle aria-label=Juketube></button>"
            "%s<script src=/assets/here.js></script></body></html>" % (title, FAKE_WS))


def make_handler(assets):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            path = self.path.split("?")[0]
            if path.startswith("/assets/"):
                f = os.path.join(assets, os.path.basename(path))
                if not os.path.isfile(f):
                    self.send_response(404)
                    self.end_headers()
                    return
                body = open(f, "rb").read()
                ctype = "text/css" if f.endswith(".css") else "text/javascript"
            else:
                body = page("walk" if path == "/walk" else "start").encode()
                ctype = "text/html"
            self.send_response(200)
            self.send_header("Content-Type", ctype + "; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

    return H


# The controls' boxes, named by the map in here.js (no copy of the class names here).
BOXES = r"""(() => { const C = window.LG_HERE_CONTROLS, q = s => document.querySelector(s), out = {vw: innerWidth, vh: innerHeight};
  if (!C) return JSON.stringify({noMap: true});
  const box = e => { if (!e) return null; const r = e.getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom, w: r.width, h: r.height}; };
  out.dpad = box(q('#lg-here-pad .lg-here-dpad'));
  out.buttons = {}; Object.keys(C.buttons).forEach(k => { out.buttons[k] = box(q('#lg-here-pad .' + C.buttons[k].cls)); });
  out.jb = box(q('#jbtoggle'));
  const h = q('header.top').getBoundingClientRect(), hit = document.elementFromPoint(h.left + h.width / 2, h.top + h.height / 2);
  out.headerHit = !!hit && q('header.top').contains(hit);
  return JSON.stringify(out); })()"""
DOT = ("(() => { const d = document.querySelector('#lg-here .lg-here-dot.me'); if (!d) return JSON.stringify(null);"
       " const r = d.getBoundingClientRect(); return JSON.stringify([r.left + scrollX, r.top + scrollY]); })()")
WAY = {"up": (0, -1), "down": (0, 1), "left": (-1, 0), "right": (1, 0)}
VK = {"arrowup": 38, "arrowdown": 40, "arrowleft": 37, "arrowright": 39, "enter": 13, "escape": 27}
DOM_KEY = {"arrowup": "ArrowUp", "arrowdown": "ArrowDown", "arrowleft": "ArrowLeft", "arrowright": "ArrowRight",
           "enter": "Enter", "escape": "Escape"}


def overlaps(a, b):
    return a and b and a["l"] < b["r"] and b["l"] < a["r"] and a["t"] < b["b"] and b["t"] < a["b"]


class Run(m.Run):
    def __init__(self, base, out):
        super().__init__(base)
        self.out = out

    def j(self, expr):
        """expr evaluates to a JSON string (or a promise of one); returns it decoded."""
        v = cdp.js(self.ws, expr)
        try:
            return json.loads(v) if isinstance(v, str) else v
        except ValueError:  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def shot(self, name, clip=None):
        args = {"format": "png"}
        if clip:
            args["clip"] = dict(clip, scale=1)
        path = os.path.join(self.out, "phone-walker-controls-%s.png" % name)
        with open(path, "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", **args)["data"]))
        print("SHOT " + path, flush=True)

    def size(self, w, h, mobile):
        self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=2 if mobile else 1, mobile=mobile)

    def goto(self, path):
        self.ws.call("Page.navigate", url=self.base + path)
        self.wait("document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")

    def wait(self, cond, limit=10):
        end = time.time() + limit
        while time.time() < end:
            if self.j("JSON.stringify(!!(%s))" % cond) is True:
                return True
            time.sleep(0.05)
        return False

    def key(self, k, hold_s=0.05, text=False):
        """k is a key as the map names it (lowercase KeyboardEvent.key)."""
        dom = DOM_KEY.get(k, k)
        vk = VK.get(k, ord(k.upper()) if len(k) == 1 else 0)
        extra = {"text": k} if text and len(k) == 1 else {}
        self.ws.call("Input.dispatchKeyEvent", type="keyDown", key=dom, windowsVirtualKeyCode=vk, **extra)
        time.sleep(hold_s)
        self.ws.call("Input.dispatchKeyEvent", type="keyUp", key=dom, windowsVirtualKeyCode=vk)

    def toggle(self):
        self.j("(() => { document.querySelector('#lg-here-pad .' + LG_HERE_CONTROLS.buttons.toggle.cls).click(); return '1'; })()")
        self.wait("!document.querySelector('#lg-here-pad .lg-here-keys').hidden", 3)

    def toggle_at_top(self, tag, b):
        t = (b.get("buttons") or {}).get("toggle")
        self.check("%s the Walk toggle is in the top 15%% of the screen and the header's middle is uncovered" % tag,
                   bool(t) and t["b"] <= 0.15 * b["vh"] and t["t"] >= 0 and b.get("headerHit"), {"toggle": t, "vh": b.get("vh"),
                                                                                                   "headerHit": b.get("headerHit")})

    def phone(self):
        self.size(390, 844, True)
        self.goto("/walk")
        self.toggle()
        time.sleep(0.3)
        b = self.j(BOXES) or {}
        C = self.j("JSON.stringify(window.LG_HERE_CONTROLS || null)") or {}
        acts = set((C.get("keys") or {}).values())
        self.check("K0 the controls map binds every direction, select and back to keys and buttons",
                   bool(C) and set(C.get("dirs", [])) == set(WAY) and set(WAY) | {"select", "back"} <= acts
                   and all(a in C.get("buttons", {}) for a in list(WAY) + ["select", "back", "toggle"]), C)
        vw, vh, btn = b.get("vw", 0), b.get("vh", 0), b.get("buttons") or {}
        dp = b.get("dpad")
        self.check("P1 phone: the D-pad's center is in the left half and the bottom quarter",
                   bool(dp) and (dp["l"] + dp["r"]) / 2 < vw / 2 and (dp["t"] + dp["b"]) / 2 > 0.75 * vh, {"dpad": dp, "vw": vw, "vh": vh})
        for act in ("select", "back"):
            r = btn.get(act)
            self.check("P2 phone: %s (%s) is in the right half and the bottom quarter" % (act, (C.get("buttons", {}).get(act) or {}).get("text")),
                       bool(r) and (r["l"] + r["r"]) / 2 > vw / 2 and (r["t"] + r["b"]) / 2 > 0.75 * vh, r)
        parts = {"dpad": dp, "select": btn.get("select"), "back": btn.get("back"), "toggle": btn.get("toggle"), "juketube": b.get("jb")}
        names = list(parts)
        clash = [(x, y) for i, x in enumerate(names) for y in names[i + 1:] if overlaps(parts[x], parts[y])]
        self.check("P3 phone: no two controls overlap (D-pad, A, B, Walk toggle, Juketube button)",
                   all(parts.values()) and not clash, {"overlaps": clash, "boxes": parts})
        small = {k: (r["w"], r["h"]) for k, r in btn.items() if r and (r["w"] < 44 or r["h"] < 44)}
        self.check("P4 phone: every control is at least 44x44 px", len(btn) >= 7 and all(btn.values()) and not small,
                   {"small": small, "count": len(btn)})
        self.toggle_at_top("P5 phone:", b)
        self.shot("phone", {"x": 0, "y": 0, "width": vw, "height": vh})

    def desktop(self):
        self.size(1280, 800, False)
        self.goto("/start")
        self.j("(() => { location.href = '/walk'; return '1'; })()")
        time.sleep(0.3)
        self.wait("location.pathname === '/walk' && document.readyState === 'complete' && !!document.querySelector('#lg-here .lg-here-dot.me')")
        self.toggle()
        b = self.j(BOXES) or {}
        self.toggle_at_top("D1 desktop:", b)
        C = self.j("JSON.stringify(window.LG_HERE_CONTROLS || null)") or {}
        for k, act in sorted((C.get("keys") or {}).items()):
            if act not in WAY:
                continue
            before = self.j(DOT)
            self.key(k, 0.3)
            time.sleep(0.15)
            after = self.j(DOT)
            dx, dy = WAY[act]
            moved = bool(before and after) and ((after[0] - before[0]) * dx + (after[1] - before[1]) * dy) > 10
            self.check("D2 desktop: key %r moves the dot %s" % (k, act), moved, (before, after))
            back_k = sorted(kk for kk, a in C["keys"].items() if a == {"up": "down", "down": "up", "left": "right", "right": "left"}[act])[0]
            self.key(back_k, 0.3)   # walk back, so the dot stays next to the button
            time.sleep(0.15)
        self.shot("desktop", {"x": 0, "y": 0, "width": 1280, "height": 800})
        sel = [kk for kk, a in C.get("keys", {}).items() if a == "select"]
        near = self.j("JSON.stringify(!!document.querySelector('#use.lg-here-near'))")
        used0 = self.j("JSON.stringify(window.LG_USED)")
        self.check("D3 desktop: positive control, you stand next to the button and nothing was used yet", near is True and used0 == 0,
                   {"near": near, "used": used0})
        for kk in sel:
            self.key(kk)
        time.sleep(0.2)
        used = self.j("JSON.stringify(window.LG_USED)")
        self.check("D3 desktop: select (%s) uses the button you stand next to" % "/".join(sel), bool(sel) and used == len(sel), used)
        self.j("(() => { document.getElementById('field').focus(); return '1'; })()")
        before = self.j(DOT)
        typed = [kk for kk, a in C.get("keys", {}).items() if a in WAY and len(kk) == 1]
        for kk in typed:
            self.key(kk, 0.15, text=True)
        time.sleep(0.15)
        after = self.j(DOT)
        val = self.j("JSON.stringify(document.getElementById('field').value)")
        self.check("D4 desktop: keys typed in a text field stay there (the dot does not move)",
                   bool(typed) and before == after and val == "".join(typed), {"dot": (before, after), "value": val, "typed": typed})
        back = [kk for kk, a in C.get("keys", {}).items() if a == "back"]
        for kk in back:
            self.key(kk)
        time.sleep(0.1)
        self.check("D4 desktop: back (%s) in a text field leaves the field" % "/".join(back),
                   bool(back) and self.j("JSON.stringify(document.activeElement && document.activeElement.id)") != "field",
                   self.j("JSON.stringify(document.activeElement && document.activeElement.id)"))
        path0 = self.j("JSON.stringify(location.pathname)")
        for kk in back[:1]:
            self.key(kk)
        self.wait("location.pathname === '/start'", 5)
        path1 = self.j("JSON.stringify(location.pathname)")
        self.check("D5 desktop: back (%s) goes back a page" % "/".join(back[:1]), path0 == "/walk" and path1 == "/start", (path0, path1))


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(os.path.join(platform, "assets")))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-walker-controls-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.phone()
        r.desktop()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT walker-controls: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
