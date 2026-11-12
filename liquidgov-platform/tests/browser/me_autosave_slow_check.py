"""me_autosave_check.py's cases with a server that takes 400 ms to answer each save, like a real network does.
On this box's localhost the save answered in a few ms, so A3 ("Saving…" seen, then "Saved") sampled every 50 ms
sometimes missed "Saving…" (2 of 5 runs at 771f7aa..a892963): a timing gap in that check, not in the page. That file is
committed and frozen, so this new file runs the same cases with the delay (Brief 4 item 11; REPORT4).
Usage: LG_BROWSER=$(command -v google-chrome) python3 me_autosave_slow_check.py [<liquidgov-platform dir>]
Prints "VERDICT me-autosave-slow: ALL-PASS" or the failed cases. Exit 1 on a FAIL."""
import http.server
import os
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import me_autosave_check as m  # noqa: E402  (sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402

DELAY = 0.4


def make_handler(script):
    Base = m.make_handler(script)

    class H(Base):
        def do_POST(self):
            time.sleep(DELAY)
            return Base.do_POST(self)

    return H


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    src = __import__("server_source").read(platform)  # whole server source (http_server.cpp + its src/site pieces)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(m.literals(src, "std::string draft_publish_script()")))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = m.Run("http://127.0.0.1:%d" % srv.server_address[1])
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-me-autosave-slow-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases(m.me_handler_markup(src))
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT me-autosave-slow: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
