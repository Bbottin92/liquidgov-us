"""Spatial step 8 (SPATIAL_EVOLUTION.md): a new group is a new instance. Brandon's decision (2026-11-14): derive it from
the existing group box, no new record type. A group made on the real server appears on the map as a box; that box is
the instance, with a region (Step 2) and a copy of its parent's valve (Step 7), walkable, and its creator can enter it.
New file: no committed check is edited.

REAL C++ server (phone_serve.sh's throwaway store, Bbottin signed in, as ci_phone_check.py), real map and walker.
One thing is staged in the page: the answer for the "you" box lists Groups with a valve private to you
(a script added before the page loads), so inheritance copies something other than Open, and the creator's right to
enter is tested under a valve that is not Open.

  G0  precondition: the group was made; the map loaded; the Groups box carries the private-to-you valve
  G1  the walker enters Groups (private to you) and opens it; the new group appears there as a box
  G2  that box is an instance: it has a region (a center and half sizes above zero) and a copy of Groups' valve
      (private, owner you), not the default
  G3  the creator walks onto it: the walker stands on it and its instanceId is that box

Writes <out>/phone-walkergroup-<label>.json. Usage as ci_phone_check.py's checks; see ci_walker_group_instance_check.py."""
import json
import os
import sys
import time
import traceback
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import walker_controls_check as wc  # noqa: E402
import walker_instance_check as wi  # noqa: E402

SLUG = "stepeightclub"
STAGE = """(() => {
  const real = window.fetch;
  window.fetch = function (url) {
    const p = real.apply(this, arguments);
    if (String(url).indexOf('/map/node?n=you') < 0) return p;
    return p.then(r => r.clone().json().then(d => {
      const me = (document.getElementById('lgmap') || {getAttribute: () => ''}).getAttribute('data-me');
      (d.links || []).forEach(l => { if (l.node && l.node.id === 'mine:groups') l.node.valve = {type: 'private', owner: me}; });
      return new Response(JSON.stringify(d), {status: r.status, headers: r.headers});
    }));
  };
})()"""


class Run(wi.Run):
    def post(self, path, fields):
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v, safe="")) for k, v in fields.items())
        return self.j("fetch(%s, {method: 'POST', credentials: 'same-origin', redirect: 'manual',"
                      " headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: %s})"
                      ".then(r => JSON.stringify({status: r.status, type: r.type}))" % (json.dumps(path), json.dumps(body)))

    def nav(self, path):
        self.ws.call("Page.navigate", url=self.base + path)
        self.wait("document.readyState === 'complete'", 20)

    def record(self, box_id):
        return self.j("JSON.stringify(window.LG_MAP && LG_MAP.instance(%s))" % json.dumps(box_id))

    def cases(self):
        self.size(390, 844, True)
        self.nav("/feed")
        made = self.post("/communities", {"slug": SLUG, "name": "Step Eight Club", "category": "0", "subcategory": "x", "about": "x"})
        self.ws.call("Page.addScriptToEvaluateOnNewDocument", source=STAGE)
        self.nav("/map")
        self.wait("!!window.LG_MAP && !!window.LG_WALKER && document.querySelectorAll('#lgm-nodes .lgn').length >= 3", 15)
        time.sleep(1.0)
        me = self.j("JSON.stringify(document.getElementById('lgmap').getAttribute('data-me'))")
        parent = self.record("mine:groups") or {}
        self.check("G0 precondition: the group was made; the map loaded; the Groups box carries the private-to-you valve",
                   isinstance(made, dict) and made.get("status") in (0, 302, 303) and bool(me)
                   and (parent.get("valve") or {}).get("type") == "private" and (parent.get("valve") or {}).get("owner") == me,
                   {"made": made, "me": me, "Groups": parent})
        self.drop()
        self.wait("!!document.getElementById('lgm-walker') && !document.getElementById('lgm-walker').hidden", 5)
        time.sleep(0.6)
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        gid = "group:" + SLUG
        st = self.hop("mine:groups")
        self.key("enter")
        appeared = self.wait("!!document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]')" % gid, 8)
        time.sleep(0.8)
        self.check("G1 the walker enters Groups (private to you) and opens it; the new group appears there as a box",
                   st.get("at") == "mine:groups" and bool(appeared), {"at": st.get("at"), "appeared": appeared})
        child = self.record(gid) or {}
        reg = child.get("region") or {}
        self.check("G2 that box is an instance: a region above zero and a copy of the Groups valve (private, owner you, nobody invited)",
                   all(isinstance(reg.get(k), (int, float)) for k in ("cx", "cy", "hw", "hh")) and reg.get("hw", 0) > 0
                   and reg.get("hh", 0) > 0 and (child.get("valve") or {}).get("type") == "private"
                   and (child.get("valve") or {}).get("owner") == me and (child.get("valve") or {}).get("invite") == [],
                   {"group": child, "Groups": parent.get("valve")})
        st = self.hop(gid)
        self.check("G3 the creator walks onto it: the walker stands on it and its instanceId is that box",
                   st.get("at") == gid and st.get("instance") == gid, {"at": st.get("at"), "instance": st.get("instance")})


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    r = Run(base, out)
    r.label = "walkergroup-" + label
    edge = None
    try:
        edge, r.ws = cdp.launch_edge("lg-walker-group-")
        for d in ("Page.enable", "Runtime.enable", "Network.enable"):
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
        print("VERDICT walkergroup-%s: %s  (%d checks)" % (label, "ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return r.fails
