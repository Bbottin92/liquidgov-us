"""The hidden attribute hides things on every page (Brief 6 item 4, found in its /me screenshots): the site stylesheet's
own display rules (button,.btn{display:inline-flex}, .row{display:flex}, ...) beat the browser's default
[hidden]{display:none}, so a "hidden" Retry showed next to "Saved" on /me. Earlier fixes went one selector at a time
(.dropdown-panel[hidden], #jbstations[hidden], .lgmap-panel[hidden], ctextrow's style.display); this checks the rule
once, for the whole site stylesheet.

Real headless Chromium, NO C++ server: a stdlib server serves a page with css()'s literal text (cut from
src/http_server.cpp) and draft_publish_script()'s, around elements that use the site's own classes.
  H1  control: without the hidden attribute, a button, a .btn link, a .ghost button, a .row and a .card are displayed
  H2  with it, each of them is display:none
  H3  /me's autosave Retry button (draft_publish_script) is not displayed after load, and the status is empty
Usage: LG_BROWSER=$(command -v google-chrome) python3 site_hidden_check.py [<liquidgov-platform dir>]
Prints "VERDICT site-hidden: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed checks unchanged."""
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

KINDS = [("button", "<button type=button %s>b</button>"), ("a.btn", "<a class=btn href=#x %s>a</a>"),
         ("button.ghost", "<button type=button class=ghost %s>g</button>"), ("div.row", "<div class=row %s>r</div>"),
         ("div.card", "<div class=card %s>c</div>")]


def make_handler(css, script):
    shown = "".join("<div data-k='shown %s'>%s</div>" % (k, t % "") for k, t in KINDS)
    hidden = "".join("<div data-k='hidden %s'>%s</div>" % (k, t % "hidden") for k, t in KINDS)
    page = ("<!doctype html><html><head><meta charset=utf-8><style>%s</style></head><body>%s%s"
            "<div class=card><form method=POST action=/me/css data-draft=1><textarea name=css></textarea></form></div>"
            "%s</body></html>" % (css, shown, hidden, script))

    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(page.encode())

    return H


STATE = r"""(() => { const d = {};
  document.querySelectorAll('[data-k]').forEach(p => { d[p.dataset.k] = getComputedStyle(p.firstElementChild).display; });
  const r = document.querySelector('[data-draft-retry]'), s = document.querySelector('[data-draft-status]');
  d.retry = r ? getComputedStyle(r).display : null; d.status = s ? s.textContent : null; return d; })()"""


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    src = __import__("server_source").read(platform)  # whole server source (http_server.cpp + its src/site pieces)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(
        open(os.path.join(platform, "assets", "site.css"), encoding="utf-8").read(), m.literals(src, "std::string draft_publish_script()")))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = m.Run("http://127.0.0.1:%d" % srv.server_address[1])
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-site-hidden-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.ws.call("Page.navigate", url=r.base + "/")
        s, end = None, time.time() + 10
        while time.time() < end:  # ready = the draft script has added its Retry button (a limit, not a fixed sleep)
            s = r.j(STATE)
            if isinstance(s, dict) and s.get("retry") is not None:
                break
            time.sleep(0.05)
        s = s if isinstance(s, dict) else {}
        shown = {k: s.get("shown " + k) for k, _ in KINDS}
        hidden = {k: s.get("hidden " + k) for k, _ in KINDS}
        r.check("H1 control: without hidden, each element is displayed", all(v and v != "none" for v in shown.values()), shown)
        r.check("H2 with hidden, each element is display:none", all(v == "none" for v in hidden.values()), hidden)
        r.check("H3 the autosave Retry button is not displayed after load, status empty",
                s.get("retry") == "none" and s.get("status") == "", {"retry": s.get("retry"), "status": s.get("status")})
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT site-hidden: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
