"""The map draws the other droplets in your instance (2026-11-14): each person in the presence
snapshot (walker_presence_check.py) is a small circle on a ring around the walker (#lgm-here .lgm-here-dot[data-u],
lgmap_walk.js hereDraw). They are removed when that person leaves your instance or the map.

Two real browsers (Bbottin and pal) on the REAL server, as walker_presence_check.py (its Tab is reused as it is).

  D0  precondition: both on the map, each in their own pool; no dot is drawn for yourself
  D1  your own pool is your own place (Brandon 2026-11-15): each in their own pool, neither has a dot
  D2  Bbottin walks onto the group's box (another instance): neither has a dot
  D3  pal walks onto the same box: each has one dot, the other's, a small circle on the screen next to the walker
  D4  Bbottin leaves the map (another page): pal no longer has Bbottin's dot (2026-11-15, Brandon's OK: no
      Drop/Scroll switch any more, so the old Scroll case became leaving the map)

Usage as ci_phone_check.py's checks; see ci_walker_presence_dots_check.py."""
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402
import walker_presence_check as wp  # noqa: E402

DOTS = """(() => { const w = document.getElementById('lgm-walker'), wr = w.getBoundingClientRect();
  const dots = [...document.querySelectorAll('.lgm-here-dot')].map(d => { const r = d.getBoundingClientRect(), cs = getComputedStyle(d);
    return {u: d.getAttribute('data-u'), w: r.width, h: r.height, round: cs.borderTopLeftRadius,
            shown: r.width > 0 && cs.visibility !== 'hidden' && cs.display !== 'none',
            onscreen: r.left >= 0 && r.top >= 0 && r.right <= innerWidth && r.bottom <= innerHeight,
            far: Math.hypot(r.left + r.width / 2 - (wr.left + wr.width / 2), r.top + r.height / 2 - (wr.top + wr.height / 2))}; });
  return JSON.stringify(dots); })()"""


def dots(tab):
    return tab.j(DOTS) or []


def who(tab):
    return sorted(d["u"] for d in dots(tab) if d["shown"])


def good(d):
    """a small circle next to the walker"""
    return d["shown"] and d["onscreen"] and 8 <= d["w"] <= 28 and abs(d["w"] - d["h"]) < 1 and d["round"] == "50%" and 10 <= d["far"] <= 60


def cases(a, b):
    A, B = "Bbottin", "pal"
    ok = a.enter_map() and b.enter_map()
    sa, sb = a.state(), b.state()
    a.check("D0 precondition: both on the map, each in their own pool; no dot for yourself",
            ok and all(s.get("at") == "you" and not s.get("hidden") for s in (sa, sb))
            and sa.get("instance") == "user:" + A and sb.get("instance") == "user:" + B
            and A not in who(a) and B not in who(b),
            {"A": {k: sa.get(k) for k in ("at", "instance", "hidden")}, "B": {k: sb.get(k) for k in ("at", "instance", "hidden")}})

    wp.time.sleep(1.5)   # several snapshots
    da, db = dots(a), dots(b)
    both = not da and not db and not who(a) and not who(b)
    a.check("D1 each in their own pool: neither has a dot", both, {"A draws": da, "B draws": db})

    st = a.walk_to_group()
    apart = wp.until(lambda: not dots(a) and not dots(b))
    a.check("D2 Bbottin walks onto the group's box (another instance): neither has a dot",
            both and st.get("instance") == wp.BOX and b.state().get("instance") == "user:" + B and apart,
            {"A instance": st.get("instance"), "A draws": dots(a), "B draws": dots(b)})

    st = b.walk_to_group()
    together = wp.until(lambda: who(a) == [B] and who(b) == [A] and all(good(d) for d in dots(a) + dots(b)))
    da, db = dots(a), dots(b)
    a.check("D3 pal walks onto the same box: each has one dot, the other's, a small circle next to the walker",
            apart and st.get("instance") == wp.BOX and together and len(da) == 1 and len(db) == 1, {"A draws": da, "B draws": db})

    a.nav("/feed")   # walking is always on (2026-11-15): the way out is leaving the map
    off = wp.until(lambda: not dots(b))
    a.check("D4 Bbottin leaves the map: pal no longer has Bbottin's dot",
            together and off and not b.state().get("hidden") and a.j("JSON.stringify(location.pathname)") == "/feed",
            {"A on": a.j("JSON.stringify(location.pathname)"), "B draws": dots(b)})


def run(base, session, expected_js, label, out):
    """Same shape as phone_check.run (expected_js unused). Returns the failed check names (empty = all pass)."""
    os.makedirs(out, exist_ok=True)
    pal = None
    with open("/tmp/lg-phone-check/sessions.txt", encoding="utf-8") as f:   # phone_serve.sh's seed: who has which session
        for row in f:
            p = row.split()
            if len(p) >= 2 and p[0] == "pal":
                pal = p[1]
    a, b = wp.Tab(base, out), wp.Tab(base, out)
    a.label = b.label = "walkerdots-" + label
    procs = []
    try:
        for tab, sess in ((a, session), (b, pal)):
            proc, tab.ws = cdp.launch_edge("lg-walker-dots-")
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
        print("VERDICT walkerdots-%s: %s  (%d checks)" % (label, "ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), a.n + b.n),
              flush=True)
    return a.fails + b.fails
