"""Earlier self-portraits on /me/portrait (Brandon 2026-11-16: "save and load prior self portraits"), in a real browser.
Real headless Edge against a running test server built from this change (tests/browser/phone_serve.sh starts one and
writes the sessions file next to its store). The person is given three flat-colour portraits in turn, then in the browser:
  H0  precondition: the page lists Current + 2 earlier portraits as buttons (a check that finds none passes on nothing)
  H1  clicking the oldest earlier portrait loads it into the canvas (centre pixel has that portrait's colour)
  H2  clicking Save as self-portrait makes it the current portrait (the served /portrait/<user> has that colour)
  H3  the portrait that was current moved into the earlier list (still 2 earlier portraits; the newest is its colour)
  H4  a stranger's browser does not get the owner-only picture (404 for /me/portrait/old/0 as someone else)
Usage: python portrait_history_check.py <base url, e.g. http://127.0.0.1:18140> <owner session id> <owner name> <other session id>
Prints "VERDICT portrait-history: ALL-PASS" or the failed cases. Exit 0 all pass, 1 a case failed, 2 could not run.
New file; committed checks unchanged."""
import os
import struct
import sys
import time
import urllib.error
import urllib.request
import uuid
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402

COLORS = [(200, 30, 30), (30, 200, 30), (30, 30, 200)]  # red, green, blue: saved in this order


def png(rgb, w=64, h=64):
    raw = b"".join(b"\x00" + bytes(rgb) * w for _ in range(h))

    def ch(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw)) + ch(b"IEND", b"")


def save_portrait(base, session, data):
    b = uuid.uuid4().hex
    body = (("--%s\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"p.png\"\r\nContent-Type: image/png\r\n\r\n" % b).encode()
            + data + ("\r\n--%s--\r\n" % b).encode())
    req = urllib.request.Request(base + "/me/avatar", data=body, method="POST", headers={
        "Content-Type": "multipart/form-data; boundary=" + b, "Cookie": "lg_session=" + session})
    urllib.request.urlopen(req, timeout=20).read()


def status_of(url, session):
    req = urllib.request.Request(url, headers={"Cookie": "lg_session=" + session})
    try:
        return urllib.request.urlopen(req, timeout=20).status
    except urllib.error.HTTPError as e:
        return e.code


# the colour at the middle of the picture at `src`, read back through a canvas (same origin, so it is not tainted)
CENTRE_JS = """(function(src){return new Promise(function(res){var i=new Image();i.onload=function(){
  var c=document.createElement('canvas');c.width=i.naturalWidth;c.height=i.naturalHeight;var x=c.getContext('2d');x.drawImage(i,0,0);
  var d=x.getImageData(c.width>>1,c.height>>1,1,1).data;res([d[0],d[1],d[2]]);};i.onerror=function(){res(null)};i.src=src;});})"""
LAYER_CENTRE_JS = """(function(){var c=document.getElementById('clayer0');var d=c.getContext('2d').getImageData(c.width>>1,c.height>>1,1,1).data;return [d[0],d[1],d[2],d[3]];})()"""

results = []


def case(name, ok, detail=""):
    results.append((name, ok))
    print(("PASS " if ok else "FAIL ") + name + ((" (" + str(detail) + ")") if detail != "" else ""), flush=True)


def near(a, b, tol=6):
    return a is not None and b is not None and all(abs(int(x) - int(y)) <= tol for x, y in zip(a[:3], b[:3]))


def wait_page(ws, url, ready_js):
    cdp.js(ws, "window.__oldpage=1")
    ws.call("Page.navigate", url=url)
    for _ in range(60):
        time.sleep(0.25)
        if cdp.js(ws, "!window.__oldpage&&(" + ready_js + ")"):
            return True
    return False


def main(argv):
    if len(argv) != 5:
        print(__doc__)
        return 2
    base, session, owner, other = argv[1].rstrip("/"), argv[2], argv[3], argv[4]
    for rgb in COLORS:
        save_portrait(base, session, png(rgb))
    proc, ws = cdp.launch_edge("lg-portrait-edge-")
    try:
        for m in ("Page.enable", "Runtime.enable", "Network.enable"):
            ws.call(m)
        ws.call("Network.setCookie", name="lg_session", value=session, domain=base.split("//")[1].split(":")[0], path="/")
        if not wait_page(ws, base + "/me/portrait", "!!document.getElementById('csave')&&typeof window.LG_CANVAS==='object'"):
            print("RUN_FAIL /me/portrait never showed the canvas (wrong server, or not signed in)")
            return 2
        srcs = cdp.js(ws, "Array.prototype.map.call(document.querySelectorAll('.oldportrait'),function(b){return b.dataset.src})")
        case("H0 the page lists Current + 2 earlier portraits as buttons", isinstance(srcs, list) and len(srcs) == 3, srcs)
        if not (isinstance(srcs, list) and len(srcs) == 3):
            return 2
        # buttons: Current, then earlier ones newest first -> srcs[1] = green (replaced by blue), srcs[2] = red (the oldest)
        cdp.js(ws, "document.querySelectorAll('.oldportrait')[2].click()")
        end = time.time() + 6
        got = None
        while time.time() < end:
            got = cdp.js(ws, LAYER_CENTRE_JS)
            if got and got[3] > 0:
                break
            time.sleep(0.25)
        case("H1 clicking the oldest earlier portrait loads it into the canvas (red)", near(got, COLORS[0]), got)
        cdp.js(ws, "window.__oldpage=1")
        cdp.js(ws, "document.getElementById('csave').click()")
        time.sleep(4)
        cur = cdp.js(ws, CENTRE_JS + "('/portrait/%s?x=%s')" % (owner, uuid.uuid4().hex[:6]))
        case("H2 Save as self-portrait makes it the current portrait (red)", near(cur, COLORS[0]), cur)
        wait_page(ws, base + "/me/portrait", "!!document.getElementById('csave')")
        srcs2 = cdp.js(ws, "Array.prototype.map.call(document.querySelectorAll('.oldportrait'),function(b){return b.dataset.src})")
        case("H3a one more earlier portrait than before (blue, the one it replaced, moved in): Current + 3", isinstance(srcs2, list) and len(srcs2) == 4, srcs2)
        newest = cdp.js(ws, CENTRE_JS + "(%r)" % (srcs2[1] if isinstance(srcs2, list) and len(srcs2) > 1 else ""))
        case("H3b the newest earlier portrait is the blue one that was current", near(newest, COLORS[2]), newest)
        code = status_of(base + "/me/portrait/old/0", other)
        case("H4 someone else (no earlier portraits of their own) gets no picture from the owner-only address", code == 404, code)
    finally:
        cdp.stop_edge(proc, ws.profile)
    failed = [n for n, ok in results if not ok]
    print("VERDICT portrait-history: " + ("ALL-PASS" if not failed else "FAILED " + "; ".join(failed)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
