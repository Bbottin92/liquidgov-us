"""Spatial step 11 (SPATIAL_EVOLUTION.md): proximity voice and video between dots on a profile page. Two real browsers (Bbottin
and pal) with a fake microphone and camera, on the REAL server (phone_serve.sh's throwaway store, as ci_phone_check.py).
Media is off until each turns it on; then, in the same instance and within range, a direct connection carries it and the
volume of what you hear grows as the dots come closer; apart, or in different instances, it stops. New file.

  M0  off by default: neither has a microphone or camera on, the server's snapshot says so, and nothing is connected
  M1  both turn the microphone on, 0.45 of the page apart (inside the 0.5 range, outside the 0.15 near edge): both are
      connected, hear each other quietly (a volume above 0 and below 1) and the server shows both as sharing
  M2  closer, 0.10 apart: the volume rises to full on both
  M3  apart again, 0.75 apart: both hang up, no connection and no media element is left on either side
  M4  close again, then pal moves to another instance: media stops on both (the server's own instance filter and
      the server's relay rule decide; eject and valves act on the same instance, so media cannot bypass them)
  M5  the server's rule, not just the browsers' manners: with both still sharing, a setup message sent to the other
      person while 0.75 apart, and again while in different instances, never arrives; positive control: during M1
      setup messages did arrive

Usage as ci_phone_check.py's checks; see ci_here_media_check.py."""
import json
import os
import stat
import sys
import tempfile
import time
import traceback
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import walker_controls_check as wc  # noqa: E402

ROOM = "/u/stranger"
STATE = "JSON.stringify(window.LG_HERE_MEDIA ? LG_HERE_MEDIA.state() : null)"


class Tab(wc.Run):
    def st(self):
        return self.j(STATE) or {}

    def post(self, path, fields):
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v, safe="")) for k, v in fields.items())
        return self.j("fetch(%s, {method: 'POST', credentials: 'same-origin', redirect: 'manual',"
                      " headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: %s})"
                      ".then(r => JSON.stringify({status: r.status}))" % (json.dumps(path), json.dumps(body)))

    def open_room(self):
        self.ws.call("Page.navigate", url=self.base + "/settings")
        self.wait("document.readyState === 'complete'", 20)
        self.post("/here/choice", {"choice": "1", "back": "/settings"})
        self.ws.call("Page.navigate", url=self.base + ROOM)
        up = self.wait("document.readyState === 'complete' && !!window.LG_HERE_MEDIA && !!window.LG_HERE_API", 20)
        self.j("(() => { window.__sigs = 0; LG_HERE_API.onSignal(() => { window.__sigs++; }); return '1'; })()")   # counts setup messages that arrive
        return up

    def sigs(self):
        return self.j("JSON.stringify(window.__sigs)")

    def poke(self, to):
        """Send a setup message to `to` the way the media module would; whether it arrives is the server's call."""
        self.j("(() => { LG_HERE_API.send({to: %s, sig: JSON.stringify({k: 'ice', c: null})}); return '1'; })()" % json.dumps(to))

    def move(self, x, y):
        self.j("(() => { LG_HERE_API.moveTo(%s, %s); return '1'; })()" % (x, y))

    def click(self, kind):
        """The switch is in your drop's menu now (Brandon 2026-11-14: no microphone or camera buttons on the screen):
        open the menu from your own drop, press the switch, close the menu."""
        self.j("(() => { const W = LG_WALKER, d = document.querySelector('#lg-here .lg-here-dot.me .' + W.CONTROLS.shape.cls);"
               " d.click(); document.querySelector('#' + W.CONTROLS.menu.id + ' [data-kind=\"%s\"]').click(); W.menuClose(false);"
               " return '1'; })()" % kind)

    def peer(self, who):
        return (self.st().get("peers") or {}).get(who)


def until(fn, limit=12):
    end = time.time() + limit
    while time.time() < end:
        v = fn()
        if v:
            return v
        time.sleep(0.2)
    return fn()


def cases(a, b):
    ok = a.open_room() and b.open_room()
    s0a, s0b = a.st(), b.st()
    time.sleep(0.8)
    a.check("M0 off by default: no microphone or camera on, the server's snapshot says so, nothing connected",
            ok and not any(s["want"]["mic"] or s["want"]["cam"] or s["peers"] for s in (s0a, s0b))
            and all((s.get("self") or {}).get("mic") is False and (s.get("self") or {}).get("cam") is False for s in (a.st(), b.st())),
            {"A": a.st(), "B": b.st()})
    a.move(0.2, 0.3)
    b.move(0.65, 0.3)
    a.click("mic")
    b.click("mic")

    def linked(t, who):
        p = t.peer(who)
        return p if p and p["conn"] == "connected" and p["volume"] is not None and p["tracks"] > 0 else None
    pa, pb = until(lambda: linked(a, "pal")), until(lambda: linked(b, "Bbottin"))
    sa, sb = a.st(), b.st()
    a.check("M1 both turn the microphone on, 0.45 apart: connected, heard quietly, and the server shows both sharing",
            bool(pa) and bool(pb) and 0 < pa["volume"] < 1 and 0 < pb["volume"] < 1
            and (sa["self"] or {}).get("mic") is True and (sb["self"] or {}).get("mic") is True,
            {"A hears pal": pa, "B hears Bbottin": pb, "A self": sa.get("self"), "B self": sb.get("self")})
    quiet = (pa or {}).get("volume")
    arrived = (a.sigs() or 0) > 0 and (b.sigs() or 0) > 0

    b.move(0.3, 0.3)
    full = until(lambda: (a.peer("pal") or {}).get("volume") == 1 and (b.peer("Bbottin") or {}).get("volume") == 1)
    a.check("M2 closer, 0.10 apart: the volume rises to full on both",
            bool(full) and quiet is not None and quiet < 1, {"quiet": quiet, "A": a.peer("pal"), "B": b.peer("Bbottin")})

    b.move(0.95, 0.3)
    gone = until(lambda: not a.st().get("peers") and not b.st().get("peers") and not a.st().get("elements") and not b.st().get("elements"))
    a.check("M3 apart again, 0.75 apart: both hang up, no connection and no media element is left",
            bool(gone), {"A": a.st(), "B": b.st()})
    far_before = b.sigs()
    a.poke("pal")
    time.sleep(1.5)
    far_blocked = b.sigs() == far_before

    b.move(0.3, 0.3)
    again = until(lambda: linked(a, "pal") and linked(b, "Bbottin"))
    b.j("(() => { LG_HERE_API.send({instanceId: 'other-instance'}); return '1'; })()")
    away = until(lambda: not a.st().get("peers") and not b.st().get("peers") and not a.st().get("elements") and not b.st().get("elements"))
    a.check("M4 close again, then pal moves to another instance: media stops on both",
            bool(again) and bool(away), {"reconnected": bool(again), "A": a.st(), "B": b.st()})
    inst_before = b.sigs()
    a.poke("pal")
    time.sleep(1.5)
    inst_blocked = b.sigs() == inst_before
    a.check("M5 the server refuses setup messages to someone out of range or in another instance (and relayed them in M1)",
            arrived and far_blocked and inst_blocked, {"arrived in M1": arrived, "far blocked": far_blocked, "other instance blocked": inst_blocked})


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
                  ' --autoplay-policy=no-user-gesture-required "$@"\n' % original)
    wrapper.close()
    os.chmod(wrapper.name, os.stat(wrapper.name).st_mode | stat.S_IXUSR)
    cdp.EDGE = wrapper.name
    a = Tab(base, out)
    b = Tab(base, out)
    procs = []
    try:
        for tab, sess in ((a, session), (b, pal)):
            proc, tab.ws = cdp.launch_edge("lg-here-media-")
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
        for proc, tab in procs:
            cdp.stop_edge(proc, tab.ws.profile if tab.ws else None)
        cdp.EDGE = original
        os.unlink(wrapper.name)
        fails = a.fails + b.fails
        print("VERDICT heremedia-%s: %s  (%d checks)" % (label, "ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), a.n + b.n),
              flush=True)
    return a.fails + b.fails
