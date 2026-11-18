"""jukebox_ended_check.py <liquidgov-platform dir>: the Juketube widget (assets/jukebox.js) reports a track as ENDED to the
server exactly when the video really ended, including a very short clip (q188, found live 2026-11-18: a 0.82 s clip on
Site-wide ended 2.5 s after it loaded, the widget ignored the end because it came inside a 4 s "stale ended" window, the
server never advanced and kept the clip as "now playing" with elapsed 80+ s, and every joiner replayed it from 0).

It runs the real script under node with a fake YouTube player, fake WebSocket and a controllable clock, then checks what
the widget sent. Cases: a clip shorter than the old 4 s window; a short clip that ends before 3 s of play time; a long
video's genuine end; and the two stale-end cases the old windows exist for (an ENDED right after loading a long video; an
ENDED while the duration is still unknown) which must NOT be reported.

Fails when: node is missing (a check that cannot run must not pass), the script does not load, or any case differs.
Last line: VERDICT jukebox-ended: ALL-PASS (n checks) or the failures. Usage: python jukebox_ended_check.py [platform dir]
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
const scenario = JSON.parse(process.argv[3]);
let clock = 1000000;
const RealDate = Date;
function FakeDate(...a) { return new RealDate(...(a.length ? a : [clock])); }
FakeDate.now = () => clock;
function el() {
  const h = {};
  const e = { style: {}, dataset: {}, children: [], hidden: false, value: '70', textContent: '', innerHTML: '', className: '',
    classList: { add() {}, remove() {}, toggle() {}, contains() { return false; } }, selectedOptions: [],
    appendChild(c) { return c; }, insertBefore(c) { return c; }, setAttribute() {}, removeAttribute() {},
    querySelectorAll() { return []; }, querySelector() { return null; }, closest() { return null; },
    getBoundingClientRect() { return { top: 0, height: 0 }; }, setPointerCapture() {}, focus() {},
    addEventListener(t, f) { (h[t] = h[t] || []).push(f); }, firstChild: null, nextSibling: null };
  return e;
}
const sent = [];
let player = null, ytEvents = null;
const loads = [];
function Player(id, opts) {
  ytEvents = opts.events;
  player = { dur: 0, cur: 0, state: -1, vid: '',
    setVolume() {}, getDuration() { return this.dur; }, getCurrentTime() { return this.cur; },
    getPlayerState() { return this.state; }, getVideoData() { return { video_id: this.vid }; },
    loadVideoById(o) { loads.push(o); this.vid = o.videoId; }, seekTo(t) { this.seeked = (this.seeked || 0) + 1; this.cur = t; },
    playVideo() { this.played = (this.played || 0) + 1; }, stopVideo() {}, pauseVideo() {}, mute() {}, unMute() {}, destroy() {} };
  return player;
}
const PlayerState = { ENDED: 0, PLAYING: 1, PAUSED: 2, BUFFERING: 3, CUED: 5 };
let ws = null;
function WS(url) { ws = this; this.readyState = 1; this.url = url; this.send = (m) => sent.push(JSON.parse(m)); this.close = () => {}; }
const document = { getElementById: () => el(), createElement: () => el(), addEventListener() {}, head: el(), body: el(), hidden: false,
  querySelector: () => null, querySelectorAll: () => [] };
const window = { YT: { Player, PlayerState }, document, addEventListener() {}, location: { protocol: 'https:', host: 'x.test' } };
const ctx = { window, document, YT: window.YT, WebSocket: WS, Date: FakeDate, console, Number, JSON, Math, isFinite, Array, Object,
  setInterval: () => 0, clearInterval() {}, setTimeout: () => 0, clearTimeout() {}, fetch: () => Promise.resolve({ ok: true, json: () => ({}) }),
  navigator: {}, localStorage: { getItem() { return null; }, setItem() {} }, location: window.location,
  LG_JUKEBOX: { standalone: true, ownUsername: '', stations: [{ id: '', name: 'Site-wide' }] }, URL, encodeURIComponent, DOMParser: function () {} };
ctx.window.LG_JUKEBOX = ctx.LG_JUKEBOX;
vm.createContext(ctx);
vm.runInContext(src, ctx);
if (!ytEvents) throw new Error('the widget never created a player');
ytEvents.onReady();
ws.onmessage({ data: JSON.stringify({ now: { video_id: scenario.id, queued_by: 't', elapsed: scenario.elapsed || 0 }, listeners: 1, upcoming: 0, queue: [] }) });
if (!loads.length) throw new Error('the widget did not load the video');
player.dur = scenario.dur; player.cur = scenario.cur; player.state = 0;
clock += scenario.afterMs;
ytEvents.onStateChange({ data: 0 });
console.log(JSON.stringify({ sent, seeked: player.seeked || 0 }));
"""


def run_case(node, js_path, src_path, scenario):
    r = subprocess.run([node, js_path, src_path, json.dumps(scenario)], capture_output=True, text=True, timeout=60)
    if r.returncode != 0:
        lines = [l.strip() for l in (r.stderr or r.stdout).splitlines() if "Error" in l and len(l) < 300]
        return None, "; ".join(lines[:2]) or " ".join((r.stderr or r.stdout).split())[-300:]
    return json.loads(r.stdout.strip().splitlines()[-1]), ""


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    checks, fails = 0, []

    def check(ok, what, detail=""):
        nonlocal checks
        checks += 1
        print(("PASS " if ok else "FAIL ") + what + ("" if ok or not detail else "  (%s)" % detail))
        if not ok:
            fails.append(what)

    node = shutil.which("node")
    check(node is not None, "node is installed (needed to run the widget script)")
    src = os.path.join(platform, "assets", "jukebox.js")
    check(os.path.isfile(src), "assets/jukebox.js exists", src)
    if node and os.path.isfile(src):
        with tempfile.TemporaryDirectory() as td:
            js = os.path.join(td, "harness.js")
            with open(js, "w", encoding="utf-8") as f:
                f.write(HARNESS)
            ended = lambda vid: [{"ended": vid}]
            cases = [
                # (label, scenario, expected messages sent to the server)
                ("0.8 s clip ending 2.5 s after load is reported ended (the live bug)",
                 dict(id="clip0", dur=0.821, cur=0.821, afterMs=2500), ended("clip0")),
                ("2 s clip ending 2.2 s after load is reported ended",
                 dict(id="clip2", dur=2.0, cur=2.0, afterMs=2200), ended("clip2")),
                ("6 s clip ending 6.5 s after load is reported ended",
                 dict(id="clip6", dur=6.0, cur=6.0, afterMs=6500), ended("clip6")),
                ("a long video's real end is reported ended",
                 dict(id="long1", dur=212.0, cur=211.8, afterMs=212000), ended("long1")),
                ("a stale ENDED just after loading a long video is NOT reported",
                 dict(id="long2", dur=212.0, cur=0.1, afterMs=800), []),
                ("an ENDED while the duration is still unknown (0) is NOT reported",
                 dict(id="unk", dur=0, cur=0, afterMs=1500), []),
            ]
            for label, scenario, expected in cases:
                out, err = run_case(node, js, src, scenario)
                check(out is not None and out["sent"] == expected, label,
                      err or "sent %s, expected %s" % (out and out["sent"], expected))
    print("VERDICT jukebox-ended: %s" % ("ALL-PASS (%d checks)" % checks if not fails else "FAIL (%d of %d: %s)" % (
        len(fails), checks, "; ".join(fails))))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
