"""The landing intro plays on every visit, says once it can be turned off, and its Settings toggle (Brandon 2026-11-13
5:04 AM: "make the intro video play every time you visit liquidgov.us ... on the first visit only, make it obvious it
can be turned off in Settings ... a Settings toggle 'Play intro video on each visit' (default on) ... doesn't collide
with the homepage Drop tip/pulse"). New file (report-only, like walker_controls_check.py, whose harness it reuses).

The markup is the real one: intro_overlay_html and intro_setting_card's literals from src/site/ui/shell.cpp (the
pref name from shell.hpp's kIntroPref), on the real home page markup (genesis.cpp, as walker_controls_check builds it),
with the real assets (intro.js, tip.js, walker.js, home_drop.js, site.css). The video is held playing (its play,
paused, ended and error are stubbed) so the overlay stays up while the cases look at it.

Cases (390x844 phone):
  I1  a fresh browser (logged out): the intro is up; its pref name is kIntroPref; the first-visit line reads "Plays
      every visit. Turn off in Settings" with a link to /settings/intro; the line does not meet Skip
  I2  while the intro plays the home page's Drop note (tip.js) waits: not shown under the video; after Skip the intro
      is gone, then the note shows and the Drop/Scroll switch pulses (one at a time)
  I3  the next visit: the intro plays again, without the first-visit line
  I4  /settings/intro logged out: 'Play intro video on each visit' is on by default; turning it off keeps '0' in
      localStorage 'liquidgov.<kIntroPref>' and the next visit has no intro (gone before the page shows); on again
      brings it back
  I5  signed in (data-mode="account": the server already checked the account's pref): this browser's '0' does not
      stop it
  I6  every screen (Brandon 5:46 AM): at 390x844, 844x390, 820x1180, 1280x800 and 2560x1080 the intro fills the
      viewport, the clip is contained (object-fit: contain, never stretched or cropped: its drawn picture lies inside
      the viewport at the clip's own aspect ratio), and Skip and the first-visit line are on screen, apart, and on top
      (a tap at their middle lands on them); one screenshot per size
Usage: LG_BROWSER=... python3 intro_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT intro: ALL-PASS" or the failed cases; screenshots in <out>. Exit 1 on a FAIL."""
import base64
import http.server
import os
import re
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import walker_controls_check as w  # noqa: E402  (its harness: Run, the home page, the asset server)
import cdp  # noqa: E402

# the video held "playing": no real playback, no end, no error, so the overlay stays until Skip
HOLD = r"""(() => {
  Object.defineProperty(HTMLMediaElement.prototype, 'paused', {get() { return false; }});
  HTMLMediaElement.prototype.play = function () { return new Promise(() => {}); };
  const add = EventTarget.prototype.addEventListener;
  EventTarget.prototype.addEventListener = function (t, f, o) {
    if (this instanceof HTMLMediaElement && (t === 'ended' || t === 'error')) return;
    return add.call(this, t, f, o);
  };
  window.LG_INTRO_AT_LOAD = null;
  document.addEventListener('DOMContentLoaded', () => { window.LG_INTRO_AT_LOAD = !!document.getElementById('lgintro'); });
})();"""


def intro_pieces(platform):
    """(overlay markup for 'browser', for 'account', the logged-out Settings card, the pref name) from shell.cpp/hpp."""
    ui = os.path.join(platform, "src", "site", "ui")
    hpp = open(os.path.join(ui, "shell.hpp"), encoding="utf-8").read()
    pref = re.search(r'kIntroPref\s*=\s*"([^"]+)"', hpp).group(1)
    src = open(os.path.join(ui, "shell.cpp"), encoding="utf-8").read()
    lit = lambda t: re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t"}.get(m.group(1), m.group(1)), t)

    def build(expr, mode=None):
        expr = re.sub(r'asset_url\("([^"]+)"\)', r'"/assets/\1"', expr)
        expr = re.sub(r'\(account_mode \? "account" : "browser"\)', '"%s"' % mode, expr)
        out = []
        for q, ident in re.findall(r'"((?:[^"\\]|\\.)*)"|\b([A-Za-z_]\w*)\b', expr):
            out.append(pref if ident in ("kIntroPref", "pref") else lit(q) if not ident else "")
        return "".join(out)
    a = src.index("std::string intro_overlay_html(")
    ret = src[src.index("return", a):src.index(";\n}", a)]
    b = src.index("std::string intro_setting_card(")
    body = src[b:src.index("\n}\n", b)]
    head = re.search(r'std::string html = (.*?);\n', body, re.S).group(1)
    tail = body[body.rindex("return html +"):]
    tail = tail[:tail.rindex(";")]
    return build(ret, "browser"), build(ret, "account"), build(head) + build(tail.replace("return html +", "", 1)), pref


intro_pieces_pref = [None]


def with_intro(home, overlay):
    return home.replace("<body>", "<body>" + overlay, 1)


def make_handler(platform):
    base = w.make_handler(os.path.join(platform, "assets"), "", w.home_markup(platform))
    browser, account, card, _ = intro_pieces(platform)
    home = w.home_page(w.home_markup(platform))
    pages = {"/": with_intro(home, browser), "/as-member": with_intro(home, account),
             "/settings/intro": "<!doctype html><html lang=en><head><meta charset=utf-8><meta name=viewport "
                                "content='width=device-width,initial-scale=1'><link rel=stylesheet href=/assets/site.css>"
                                "</head><body><main class=wrap><h1>Settings</h1>%s</main></body></html>" % card}

    class H(base):
        def do_GET(self):
            path = self.path.split("?")[0]
            if path in pages:
                return self.reply(200, "text/html", pages[path].encode())
            if path == "/assets/intro.mp4":
                return self.reply(200, "video/mp4", open(os.path.join(platform, "assets", "intro.mp4"), "rb").read())
            return base.do_GET(self)
    return H


STATE = r"""(() => { const q = s => document.querySelector(s), box = e => { if (!e || e.hidden || getComputedStyle(e).display === 'none') return null;
    const r = e.getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom}; };
  const P = LG_WALKER && LG_WALKER.CONTROLS.buttons.drop, tip = P && P.tip ? q('.lg-tip.' + P.tip.cls) : null, el = q('#lgintro'), note = q('#lgintronote');
  return JSON.stringify({intro: !!el, atLoad: window.LG_INTRO_AT_LOAD, mode: el && el.dataset.mode, pref: el && el.dataset.pref,
    note: box(note), noteText: note ? note.textContent.trim() : null, link: note && note.querySelector('a') ? note.querySelector('a').getAttribute('href') : null,
    skip: box(q('#lgintroskip')), tip: box(tip), pulse: !!q('.' + P.cls + '.' + LG_WALKER.CONTROLS.home.invite),
    store: (() => { try { return Object.assign({}, localStorage); } catch (e) { return null; } })()}); })()"""


FIT = r"""(() => { const vw = innerWidth, vh = innerHeight, q = s => document.querySelector(s), el = q('#lgintro'), v = q('#lgintrovid');
  if (!el) return JSON.stringify(null);
  const box = e => { if (!e || e.hidden) return null; const r = e.getBoundingClientRect(); return {l: r.left, t: r.top, r: r.right, b: r.bottom}; };
  const on = e => { const b = box(e); if (!b) return false; const h = document.elementFromPoint((b.l + b.r) / 2, (b.t + b.b) / 2); return !!h && (h === e || e.contains(h)); };
  const o = box(el), vb = box(v), iw = v.videoWidth, ih = v.videoHeight;
  let pic = null;
  if (iw && ih) { const k = Math.min((vb.r - vb.l) / iw, (vb.b - vb.t) / ih), w = iw * k, h = ih * k, cx = (vb.l + vb.r) / 2, cy = (vb.t + vb.b) / 2;
    pic = {l: cx - w / 2, t: cy - h / 2, r: cx + w / 2, b: cy + h / 2, aspect: w / h, own: iw / ih}; }
  return JSON.stringify({vw: vw, vh: vh, overlay: o, video: vb, fit: getComputedStyle(v).objectFit, pic: pic,
    skip: box(q('#lgintroskip')), note: box(q('#lgintronote')), skipOn: on(q('#lgintroskip')), noteOn: on(q('#lgintronote'))}); })()"""
SIZES = [(390, 844, True), (844, 390, True), (820, 1180, True), (1280, 800, False), (2560, 1080, False)]


def inside(b, vw, vh, slack=1):
    return bool(b) and b["l"] >= -slack and b["t"] >= -slack and b["r"] <= vw + slack and b["b"] <= vh + slack


class Run(w.Run):
    def fits(self):
        for vw, vh, mobile in SIZES:
            self.size(vw, vh, mobile)
            self.load("/")
            self.j("(localStorage.removeItem('liquidgov.intro.noted'), localStorage.removeItem('liquidgov.%s'), '1')"
                   % intro_pieces_pref[0])
            self.load("/")
            self.wait("document.getElementById('lgintrovid') && document.getElementById('lgintrovid').videoWidth > 0", 5)
            f = self.j(FIT) or {}
            tag = "%dx%d" % (vw, vh)
            self.shot("fit-" + tag)
            o, p, sk, nt = f.get("overlay"), f.get("pic"), f.get("skip"), f.get("note")
            full = bool(o) and abs(o["l"]) <= 1 and abs(o["t"]) <= 1 and abs(o["r"] - vw) <= 1 and abs(o["b"] - vh) <= 1
            self.check("I6 %s: the intro fills the viewport, the clip is contained (inside the viewport, its own aspect, "
                       "not stretched or cropped), Skip and the first-visit line on screen, apart and on top" % tag,
                       full and f.get("fit") == "contain" and bool(p) and inside(p, vw, vh)
                       and abs(p["aspect"] - p["own"]) < 0.01 and inside(sk, vw, vh) and inside(nt, vw, vh)
                       and not w.overlaps(sk, nt) and f.get("skipOn") is True and f.get("noteOn") is True, f)

    def shot(self, name, clip=None):
        args = {"format": "png"}
        if clip:
            args["clip"] = dict(clip, scale=1)
        path = os.path.join(self.out, "phone-intro-%s.png" % name)
        with open(path, "wb") as f:
            f.write(base64.b64decode(self.ws.call("Page.captureScreenshot", **args)["data"]))
        print("SHOT " + path, flush=True)

    def load(self, path):
        self.ws.call("Page.navigate", url=self.base + path)
        self.wait("document.readyState === 'complete' && !!window.LG_WALKER", 10)
        time.sleep(0.4)
        return self.j(STATE) or {}

    def intro(self, pref):
        self.size(390, 844, True)
        self.ws.call("Page.addScriptToEvaluateOnNewDocument", source=HOLD)
        self.load("/")
        self.j("(localStorage.clear(), '1')")
        s = self.load("/")
        n, k = s.get("note"), s.get("skip")
        self.shot("intro-first-phone")
        self.check("I1 fresh browser: the intro is up, its pref is kIntroPref, the first-visit line says it can be turned "
                   "off in Settings (link /settings/intro) and does not meet Skip",
                   s.get("intro") is True and s.get("mode") == "browser" and s.get("pref") == pref and bool(n) and bool(k)
                   and s.get("noteText") == "Plays every visit. Turn off in Settings" and s.get("link") == "/settings/intro"
                   and not w.overlaps(n, k), s)
        self.check("I2 while the intro plays the Drop note waits (not shown under the video)", s.get("tip") is None, s)
        self.tap("#lgintroskip")
        self.wait("!document.getElementById('lgintro')", 3)
        time.sleep(0.3)
        s2 = self.j(STATE) or {}
        self.shot("intro-after-skip-phone", {"x": 0, "y": 0, "width": 390, "height": 420})
        self.check("I2 after Skip the intro is gone, then the Drop note shows and the switch pulses (one at a time)",
                   s2.get("intro") is False and bool(s2.get("tip")) and s2.get("pulse") is True, s2)
        s3 = self.load("/")
        self.check("I3 the next visit plays the intro again, without the first-visit line",
                   s3.get("intro") is True and s3.get("note") is None, s3)
        # I4 the logged-out toggle
        self.ws.call("Page.navigate", url=self.base + "/settings/intro")
        self.wait("document.readyState === 'complete'", 10)
        time.sleep(0.3)
        t0 = self.j("JSON.stringify((() => { const b = document.querySelector('input[data-lg-intro-toggle]');"
                    " return b ? {on: b.checked, label: b.parentNode.textContent.trim(), pref: b.dataset.pref} : null; })())") or {}
        self.shot("intro-settings-phone", {"x": 0, "y": 0, "width": 390, "height": 360})
        self.tap("input[data-lg-intro-toggle]")
        time.sleep(0.2)
        kept = self.j("JSON.stringify(localStorage.getItem('liquidgov.%s'))" % pref)
        s4 = self.load("/")
        self.check("I4 Settings (logged out): 'Play intro video on each visit' is on by default; off keeps '0' on this "
                   "browser and the next visit has no intro (gone before the page shows)",
                   t0.get("on") is True and t0.get("label") == "Play intro video on each visit" and t0.get("pref") == pref
                   and kept == "0" and s4.get("intro") is False and s4.get("atLoad") is False, {"toggle": t0, "kept": kept, "home": s4})
        s5 = self.load("/as-member")
        self.check("I5 signed in (data-mode account, the server checked the account's pref): this browser's '0' does not stop it",
                   s5.get("intro") is True and s5.get("mode") == "account", s5)
        self.ws.call("Page.navigate", url=self.base + "/settings/intro")
        self.wait("document.readyState === 'complete'", 10)
        time.sleep(0.3)
        self.tap("input[data-lg-intro-toggle]")
        time.sleep(0.2)
        s6 = self.load("/")
        self.check("I4 turned on again: the intro is back", s6.get("intro") is True, s6)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(platform, "tests", "browser", "out")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(platform))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-intro-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        intro_pieces_pref[0] = intro_pieces(platform)[3]
        r.intro(intro_pieces_pref[0])
        r.fits()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT intro: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
