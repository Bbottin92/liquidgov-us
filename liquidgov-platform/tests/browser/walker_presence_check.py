"""The map walker tells the presence server which instance it is in (2026-11-14). On the map, lgmap_walk.js is in
the presence server's map room (/here/ws?page=/map) and every position update it sends is {x, y, instanceId}, the
message the dots on profile pages send. The server's snapshot holds only the walkers in the same instance (spatial
step 5), so two walkers on different boxes do not see each other and two on the same box do. New file.

Two real browsers (Bbottin and pal) on the REAL server (phone_serve.sh's throwaway store, as ci_phone_check.py), real
map and walker. Who a walker sees is read from #lgm-walker[data-here]; what it sent, from a recorder put on
WebSocket.send before the page loads. Both are in the group Fresno Neighbors, so that box is on both maps.

  W0  precondition: both chose Yes, both maps loaded, each standing in their own pool, the socket is open
  W1  every message the walker sent has x, y and instanceId; standing in your own pool it says user:<you>
  W2  your own pool is your own place (Brandon 2026-11-15: it was a lobby everyone shared, "mainstream"): each in
      their own pool, neither sees the other
  W3  Bbottin walks onto the group's box: the moves it sends name that box, and still neither sees the other
  W4  pal walks onto the same box: both in that instance, each sees the other again
  W5  Bbottin leaves the map (another page): pal no longer sees Bbottin; back on the map its socket is open again
      and it says user:Bbottin (2026-11-15, Brandon's OK: there is no Drop/Scroll switch any more, so the old Scroll
      case became leaving the map)

Usage as ci_phone_check.py's checks; see ci_walker_presence_check.py."""
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

GROUPS, BOX = "mine:groups", "group:fresno-neighbors"
RECORD = """(() => {
  window.__sent = []; window.__open = 0;
  const send = WebSocket.prototype.send;
  WebSocket.prototype.send = function (m) {
    if (String(this.url).indexOf('/here/ws') >= 0) window.__sent.push(String(m));
    return send.apply(this, arguments);
  };
  const Real = WebSocket;
  window.WebSocket = function (u, p) {
    const s = p === undefined ? new Real(u) : new Real(u, p);
    if (String(u).indexOf('/here/ws') >= 0) { s.addEventListener('open', () => { window.__open++; }); s.addEventListener('close', () => { window.__open--; }); }
    return s;
  };
  window.WebSocket.prototype = Real.prototype;
})()"""
SEEN = ("(() => { const w = document.getElementById('lgm-walker'); return JSON.stringify({here: w && w.getAttribute('data-here'),"
        " open: window.__open, sent: window.__sent}); })()")


class Tab(wg.Run):
    def presence(self):
        d = self.j(SEEN) or {}
        d["sent"] = [json.loads(m) for m in d.get("sent") or []]
        d["sees"] = [u for u in (d.get("here") or "").split(",") if u]
        return d

    def enter_map(self):
        self.size(390, 844, True)
        self.nav("/settings")
        self.post("/here/choice", {"choice": "1", "back": "/settings"})
        self.ws.call("Page.addScriptToEvaluateOnNewDocument", source=RECORD)
        self.nav("/map")
        ok = self.wait("!!window.LG_MAP && !!window.LG_WALKER && document.querySelectorAll('#lgm-nodes .lgn').length >= 3", 15)
        time.sleep(1.0)
        self.drop()
        ok = self.wait("!!document.getElementById('lgm-walker') && !document.getElementById('lgm-walker').hidden", 5) and ok
        time.sleep(0.6)
        self.tp = (self.j(wc.BOXES) or {}).get("touchpad")
        self.reach = (self.j("JSON.stringify(LG_WALKER.CONTROLS.touchpad)") or {}).get("reach", 0)
        return bool(ok)

    def walk_to_group(self):
        self.hop(GROUPS)
        self.open_here()
        self.wait("!!document.querySelector('#lgm-nodes .lgn[data-id=\"%s\"]')" % BOX, 8)
        time.sleep(0.8)
        return self.hop(BOX)


def until(fn, limit=6):
    end = time.time() + limit
    while time.time() < end:
        if fn():
            return True
        time.sleep(0.2)
    return bool(fn())


def cases(a, b):
    A, B = "Bbottin", "pal"
    ok = a.enter_map() and b.enter_map()
    until(lambda: a.presence().get("open") == 1 and b.presence().get("open") == 1 and a.presence()["sent"] and b.presence()["sent"])
    sa, sb, pa, pb = a.state(), b.state(), a.presence(), b.presence()
    a.check("W0 precondition: both chose Yes, maps loaded, each in their own pool (user:<name>), the socket is open",
            ok and all(s.get("at") == "you" and not s.get("hidden") for s in (sa, sb))
            and sa.get("instance") == "user:" + A and sb.get("instance") == "user:" + B
            and pa.get("open") == 1 and pb.get("open") == 1,
            {"A": {k: sa.get(k) for k in ("at", "instance", "hidden")}, "B": {k: sb.get(k) for k in ("at", "instance", "hidden")},
             "A open": pa.get("open"), "B open": pb.get("open")})

    def shaped(m):
        return (isinstance(m.get("x"), (int, float)) and isinstance(m.get("y"), (int, float)) and 0 <= m["x"] <= 1
                and 0 <= m["y"] <= 1 and isinstance(m.get("instanceId"), str) and m["instanceId"])
    a.check("W1 every message the walker sent has x, y and instanceId; standing in your own pool it says user:<you>",
            bool(pa["sent"]) and bool(pb["sent"]) and all(shaped(m) for m in pa["sent"] + pb["sent"])
            and pa["sent"][-1]["instanceId"] == "user:" + A and pb["sent"][-1]["instanceId"] == "user:" + B,
            {"A sent": pa["sent"][-3:], "B sent": pb["sent"][-3:]})

    time.sleep(1.5)   # several snapshots (five a second)
    both = B not in a.presence()["sees"] and A not in b.presence()["sees"]
    a.check("W2 each in their own pool: neither sees the other (your pool is your own place)", both,
            {"A sees": a.presence()["sees"], "B sees": b.presence()["sees"]})

    st = a.walk_to_group()
    apart = until(lambda: B not in a.presence()["sees"] and A not in b.presence()["sees"])
    pa = a.presence()
    a.check("W3 Bbottin walks onto the group's box: its moves name that box, and neither sees the other",
            both and st.get("at") == BOX and st.get("instance") == BOX and bool(pa["sent"])
            and pa["sent"][-1]["instanceId"] == BOX and all(shaped(m) for m in pa["sent"]) and apart
            and b.state().get("instance") == "user:" + B,
            {"A at": st.get("at"), "A sent": pa["sent"][-3:], "A sees": pa["sees"], "B sees": b.presence()["sees"]})

    st = b.walk_to_group()
    together = until(lambda: B in a.presence()["sees"] and A in b.presence()["sees"])
    pb = b.presence()
    a.check("W4 pal walks onto the same box: both in that instance, each sees the other again",
            apart and st.get("at") == BOX and bool(pb["sent"]) and pb["sent"][-1]["instanceId"] == BOX and together,
            {"B at": st.get("at"), "B sent": pb["sent"][-3:], "A sees": a.presence()["sees"], "B sees": pb["sees"]})

    # walking is always on (2026-11-15): the way out of the map room is leaving the map
    a.nav("/feed")
    off = until(lambda: A not in b.presence()["sees"])
    after_off = {"A on": a.j("JSON.stringify(location.pathname)"), "B sees": b.presence()["sees"]}
    a.nav("/map")
    a.wait("!!document.getElementById('lgm-walker') && !document.getElementById('lgm-walker').hidden", 15)
    back = until(lambda: a.presence().get("open") == 1 and bool(a.presence()["sent"])
                 and a.presence()["sent"][-1]["instanceId"] == "user:" + A)
    a.check("W5 Bbottin leaves the map: pal no longer sees Bbottin; back on the map its socket is open again and it says user:Bbottin",
            together and off and back and after_off["A on"] == "/feed",
            {"after leaving": after_off, "back": {"A": a.state().get("instance"), "A open": a.presence().get("open"),
                                                  "A sent": a.presence()["sent"][-2:]}})


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    pal = None
    with open("/tmp/lg-phone-check/sessions.txt", encoding="utf-8") as f:   # phone_serve.sh's seed: who has which session
        for row in f:
            p = row.split()
            if len(p) >= 2 and p[0] == "pal":
                pal = p[1]
    a, b = Tab(base, out), Tab(base, out)
    a.label = b.label = "walkerpresence-" + label
    procs = []
    try:
        for tab, sess in ((a, session), (b, pal)):
            proc, tab.ws = cdp.launch_edge("lg-walker-presence-")
            procs.append((proc, tab))
            for d in ("Page.enable", "Runtime.enable", "Network.enable"):
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
        fails = a.fails + b.fails
        print("VERDICT walkerpresence-%s: %s  (%d checks)" % (label, "ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), a.n + b.n),
              flush=True)
    return a.fails + b.fails
