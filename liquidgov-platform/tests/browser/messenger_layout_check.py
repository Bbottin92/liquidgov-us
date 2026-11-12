"""Messages page layout (Brandon 2026-11-11 4:37 PM, Brief 4 item 8; site-look2 FINDINGS 2): on wide screens the
conversation list sits beside the open thread; the composer stays pinned under the thread; one scroll region (the
page; the transcript has no scrollbar of its own); the age-assurance note is one line plus "more"; the heading says
Messages, not Mail; no bubble is cut off at 390 px.

Real headless Chromium with the REAL site stylesheet (css()'s literals cut from src/http_server.cpp) and the GET /mail
handler's own literal text for the top of the page (heading, note, the wrapper's classes); the thread and the list are
built here in the shapes mail_thread_html / mail_inbox_html emit (.mailthread > .mailthreadhead, .msglist > .msg.mine|
.theirs > .bubble, form.mailcomposer; .mailsection > .inboxlist > a.inboxrow). NO C++ server.

  M1  the GET /mail handler's heading is <h1>Messages</h1> (not Mail)
  M2  the age note shows one line with a "more" toggle; the long explanation is hidden until "more" is opened
  M3  1280 px: the conversation list is beside the thread (side by side, overlapping vertically)
  M4  at 390/768/1280: the transcript (#msglist) has no scroll region of its own
  M5  at 390/768/1280, scrolled to the top of the thread: the composer is on screen (pinned under the thread)
  M6  at 390/768/1280: no bubble runs past the transcript's right edge or the window; the page has no sideways scroll
Usage: LG_BROWSER=$(command -v google-chrome) python3 messenger_layout_check.py [<liquidgov-platform dir>]
Prints "VERDICT messenger-layout: ALL-PASS" or the failed cases. Exit 1 on a FAIL. New file; committed checks unchanged."""
import http.server
import json
import os
import re
import sys
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import juketube_overlap_check as jo  # noqa: E402  (site_css; sets LG_BROWSER on Linux before cdp is imported)
import cdp  # noqa: E402


def mail_handler(src):
    a = src.index('if(method=="GET"&&path=="/mail") {')
    b = src.index('return pg("Messages"', a)
    return src[a:b]


def lits(code):
    body = "\n".join(l for l in code.splitlines() if not l.strip().startswith("//"))
    return "".join(p.encode().decode("unicode_escape") for p in re.findall(r'"((?:[^"\\]|\\.)*)"', body))


def top_html(handler):
    """The handler's literal page top: everything it writes before the thread (heading, note)."""
    head = handler[:handler.index('if (qs["skipped"]')]
    head = head[head.index("std::string html"):]
    return lits(head)


def wrapper_open(handler):
    """The handler's opening tag(s) around thread + list, e.g. '<div class="card mailwidget">' (with a thread open)."""
    m = re.search(r'html \+= (.*?)\+thread\+mail_inbox_html', handler, re.S)
    if not m:
        return '<div class="card mailwidget">'
    code = m.group(1)
    # a class added only when a thread is open: keep the "with a thread" branch of a ?: on thread.empty()
    code = re.sub(r'\(thread\.empty\(\)\s*\?\s*"[^"]*"\s*:\s*("[^"]*")\)', r'\1', code)
    return lits(code)


LONGURL = "https://example.com/" + "a" * 160
MSGS = []
for i in range(30):
    mine = i % 2 == 0
    text = ("hey, did you see the council agenda for Tuesday? " * (1 + i % 3)) if i % 5 else LONGURL
    MSGS.append('<div class="msg %s"><div class="bubble">%s<span class="bubtime">2026-11-11</span></div></div>'
                % ("mine" if mine else "theirs", text))


def thread_html():
    return ('<div class="mailthread"><div class="mailthreadhead"><a class="mailback" href="/mail">&larr;</a>'
            '<span class="portrait sm ph"></span><strong><a href="/u/pal">pal</a></strong></div>'
            '<div class="msglist" id="msglist">' + "".join(MSGS) + '</div>'
            '<form method="POST" action="/mail" class="mailcomposer" id="mailcomposer">'
            '<div class="row" style="align-items:flex-end;gap:.4rem;flex-wrap:nowrap">'
            '<textarea name="text" id="mailtext" rows="2" style="flex:1;min-width:0;min-height:44px"></textarea>'
            '<label class="mailattbtn">&#128206;</label><button type="submit">Send</button></div>'
            '<p class="muted" style="margin:.3rem 0 0">Enter sends. Shift+Enter is a new line.</p></form></div>')


def inbox_html():
    rows = "".join('<a class="inboxrow mailwith" href="/mail?with=f%d"><span class="portrait sm ph"></span>'
                   '<span style="min-width:0;flex:1"><span class="inboxtitle">friend%d</span>'
                   '<span class="inboxpreview">last thing they said to you here</span></span>'
                   '<span class="inboxmeta">2026-11-11</span></a>' % (i, i) for i in range(8))
    return ('<div class="mailsection"><button type="button" class="ghost mailgroupbtn">Group</button>'
            '<p class="eyebrow">Friends</p><div class="inboxlist">' + rows + '</div>'
            '<p class="maildisclose">Messages on this copy are stored so the operator can read them.</p></div>')


def page(platform):
    src = open(os.path.join(platform, "src", "http_server.cpp"), encoding="utf-8").read()
    h = mail_handler(src)
    body = top_html(h) + wrapper_open(h) + thread_html() + inbox_html() + "</div>"
    if body.count("<div") > body.count("</div"):  # a page wrapper the handler opened before the widget
        body += "</div>" * (body.count("<div") - body.count("</div"))
    return ("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width'>"
            "<style>" + jo.site_css(platform) + "</style></head><body>"
            "<header class='top'><div class='topinner'><a class='brand' href='/'>LiquidGov</a></div></header>"
            "<main class='wrap'>" + body + "</main><footer class='site'>footer</footer></body></html>"), h


def make_handler(html):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(html.encode())

    return H


PROBE = r"""(() => {
  const r = e => { const b = e.getBoundingClientRect(); return {l: b.left, t: b.top, r: b.right, b: b.bottom}; };
  const list = document.getElementById('msglist'), comp = document.getElementById('mailcomposer');
  const inbox = document.querySelector('.mailsection'), th = document.querySelector('.mailthread');
  const bubbles = [...document.querySelectorAll('.msg .bubble')].map(r);
  return {W: innerWidth, H: innerHeight, sw: document.documentElement.scrollWidth,
          listScroll: list.scrollHeight - list.clientHeight, listOverflow: getComputedStyle(list).overflowY,
          list: r(list), comp: r(comp), inbox: r(inbox), thread: r(th),
          worst: bubbles.reduce((m, b) => Math.max(m, b.r), 0)};
})()"""


class Run:
    def __init__(self, base):
        self.base, self.fails, self.n = base, [], 0

    def check(self, name, ok, detail):
        self.n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + json.dumps(detail)[:500]), flush=True)
        if not ok:
            self.fails.append(name)

    def j(self, expr):
        v = cdp.js(self.ws, "JSON.stringify(%s)" % expr)
        try:
            return json.loads(v)
        except (TypeError, ValueError):  # err-ok: kept raw; the check on it then fails and prints it
            return {"raw": v}

    def cases(self, handler):
        top = top_html(handler)
        self.check("M1 the heading is Messages, not Mail", "<h1>Messages</h1>" in top and "<h1>Mail</h1>" not in top, top[:200])
        for w, h in ((1280, 900), (768, 1000), (390, 844)):
            self.ws.call("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=1, mobile=w < 500)
            self.ws.call("Page.navigate", url=self.base + "/mail")
            time.sleep(1.2)
            if w == 1280:
                note = self.j(r"""(() => { const d = document.querySelector('main details'), s = d && d.querySelector('summary');
                  const p = d && d.parentElement; const lh = p ? parseFloat(getComputedStyle(p).lineHeight) || 24 : 24;
                  return {has: !!d, summary: s ? s.textContent.trim() : '', open: d ? d.open : null,
                          visible: document.querySelector('main').innerText.includes('We will not treat a checkbox'),
                          h: p ? p.getBoundingClientRect().height : 0, lh: lh}; })()""")
                self.check("M2 the age note is one line with a closed \"more\"", isinstance(note, dict) and note.get("has")
                           and note.get("summary") == "more" and note.get("open") is False and not note.get("visible")
                           and note.get("h", 99) <= note.get("lh", 0) * 1.6, note)
            cdp.js(self.ws, "(() => { const t = document.querySelector('.mailthread'); window.scrollTo(0, t.getBoundingClientRect().top + scrollY - 80); return 1; })()")
            time.sleep(0.4)
            p = self.j(PROBE)
            if w == 1280:
                side = isinstance(p, dict) and (p["inbox"]["r"] <= p["thread"]["l"] + 1 or p["thread"]["r"] <= p["inbox"]["l"] + 1) \
                    and p["inbox"]["t"] < p["thread"]["b"] and p["thread"]["t"] < p["inbox"]["b"]
                self.check("M3 1280: the conversation list is beside the thread", side, p)
            self.check("M4 %d: the transcript has no scroll region of its own" % w, isinstance(p, dict) and p["listScroll"] <= 1
                       and p["listOverflow"] not in ("auto", "scroll"), p)
            self.check("M5 %d: at the top of the thread the composer is on screen" % w, isinstance(p, dict)
                       and p["comp"]["t"] >= 0 and p["comp"]["b"] <= p["H"] + 1, p)
            self.check("M6 %d: no bubble is cut off and no sideways scroll" % w, isinstance(p, dict)
                       and p["worst"] <= min(p["list"]["r"], p["W"]) + 1 and p["sw"] <= p["W"], p)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    html, handler = page(platform)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), make_handler(html))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    r = Run("http://127.0.0.1:%d" % srv.server_address[1])
    proc = None
    try:
        proc, r.ws = cdp.launch_edge("lg-messenger-")
        for d in ("Page.enable", "Runtime.enable"):
            r.ws.call(d)
        r.cases(handler)
    except Exception as e:  # a crash mid-run is a failed run, said with its traceback, never a quiet pass
        traceback.print_exc()
        r.check("run finished without an exception", False, "%s: %s" % (type(e).__name__, e))
    finally:
        if proc is not None:
            cdp.stop_edge(proc, r.ws.profile if r.ws else None)
        srv.shutdown()
        print("VERDICT messenger-layout: %s  (%d checks)" % ("ALL-PASS" if not r.fails else "FAILS: " + ", ".join(r.fails), r.n),
              flush=True)
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
