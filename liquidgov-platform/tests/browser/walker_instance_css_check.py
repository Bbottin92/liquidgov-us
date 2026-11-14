"""Spatial step 10 (SPATIAL_EVOLUTION.md): scoped admin CSS for an instance. An operator sets CSS for a map box through
POST /map/instance-css (the one edit path; operators only); the server delivers it with the box (it must pass the same
filter as profile CSS); the walker injects it as text, nested under the side panel, only while it is Inside that box.
New file: no committed check is edited.

REAL C++ server (phone_serve.sh's throwaway store, Bbottin signed in and an operator, as ci_phone_check.py), real map
and walker. The CSS used is hostile on purpose but passes the filter: it tries to cover the whole screen.

  C0  precondition: CSS that loads something (url(...)) is refused by the edit path and never delivered; the hostile
      CSS that passes the filter is accepted; the map delivers it with its box and no other box carries any
  C1  not Inside: no CSS in the page (no <style id=lgm-instance-css>) before the walker enters
  C2  Inside: the walker enters the box; the CSS is in the page as text nested under #lgm-panel, and gone again when
      the walker walks off the box
  C3  NEGATIVE, the one check of this step: with the CSS present and the walker Inside, (a) its instanceId does not
      change, (b) a box whose valve is closed is still not entered (the CSS cannot open it: set on a closed box it is
      never applied), and (c) the page made no new request to anything off this site

Usage as ci_phone_check.py's checks; see ci_walker_instance_css_check.py."""
import json
import os
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import walker_controls_check as wc  # noqa: E402
import walker_group_instance_check as wg  # noqa: E402

BOX = "mine:politics"
HOSTILE = "position:fixed;inset:0;z-index:2147483647;background:#f00;pointer-events:none"
LOADS = "background:url(https://evil.example/x.png)"
OFFSITE = ("JSON.stringify([...performance.getEntriesByType('resource')].map(e => e.name)"
           ".filter(n => new URL(n).origin !== location.origin))")
STYLE = "(() => { const s = document.getElementById('lgm-instance-css'); return JSON.stringify(s ? s.textContent : null); })()"


class Run(wg.Run):
    def set_valve(self, box_id, kind):
        self.j("(() => { document.querySelector('#lgm-nodes .lgn[data-id=' + JSON.stringify(%s) + ']').setAttribute('data-valve', %s); return '1'; })()"
               % (json.dumps(box_id), json.dumps(kind)))

    def edit(self, box_id, css):
        r = self.post("/map/instance-css", {"id": box_id, "css": css})
        return (r or {}).get("status") if isinstance(r, dict) else None

    def cases(self):
        self.size(390, 844, True)
        self.nav("/feed")
        refused = self.edit(BOX, LOADS)
        accepted = self.edit(BOX, HOSTILE)
        self.nav("/map")
        self.wait("!!window.LG_MAP && !!window.LG_WALKER && document.querySelectorAll('#lgm-nodes .lgn').length >= 3", 15)
        time.sleep(1.0)
        got = self.record(BOX) or {}
        others = [i for i in ("mine:friends", "mine:events", "mine:groups") if (self.record(i) or {}).get("css")]
        self.check("C0 precondition: a CSS that loads something is refused, the hostile one is accepted and delivered with its box only",
                   refused == 400 and accepted == 200 and got.get("css") == HOSTILE and not others,
                   {"refused": refused, "accepted": accepted, "delivered": got.get("css"), "other boxes with css": others})
        self.check("C1 not Inside: no instance CSS in the page", self.j(STYLE) is None, self.j(STYLE))
        base = self.j(OFFSITE) or []   # what the site itself loads from elsewhere (its font stylesheet), before any instance CSS
        self.drop()
        self.wait("!!document.getElementById('lgm-walker') && !document.getElementById('lgm-walker').hidden", 5)
        time.sleep(0.6)
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        self.set_valve(BOX, "closed")
        shut = self.hop(BOX)
        closed_css = self.j(STYLE)
        self.set_valve(BOX, "open")
        inside = self.hop(BOX)
        text = self.j(STYLE)
        self.check("C2 Inside: the CSS is in the page as text nested under #lgm-panel",
                   inside.get("at") == BOX and isinstance(text, str) and text.startswith("#lgm-panel *{" + HOSTILE) and "<" not in text,
                   {"at": inside.get("at"), "style": text})
        time.sleep(1.2)
        still = self.state()
        self.set_valve("mine:events", "closed")
        self.hop("you")
        away = self.j(STYLE)
        refused_box = self.hop("mine:events")
        outside = [n for n in (self.j(OFFSITE) or []) if n not in base]
        self.check("C3 NEGATIVE: the CSS cannot change the instanceId, open a closed valve, or load anything;"
                   " and it is gone when the walker leaves",
                   inside.get("instance") == BOX and still.get("instance") == BOX
                   and shut.get("at") != BOX and shut.get("instance") == "mainstream" and closed_css is None
                   and refused_box.get("at") == "you" and away is None and outside == [],
                   {"instance while Inside": still.get("instance"), "closed box + css -> at": shut.get("at"),
                    "css applied on a closed box": closed_css, "closed events box -> at": refused_box.get("at"),
                    "css after leaving": away, "new requests off this site": outside})


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    r = Run(base, out)
    edge = None
    try:
        edge, r.ws = cdp.launch_edge("lg-walker-css-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        host = base.split("//", 1)[1].split("/")[0].split(":")[0]
        r.ws.call("Network.setCookie", name="lg_session", value=session, domain=host, path="/")
        r.cases()
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if edge is not None:
            cdp.stop_edge(edge, r.ws.profile if r.ws else None)
        print("VERDICT walkercss-%s: %s  (%d checks)" % (label, "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return r.fails
