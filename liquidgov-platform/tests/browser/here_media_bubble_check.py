"""Someone's camera is a small round bubble on their own dot (Brandon 2026-11-14: "drawn over your droplet and it should
be smaller and round. and if you click on it it should enlarge it and there should be a fullscreen button that appears
when you hover it when its enlarged"). assets/here_media.js .lg-media-bubble, site.css. New file.

Two real browsers (Bbottin and pal) with a fake camera on the REAL server, as here_media_check.py (its Tab, reused).
Looked at from Bbottin's browser, at pal's picture:

  B0  precondition: both turn the camera on, in range; pal's picture arrives and is playing
  B1  it is inside pal's dot, centered on pal's portrait, small (at most 48px) and round; not in the corner strip
  B2  it moves with the dot: pal walks, the bubble is still centered on pal's portrait
  B3  small: no fullscreen button to be seen
  B4  click it: it is enlarged (at least 150px wide), the whole bubble on the screen; the fullscreen button is there
      but not visible while the pointer is elsewhere
  B5  pointer over the enlarged picture: the fullscreen button is visible; clicking it puts pal's video fullscreen
  B6  out of fullscreen, click the picture again: small and round again
  B7  pal turns the camera off: the bubble is gone

Usage as ci_phone_check.py's checks; see ci_here_media_bubble_check.py."""
import json
import os
import stat
import sys
import tempfile
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import here_media_check as hm  # noqa: E402

LOOK = """(() => { const b = document.querySelector('.lg-media-bubble[data-from="pal"]');
  if (!b) return JSON.stringify(null);
  const r = b.getBoundingClientRect(), cs = getComputedStyle(b), v = b.querySelector('video'), f = b.querySelector('.lg-media-full');
  const dot = b.closest('.lg-here-dot'), pic = dot && dot.querySelector('img, .lg-here-ph'), pr = pic ? pic.getBoundingClientRect() : null;
  const fr = f.getBoundingClientRect(), fs = getComputedStyle(f);
  return JSON.stringify({w: r.width, h: r.height, cx: r.left + r.width / 2, cy: r.top + r.height / 2, radius: cs.borderTopLeftRadius,
    shown: cs.display !== 'none' && r.width > 0, onscreen: r.left >= 0 && r.top >= 0 && r.right <= innerWidth && r.bottom <= innerHeight,
    inDot: dot ? dot.getAttribute('data-u') : null, inStrip: !!b.closest('#lg-media-held'), big: b.classList.contains('lg-media-big'),
    pcx: pr ? pr.left + pr.width / 2 : null, pcy: pr ? pr.top + pr.height / 2 : null,
    playing: !!v && v.readyState >= 2 && v.videoWidth > 0 && !v.paused,
    full: {shown: fs.display !== 'none' && fr.width > 0, opacity: parseFloat(fs.opacity), x: fr.left + fr.width / 2, y: fr.top + fr.height / 2},
    fullscreen: !!document.fullscreenElement && document.fullscreenElement === v}); })()"""


def look(t):
    return t.j(LOOK)


def centered(s):
    return s and s["pcx"] is not None and abs(s["cx"] - s["pcx"]) <= 3 and abs(s["cy"] - s["pcy"]) <= 3


def small_round(s):
    return bool(s) and s["shown"] and not s["big"] and s["w"] <= 48 and abs(s["w"] - s["h"]) < 1 and s["radius"] == "50%"


def mouse(t, kind, x, y):
    t.ws.call("Input.dispatchMouseEvent", type=kind, x=x, y=y, button="none" if kind == "mouseMoved" else "left",
              clickCount=0 if kind == "mouseMoved" else 1)
    time.sleep(0.1)


def click(t, x, y):
    mouse(t, "mouseMoved", x, y)
    mouse(t, "mousePressed", x, y)
    mouse(t, "mouseReleased", x, y)
    time.sleep(0.4)


def cases(a, b):
    ok = a.open_room() and b.open_room()
    a.move(0.3, 0.3)
    b.move(0.5, 0.35)
    a.click("cam")
    b.click("cam")
    hm.until(lambda: (look(a) or {}).get("playing"), 15)
    time.sleep(0.6)   # the dot glides to its place
    s = look(a)
    b0 = bool(ok) and bool(s) and s["playing"]
    # 2026-11-21: B0 failed once in 21 CI runs with no bubble at all (master 0e96918) and nothing to say why; a failure
    # now carries whether both rooms opened, what Bbottin's page holds for pal, and the setup messages each side got
    a.check("B0 precondition: both turn the camera on, in range; pal's picture arrives and is playing", b0,
            s if b0 else {"bubble": s, "rooms opened": bool(ok), "pal as Bbottin's peer": a.peer("pal"),
                          "setup messages (Bbottin, pal)": [a.sigs(), b.sigs()]})
    a.check("B1 it is inside pal's dot, centered on pal's portrait, small and round; not in the corner strip",
            small_round(s) and s["inDot"] == "pal" and not s["inStrip"] and centered(s), s)
    b.move(0.6, 0.25)
    time.sleep(1.2)
    s2 = look(a)
    a.check("B2 it moves with the dot: pal walks, the bubble is still centered on pal's portrait",
            bool(s) and small_round(s2) and centered(s2) and s2["playing"] and abs(s2["cx"] - s["cx"]) > 10, {"before": s, "after": s2})
    a.check("B3 small: no fullscreen button to be seen", bool(s2) and not s2["full"]["shown"], s2 and s2["full"])

    click(a, s2["cx"], s2["cy"]) if s2 else None
    mouse(a, "mouseMoved", 5, 5)   # the pointer elsewhere
    time.sleep(0.4)
    s3 = look(a)
    a.check("B4 click it: enlarged, all of it on the screen; the fullscreen button is there but not visible with the pointer elsewhere",
            bool(s3) and s3["big"] and s3["w"] >= 150 and s3["onscreen"] and s3["playing"] and s3["full"]["shown"]
            and s3["full"]["opacity"] == 0, s3)

    mouse(a, "mouseMoved", s3["cx"], s3["cy"]) if s3 else None
    time.sleep(0.5)
    s4 = look(a)
    seen = bool(s4) and s4["full"]["opacity"] == 1
    click(a, s4["full"]["x"], s4["full"]["y"]) if s4 else None
    fs = hm.until(lambda: (look(a) or {}).get("fullscreen"), 4)
    a.check("B5 pointer over the enlarged picture: the fullscreen button is visible; clicking it puts pal's video fullscreen",
            seen and bool(fs) and (look(a) or {}).get("big"), {"hover": s4 and s4["full"], "fullscreen": fs})

    a.j("(document.fullscreenElement ? document.exitFullscreen() : Promise.resolve()).then(() => '1')")
    hm.until(lambda: not (look(a) or {}).get("fullscreen"), 4)
    time.sleep(0.4)
    s5 = look(a)
    click(a, s5["cx"], s5["cy"]) if s5 else None
    s6 = look(a)
    a.check("B6 out of fullscreen, click the picture again: small and round again",
            bool(s5) and s5["big"] and not s5["fullscreen"] and small_round(s6) and centered(s6), {"before": s5, "after": s6})

    b.click("cam")
    gone = hm.until(lambda: look(a) is None, 8)
    a.check("B7 pal turns the camera off: the bubble is gone", bool(gone), look(a))


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    pal = None
    with open("/tmp/lg-phone-check/sessions.txt", encoding="utf-8") as f:   # phone_serve.sh's seed: who has which session
        for row in f:
            p = row.split()
            if len(p) >= 2 and p[0] == "pal":
                pal = p[1]
    original = cdp.EDGE
    wrapper = tempfile.NamedTemporaryFile("w", suffix=".sh", delete=False)
    wrapper.write('#!/bin/bash\nexec "%s" --use-fake-ui-for-media-stream --use-fake-device-for-media-stream'
                  ' --autoplay-policy=no-user-gesture-required'
                  # a desktop with a mouse: headless Chromium otherwise says it cannot hover, which is the touch-screen
                  # rule (fullscreen button always shown)
                  ' --blink-settings=primaryHoverType=2,availableHoverTypes=2,primaryPointerType=4,availablePointerTypes=4'
                  ' "$@"\n' % original)
    wrapper.close()
    os.chmod(wrapper.name, os.stat(wrapper.name).st_mode | stat.S_IXUSR)
    cdp.EDGE = wrapper.name
    a, b = hm.Tab(base, out), hm.Tab(base, out)
    procs = []
    try:
        for tab, sess in ((a, session), (b, pal)):
            proc, tab.ws = cdp.launch_edge("lg-here-bubble-")
            procs.append((proc, tab))
            for d in ("Page.enable", "Runtime.enable"):
                tab.ws.call(d)
            host = base.split("//", 1)[1].split("/")[0].split(":")[0]
            tab.ws.call("Network.setCookie", name="lg_session", value=sess, domain=host, path="/")
        cases(a, b)
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        a.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if os.environ.get("LG_BUBBLE_SHOT") and procs:
            try:
                import base64
                r = a.ws.call("Page.captureScreenshot", format="png")
                with open(os.environ["LG_BUBBLE_SHOT"], "wb") as f:
                    f.write(base64.b64decode(r["data"]))
            except Exception:
                pass
        for proc, tab in procs:
            cdp.stop_edge(proc, tab.ws.profile if tab.ws else None)
        cdp.EDGE = original
        os.unlink(wrapper.name)
        fails = a.fails + b.fails
        print("VERDICT herebubble-%s: %s  (%d checks)" % (label, "ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), a.n + b.n),
              flush=True)
    return a.fails + b.fails
