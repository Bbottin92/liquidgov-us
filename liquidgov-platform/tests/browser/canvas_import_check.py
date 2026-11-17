"""The canvas's "Import image/video" control (Brandon 2026-11-16: the upload on the profile canvas "does not seem to work";
his canvas visits in the live log are all iPhone Safari 18.5, where nothing shows when a picked file cannot be read).

Real headless Edge against a running test server (start one with tests/browser/phone_serve.sh; it prints the sessions
file next to the store). The page must load assets/canvas.js (the build from 2026-11-13 on). Cases:
  P0  precondition: /me/canvas has #cimport and loads assets/canvas.js (a check that finds nothing passes on nothing)
  I1  right after a picture is chosen the file input still holds it (the old code emptied it at once; iPhone Safari can
      lose the file when the input is cleared before it has been read)
  I2  the picture is drawn on layer 1, and only then is the input emptied
  V1  a video whose browser never sends "loadeddata" (as iPhone Safari may not) still gets its frame drawn
  V2  a file that is not a video says so, in words, and empties the input (no silent nothing)
Usage: python canvas_import_check.py <base url, e.g. http://127.0.0.1:18140> <session id of any signed-in person>
Prints "VERDICT canvas-import: ALL-PASS" or the failed cases. Exit 0 all pass, 1 a case failed, 2 could not run.
New file; committed checks unchanged. Run it on the old assets/canvas.js too: I1 and V1 must FAIL there."""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdp  # noqa: E402

PICTURE_JS = """(async function(){
  window.__fetches=[];var of=window.fetch;window.fetch=function(u,o){__fetches.push(String(u));return of(u,o);};
  var c=document.createElement('canvas');c.width=300;c.height=200;var x=c.getContext('2d');
  x.fillStyle='#c33';x.fillRect(0,0,300,200);x.fillStyle='#39c';x.fillRect(50,50,100,100);
  var b=await new Promise(function(r){c.toBlob(r,'image/png')});
  var dt=new DataTransfer();dt.items.add(new File([b],'pic.png',{type:'image/png'}));
  var i=document.getElementById('cimport');i.files=dt.files;i.dispatchEvent(new Event('change',{bubbles:true}));
  return i.files.length;})()"""

# A real video, made in the page (MediaRecorder -> webm), handed to the picker. If hide_loadeddata, the page's videos
# never receive a "loadeddata" listener, which is what a browser that does not send the event looks like.
VIDEO_JS = """(async function(hide){
  if(hide){var orig=HTMLVideoElement.prototype.addEventListener;
    HTMLVideoElement.prototype.addEventListener=function(t,f,o){if(t==='loadeddata')return;return orig.call(this,t,f,o);};}
  var c=document.createElement('canvas');c.width=320;c.height=240;var x=c.getContext('2d');
  var rec=new MediaRecorder(c.captureStream(15),{mimeType:'video/webm'});var parts=[];
  rec.ondataavailable=function(e){parts.push(e.data)};var done=new Promise(function(r){rec.onstop=r});rec.start();
  for(var i=0;i<12;i++){x.fillStyle='hsl('+(i*30)+',80%,50%)';x.fillRect(0,0,320,240);await new Promise(function(r){setTimeout(r,80)});}
  rec.stop();await done;
  var dt=new DataTransfer();dt.items.add(new File(parts,'clip.webm',{type:'video/webm'}));
  var inp=document.getElementById('cimport');inp.files=dt.files;inp.dispatchEvent(new Event('change',{bubbles:true}));
  return inp.files.length;})"""

NOT_A_VIDEO_JS = """(function(){var dt=new DataTransfer();dt.items.add(new File(['this is text'],'notes.webm',{type:'video/webm'}));
  var inp=document.getElementById('cimport');inp.files=dt.files;inp.dispatchEvent(new Event('change',{bubbles:true}));return inp.files.length;})()"""

PAINTED_JS = """(function(){var c=document.getElementById('clayer0');var d=c.getContext('2d').getImageData(0,0,c.width,c.height).data;
  var n=0;for(var i=3;i<d.length;i+=4)if(d[i])n++;return n;})()"""
STATUS_JS = "document.getElementById('cstatus').textContent"
FILES_JS = "document.getElementById('cimport').files.length"
# the page reloads the last saved drawing onto layer 1: empty it first, so "painted" can only come from the import
CLEAR_JS = "(function(){var c=document.getElementById('clayer0');c.getContext('2d').clearRect(0,0,c.width,c.height);return 1;})()"

results = []


def case(name, ok, detail=""):
    results.append((name, ok))
    print(("PASS " if ok else "FAIL ") + name + ((" (" + str(detail) + ")") if detail != "" else ""), flush=True)


def wait_for(ws, expr, want, seconds):
    end = time.time() + seconds
    v = cdp.js(ws, expr)
    while time.time() < end and not want(v):
        time.sleep(0.25)
        v = cdp.js(ws, expr)
    return v


def fresh_page(ws, base, session):
    ws.call("Network.setCookie", name="lg_session", value=session, domain=base.split("//")[1].split(":")[0], path="/")
    cdp.js(ws, "window.__oldpage=1")  # still set after the navigation = we are looking at the page we left
    ws.call("Page.navigate", url=base + "/me/canvas")
    for _ in range(40):
        time.sleep(0.25)
        if cdp.js(ws, "!window.__oldpage&&!!document.getElementById('cimport')&&typeof window.LG_CANVAS==='object'"):
            return True
    return False


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    base, session = argv[1].rstrip("/"), argv[2]
    proc, ws = cdp.launch_edge("lg-import-edge-")
    try:
        for m in ("Page.enable", "Runtime.enable", "Network.enable"):
            ws.call(m)
        if not fresh_page(ws, base, session):
            print("RUN_FAIL /me/canvas never showed #cimport (wrong server, or not signed in)")
            return 2
        src_ok = cdp.js(ws, "Array.prototype.some.call(document.scripts,function(s){return /\\/assets\\/canvas\\.js/.test(s.src)})")
        case("P0 /me/canvas has #cimport and loads assets/canvas.js", bool(src_ok), src_ok)
        if not src_ok:
            return 2

        cdp.js(ws, CLEAR_JS)
        held = cdp.js(ws, PICTURE_JS)
        case("I1 the input still holds the chosen picture right after the choice", held == 1, "files.length=%r" % held)
        painted = wait_for(ws, PAINTED_JS, lambda v: isinstance(v, int) and v > 0, 8)
        case("I2a the picture is drawn on layer 1", isinstance(painted, int) and painted > 0, "painted=%r" % painted)
        left = wait_for(ws, FILES_JS, lambda v: v == 0, 4)
        case("I2b ...and then the input is emptied", left == 0, "files.length=%r" % left)

        fresh_page(ws, base, session)
        cdp.js(ws, CLEAR_JS)
        cdp.js(ws, VIDEO_JS + "(true)")
        painted = wait_for(ws, PAINTED_JS, lambda v: isinstance(v, int) and v > 0, 9)
        case("V1 a video whose browser never sends loadeddata still gets its frame drawn", isinstance(painted, int) and painted > 0,
             "painted=%r status=%r" % (painted, cdp.js(ws, STATUS_JS)))

        fresh_page(ws, base, session)
        cdp.js(ws, NOT_A_VIDEO_JS)
        status = wait_for(ws, STATUS_JS, lambda v: isinstance(v, str) and v != "" and "Reading" not in v, 13)
        left = cdp.js(ws, FILES_JS)
        case("V2 a file that is not a video says so in words", isinstance(status, str) and status != "" and "Reading" not in status, repr(status))
        case("V2b ...and the input is emptied", left == 0, "files.length=%r" % left)
    finally:
        cdp.stop_edge(proc, ws.profile)
    failed = [n for n, ok in results if not ok]
    print("VERDICT canvas-import: " + ("ALL-PASS" if not failed else "FAILED " + "; ".join(failed)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
