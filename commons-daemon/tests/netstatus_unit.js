// Unit cases for the client's "Reconnecting…" badge (commons/js/net.js, the block between the `netStatus:begin` and `netStatus:end`
// markers). The SHIPPED code is extracted and run in a vm with a stub DOM element, so this tests what players load, not a copy.
//   node netstatus_unit.js <path to commons/js/net.js>
// Prints 'netstatus_unit: N checks, F failed'; exit 1 if any failed.
"use strict";
const fs = require("fs");
const vm = require("vm");

const src = fs.readFileSync(process.argv[2], "utf8");
const m = src.match(/\/\/ ---- netStatus:begin[^\n]*\n([\s\S]*?)\/\/ ---- netStatus:end/);
if (!m) { console.log("FATAL: the netStatus:begin/netStatus:end markers are missing from " + process.argv[2]); process.exit(2); }
const CODE = m[1];

let checks = 0, failed = 0;
function check(ok, what) { checks++; if (!ok) { failed++; console.log("  FAIL " + what); } }

// A fresh world: the block, a stub element that records every write, and `netMintPending` as the page has it.
function makeEnv(opts) {
  const el = { text: "", writes: 0 };
  const node = new Proxy(el, { set: (t, k, v) => { if (k === "textContent") { t.text = v; t.writes++; } return true; }, get: (t, k) => k === "textContent" ? t.text : t[k] });
  const env = { netMintPending: null, document: { getElementById: (id) => (opts && opts.noElement) ? null : (id === "connStatus" ? node : null) } };
  vm.createContext(env); vm.runInContext(CODE, env);
  return {
    env, el,
    tick: (t) => vm.runInContext("netStatusTick(" + t + ")", env),
    report: (ok) => vm.runInContext("netStatusReport(" + ok + ")", env),
    setPending: (v) => { env.netMintPending = v ? {} : null; },
    shown: () => el.text === "Reconnecting…",
    get(name) { return vm.runInContext(name, env); },
  };
}
const H = "Reconnecting…";

// ---- the block itself
check(/NET_STATUS_MINT_MS = 4000/.test(CODE) && /NET_STATUS_FAILS = 6/.test(CODE), "the thresholds are 4000ms and 6 failures");
check(!/\bfetch\s*\(|XMLHttpRequest|sendBeacon|setTimeout|setInterval|new WebSocket/.test(CODE), "OBSERVE-ONLY: the block sends no request and starts no timer");
{ const t = makeEnv(); check(t.get("NET_STATUS_MINT_MS") === 4000 && t.get("NET_STATUS_FAILS") === 6, "the constants load as documented"); }

// ---- the ordinary path shows nothing at all
{
  const t = makeEnv();
  for (let i = 0; i < 4000; i++) t.tick(i * 16);                        // a minute of frames, connected
  check(t.el.text === "" && t.el.writes === 0, "an ordinary session never shows the badge and never writes to the DOM");
  for (let i = 0; i < 200; i++) { t.report(true); t.tick(70000 + i * 16); }
  check(t.el.writes === 0, "successful heartbeats change nothing");
}

// ---- no credential for more than 4s (startup mint, or a remint after a 403)
{
  const t = makeEnv();
  t.setPending(true); t.tick(1000);                                     // first seen pending at t=1000
  t.tick(4999); check(!t.shown(), "a mint pending for 3999ms: not shown");
  t.tick(5000); check(!t.shown(), "exactly 4000ms: not shown (the threshold is 'more than')");
  t.tick(5001); check(t.shown() && t.el.text === H, "4001ms: shown, with the text \"Reconnecting…\" (a real ellipsis)");
  t.tick(9000); t.tick(20000); check(t.el.writes === 1, "and while it stays shown nothing is rewritten (one DOM write so far)");
  t.setPending(false); t.tick(20016);
  check(!t.shown() && t.el.text === "", "the credential arrives: hidden on the very next tick, and the element is EMPTY (so CSS :empty hides it)");
  check(t.el.writes === 2, "exactly two DOM writes for the whole episode (show, hide)");
}
{
  const t = makeEnv();
  t.setPending(true); t.tick(0); t.tick(3900); t.setPending(false); t.tick(3916);
  check(t.el.writes === 0, "a mint that resolves at 3.9s never shows the badge");
  t.setPending(true); t.tick(10000); t.tick(13900);
  check(!t.shown(), "a LATER remint gets its own fresh 4s (the timer restarts when a new mint loop is first seen)");
  t.tick(14001); check(t.shown(), "...and shows after that remint has been pending more than 4s");
}
{
  const t = makeEnv();
  t.setPending(true); t.tick(50000);                                     // the first frame we see it is late (a background tab)
  t.tick(50001); check(!t.shown(), "the 4s counts from when the mint was first SEEN pending, not from page load");
}

// ---- 6 consecutive failed heartbeats
{
  const t = makeEnv();
  for (let i = 1; i <= 5; i++) { t.report(false); t.tick(i * 500); }
  check(!t.shown() && t.get("netStatusFails") === 5, "5 failed heartbeats: not shown");
  t.report(false); t.tick(3000); check(t.shown(), "the 6th consecutive failure: shown");
  t.report(false); t.tick(3500); check(t.shown() && t.el.writes === 1, "more failures: still shown, no rewrite");
  t.report(true); t.tick(3516);
  check(!t.shown() && t.el.text === "" && t.get("netStatusFails") === 0, "the next success hides it on the next tick and resets the count");
}
{
  const t = makeEnv();
  for (let r = 0; r < 20; r++) { for (let i = 0; i < 5; i++) { t.report(false); t.tick(r * 10000 + i * 500); } t.report(true); t.tick(r * 10000 + 5000); }
  check(t.el.writes === 0, "failures that are never 6 in a row (5 bad, 1 good, repeat) never show it");
}
{
  const t = makeEnv();                                                    // an ordinary daemon restart: about a second of failures, then the 403 remint
  t.report(false); t.tick(0); t.report(false); t.tick(500); t.report(true); t.tick(1000);
  check(t.el.writes === 0, "a restart that recovers in ~1s (2 failures, then the daemon answers) never shows it");
}
{
  const t = makeEnv();
  t.report(false); t.tick(0);
  for (let i = 1; i <= 5; i++) { t.report(false); t.tick(i * 500); }
  check(t.shown(), "six failures at the 2Hz cadence (0, 0.5, ... 2.5s) show it at the 6th, about 3s after the last success");
  t.env.netStatusFails = 1e9; t.tick(4000);
  check(t.shown(), "a huge failure count stays shown");
  t.report(true); t.tick(4016); check(!t.shown(), "and one success still clears it");
}

// ---- both conditions together, and hiding only when both are clear
{
  const t = makeEnv();
  t.setPending(true); t.tick(0); t.tick(4100);
  for (let i = 0; i < 6; i++) t.report(false);
  t.tick(4200); check(t.shown(), "both problems: shown");
  t.setPending(false); t.tick(4300); check(t.shown(), "the credential arrives but 6 heartbeats have failed: still shown");
  t.report(true); t.tick(4400); check(!t.shown(), "a success clears the last cause: hidden");
  t.report(false); t.tick(4500); check(!t.shown(), "one new failure after that is not enough");
}

// ---- robustness
{
  const t = makeEnv({ noElement: true });
  t.setPending(true); t.tick(0); t.tick(5000);
  check(t.get("netStatusShown") === true, "with no #connStatus element (an old cached page) it does not throw and still tracks state");
}
{
  const t = makeEnv();
  let threw = false;
  try { for (let i = 0; i < 1000; i++) { t.report(i % 7 === 0); t.setPending(i % 11 === 0); t.tick(i * 137); } } catch (e) { threw = true; }
  check(!threw, "a noisy mix of reports, pending flips and irregular frame times never throws");
  check(typeof t.get("netStatusShown") === "boolean", "the shown flag is always a boolean");
}

console.log("netstatus_unit: " + checks + " checks, " + failed + " failed");
process.exit(failed ? 1 : 0);
