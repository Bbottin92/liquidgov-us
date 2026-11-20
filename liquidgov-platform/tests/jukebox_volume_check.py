"""jukebox_volume_check.py <liquidgov-platform dir>: the Juketube volume slider (assets/jukebox.js) on a phone that will
not let a page set volume. Brandon's phone check 2026-11-20 (iPhone 14, Safari): "nope doesnt work at all". iPhone and
iPad ignore a page's volume (only the hardware buttons set it), so the slider moved and nothing changed. Now, where a
media element will not take a volume, the slider is hidden and a note says to use the phone's buttons; Mute stays.

Runs the real script under node with fake page elements (as jukebox_ended_check.py does) and a fake <audio> whose volume
either sticks (a desktop browser) or stays 1 whatever is set (iPhone):
  V1  desktop: the slider stays visible and no note is added
  V2  iPhone: the slider is hidden
  V3  iPhone: a note right before it says to use the phone's buttons
Fails when node is missing (a check that cannot run must not pass), the script does not load, or a case differs.
Last line: VERDICT jukebox-volume: ALL-PASS (n checks) or the failures. Usage: python jukebox_volume_check.py [platform dir]
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HARNESS = r"""
const fs = require('fs'), vm = require('vm');
const src = fs.readFileSync(process.argv[2], 'utf8');
const ios = process.argv[3] === 'ios';
const els = {};
function el(id) {
  const h = {};
  const e = { id: id || '', style: {}, dataset: {}, children: [], hidden: false, value: '70', textContent: '', innerHTML: '', className: '',
    classList: { add() {}, remove() {}, toggle() {}, contains() { return false; } }, selectedOptions: [], inserted: [],
    appendChild(c) { return c; }, insertBefore(c, ref) { this.inserted.push({ node: c, before: ref && ref.id }); return c; },
    setAttribute() {}, removeAttribute() {},
    querySelectorAll() { return []; }, querySelector() { return null; }, closest() { return null; },
    getBoundingClientRect() { return { top: 0, height: 0 }; }, setPointerCapture() {}, focus() {},
    addEventListener(t, f) { (h[t] = h[t] || []).push(f); }, firstChild: null, nextSibling: null };
  return e;
}
function byId(id) { if (!els[id]) { els[id] = el(id); } return els[id]; }
const row = el('row');
byId('jbvolume').parentNode = row;
function audio() {   // a desktop browser keeps the volume it is given; iPhone Safari leaves it at 1
  let v = 1;
  return { get volume() { return v; }, set volume(x) { if (!ios) v = x; } };
}
const document = { getElementById: byId, createElement: (t) => t === 'audio' ? audio() : el(), addEventListener() {}, head: el(), body: el(),
  hidden: false, querySelector: () => null, querySelectorAll: () => [] };
function Player(id, opts) { return { setVolume() {}, getDuration() { return 0; }, getCurrentTime() { return 0; }, getPlayerState() { return -1; },
  getVideoData() { return { video_id: '' }; }, loadVideoById() {}, seekTo() {}, playVideo() {}, stopVideo() {}, pauseVideo() {}, mute() {},
  unMute() {}, destroy() {} }; }
const PlayerState = { ENDED: 0, PLAYING: 1, PAUSED: 2, BUFFERING: 3, CUED: 5 };
function WS() { this.readyState = 1; this.send = () => {}; this.close = () => {}; }
const window = { YT: { Player, PlayerState }, document, addEventListener() {}, location: { protocol: 'https:', host: 'x.test' } };
const ctx = { window, document, YT: window.YT, WebSocket: WS, Date, console, Number, JSON, Math, isFinite, Array, Object,
  setInterval: () => 0, clearInterval() {}, setTimeout: () => 0, clearTimeout() {}, fetch: () => Promise.resolve({ ok: true, json: () => ({}) }),
  navigator: {}, localStorage: { getItem() { return null; }, setItem() {} }, location: window.location,
  LG_JUKEBOX: { standalone: true, ownUsername: '', stations: [{ id: '', name: 'Site-wide' }] }, URL, encodeURIComponent, DOMParser: function () {} };
ctx.window.LG_JUKEBOX = ctx.LG_JUKEBOX;
vm.createContext(ctx);
vm.runInContext(src, ctx);
const slider = els['jbvolume'];
const notes = row.inserted.filter(i => i.before === 'jbvolume').map(i => i.node.textContent);
console.log(JSON.stringify({ hidden: !!slider.hidden, notes }));
"""


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src = os.path.join(platform, "assets", "jukebox.js")
    fails, n = [], 0

    def check(name, ok, detail=""):
        nonlocal n
        n += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]), flush=True)
        if not ok:
            fails.append(name)

    node = shutil.which("node")
    check("V0 node is installed and assets/jukebox.js is there", bool(node) and os.path.exists(src), (node, src))
    if fails:
        print("VERDICT jukebox-volume: FAILS: " + ", ".join(fails) + "  (%d checks)" % n)
        return 1
    tmp = tempfile.mkdtemp(prefix="jbvol-")
    try:
        js = os.path.join(tmp, "h.js")
        open(js, "w", encoding="utf-8").write(HARNESS)
        out = {}
        for mode in ("desktop", "ios"):
            r = subprocess.run([node, js, src, mode], capture_output=True, text=True, timeout=60)
            try:
                out[mode] = json.loads(r.stdout.strip().splitlines()[-1])
            except (ValueError, IndexError):
                out[mode] = {"error": " ".join((r.stderr or r.stdout).split())[-300:]}
        d, i = out["desktop"], out["ios"]
        check("V1 desktop: the volume slider stays visible and no note is added", d.get("hidden") is False and d.get("notes") == [], d)
        check("V2 iPhone (volume cannot be set): the slider is hidden", i.get("hidden") is True, i)
        check("V3 iPhone: a note before it says to use the phone's buttons",
              any("phone" in (t or "") and "button" in (t or "") for t in i.get("notes") or []), i)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("VERDICT jukebox-volume: %s  (%d checks)" % ("ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), n))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
