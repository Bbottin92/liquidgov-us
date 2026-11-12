"""Map side panel is view-only until Edit (Brandon 2026-11-11 4:30 PM decision 2; live map finding #11: tapping
Interests opened the full profile editor with Publish revision, The Gov opened the post composer and Leave, Politics
opened New proposal). Checked in a real Chromium against the real lgmap.js/lgmap.css and the map page's real markup,
with NO C++ server: map_live_check.py's stdlib server and fake graph, with box pages that carry edit forms.

Cases (desktop 1600x1000, then a 390x844 phone):
  V0  precondition: tapping Groups then The Gov opens the side panel on The Gov's page
  V1  in the panel, every form that changes something (method=post) is hidden: the composer, Leave
  V1b what is there to read stays: the heading, the post text, a plain search (GET) form, links
  V2  an Edit button shows in the panel's top bar (the page has edit forms)
  V3  Edit shows the forms; the button then reads Done; Done hides them again
  V4  while editing, a page reload in the panel (a form was sent) keeps the forms shown
  V5  opening another box starts view-only again
  V6  a page with no edit forms: no Edit button
  V7  phone: the full-screen sheet is view-only too, with Edit
Usage: LG_BROWSER=$(command -v google-chrome) python3 map_view_only_check.py [<liquidgov-platform dir>] [<out dir>]
Prints "VERDICT map-view-only: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed checks unchanged."""
import http.server
import json
import os
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import map_live_check as mlc  # noqa: E402  (sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

EDIT_PAGES = {
    # The Gov: what finding #11 saw (post composer + Leave), plus things to read
    "/g/gov": "<h1 id=t>The Gov</h1><p id=post>A post in The Gov</p>"
              "<form id=search method=get action=/g/gov><input name=q><button>Search</button></form>"
              "<form id=compose method=POST action=/g/gov/post><textarea name=text></textarea><button>Post</button></form>"
              "<form id=leave method=post action=/g/gov/leave><button>Leave</button></form>"
              "<a id=next href=/g/gov?again=1>more</a>",
    # Interests: the profile editor
    "/me": "<h1 id=t>You</h1><p id=post>About you</p>"
           "<form id=compose method=post action=/me data-draft><input name=display_name><button>Save</button></form>",
}


def make_handler(platform):
    Base = mlc.make_handler(platform)

    class H(Base):
        def do_GET(self):
            path = self.path.split("?")[0]
            if path in EDIT_PAGES:
                body = "<!doctype html><html><body>%s</body></html>" % EDIT_PAGES[path]
                return self.send(200, "text/html; charset=utf-8", body.encode())
            return Base.do_GET(self)

    return H


FRAME = r"""
window.__v = {
  doc() { const f = document.getElementById('lgm-frame'); try { return f.contentDocument; } catch (e) { return null; } },
  shown(id) { const d = this.doc(); const e = d && d.getElementById(id); if (!e) return null;
              const r = e.getBoundingClientRect(); return d.defaultView.getComputedStyle(e).display !== 'none' && r.height > 0; },
  state() { const p = document.getElementById('lgm-panel'), b = document.getElementById('lgm-edit'), d = this.doc();
    return {panel: !p.hidden, path: d && d.location ? d.location.pathname + d.location.search : null,
            edit: b ? {shown: !b.hidden && b.getBoundingClientRect().height > 0, text: b.textContent.trim()} : null,
            t: this.shown('t'), post: this.shown('post'), search: this.shown('search'), next: this.shown('next'),
            compose: this.shown('compose'), leave: this.shown('leave')}; },
};
1
"""


class Run(mlc.LiveRun):
    def st(self):
        cdp.js(self.ws, FRAME)
        return self.j("__v.state()")

    def wait_frame(self, path, secs=6):
        for _ in range(int(secs / 0.25)):
            s = self.st()
            if isinstance(s, dict) and s.get("path") == path and s.get("t"):
                break
            time.sleep(0.25)
        time.sleep(0.4)
        return self.st()

    def cases(self):
        self.size(1600, 1000)
        self.load()
        self.click_box("mine:groups")
        self.click_box("group:gov", settle=1.6)
        s = self.wait_frame("/g/gov")
        self.check("V0 precondition: The Gov's page is open in the side panel", s.get("panel") and s.get("path") == "/g/gov"
                   and s.get("t"), s)
        self.shot("view-only")
        self.check("V1 the post composer and Leave are hidden", s.get("compose") is False and s.get("leave") is False, s)
        self.check("V1b the heading, the post, a search form and links still show",
                   all(s.get(k) for k in ("t", "post", "search", "next")), s)
        e = s.get("edit") or {}
        self.check("V2 an Edit button shows in the panel's top bar", e.get("shown") and e.get("text") == "Edit", e)
        self.click_sel("#lgm-edit", settle=0.6)
        s = self.st()
        self.check("V3 Edit shows the composer and Leave", s.get("compose") is True and s.get("leave") is True, s)
        self.check("V3b ... and the button reads Done", (s.get("edit") or {}).get("text") == "Done", s.get("edit"))
        self.shot("editing")
        # V4: a reload of the panel's page (what sending a form does) keeps editing
        cdp.js(self.ws, "document.getElementById('lgm-frame').contentWindow.location.reload(), 1")
        time.sleep(0.5)
        s = self.wait_frame("/g/gov")
        self.check("V4 while editing, a reload in the panel keeps the forms shown", s.get("compose") is True, s)
        self.click_sel("#lgm-edit", settle=0.6)
        s = self.st()
        self.check("V3c Done hides them again", s.get("compose") is False and s.get("leave") is False
                   and (s.get("edit") or {}).get("text") == "Edit", s)
        self.click_sel("#lgm-edit", settle=0.6)  # editing again, then another box
        # V6: a page with no edit forms (a person) has no Edit button
        self.click_box("person:bottinbrandon", settle=1.6)
        s = self.wait_frame("/u/bottinbrandon")
        self.check("V6 a page with no edit forms: no Edit button", not (s.get("edit") or {}).get("shown"), s)
        # V5: back to Groups > The Gov: a fresh box opens view-only even though Edit was on before
        self.click_sel("#lgm-back", settle=1.2)
        self.click_sel("#lgm-back", settle=1.2)
        self.click_box("group:gov", settle=1.6)
        s = self.wait_frame("/g/gov")
        self.check("V5 opening another box starts view-only again", s.get("compose") is False and s.get("leave") is False
                   and (s.get("edit") or {}).get("text") == "Edit", s)

        # V7: phone, Interests opens the profile editor page as a sheet
        self.ws.call("Emulation.setDeviceMetricsOverride", width=390, height=844, deviceScaleFactor=2, mobile=True)
        self.load()
        # phone: open the box's page the way a screen reader does (its "Open Interests" button; phone_check covers the
        # press-and-hold gesture itself)
        cdp.js(self.ws, "(() => { const b = document.querySelector('.lgn-sr[aria-label=\"Open Interests\"]');"
                        " if (b) b.dispatchEvent(new MouseEvent('click', {bubbles: true})); return !!b; })()")
        s = self.wait_frame("/me")
        self.shot("phone")
        self.check("V7 phone: Interests opens view-only (no profile editor form)", s.get("path") == "/me" and s.get("t")
                   and s.get("compose") is False, s)
        self.check("V7b phone: Edit shows", (s.get("edit") or {}).get("shown") is True, s.get("edit"))


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    out = argv[1] if len(argv) > 1 else os.path.join("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."), "lg-map-view-only")
    os.makedirs(out, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(platform))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1], out)
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-map-view-only-")
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
        with open(os.path.join(out, "map-view-only.json"), "w", encoding="utf-8") as f:
            json.dump(r.rep, f, indent=1)
        print("VERDICT map-view-only: %s  (%d checks)" % (r.rep["verdict"], len(r.rep["checks"])), flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
