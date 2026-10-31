// Unit cases for the client's guest-credential mint (commons/js/net.js, the block between the `netMint:begin` and
// `netMint:end` markers). The SHIPPED code is extracted from the page and run in a vm with a fake clock and a scripted
// fake fetch, so this tests what players actually load, not a copy.
//   node netmint_unit.js <path to commons/js/net.js>
// Prints 'netmint_unit: N checks, F failed'; exit 1 if any failed.
"use strict";
const fs = require("fs");
const vm = require("vm");

const html = fs.readFileSync(process.argv[2], "utf8");
const m = html.match(/\/\/ ---- netMint:begin[^\n]*\n([\s\S]*?)\/\/ ---- netMint:end/);
if (!m) { console.log("FATAL: the netMint:begin/netMint:end markers are missing from " + process.argv[2]); process.exit(2); }
const CODE = m[1];

let checks = 0, failed = 0;
function check(ok, what) { checks++; if (!ok) { failed++; console.log("  FAIL " + what); } }
const flush = () => new Promise((r) => setImmediate(r));         // let every pending promise reaction run

const GOOD = { id: "g_0123456789abcdef", secret: "0123456789abcdef0123456789abcdef" };
// scripted answers
const res = (status, body, headers) => () => Promise.resolve({
  ok: status >= 200 && status < 300, status,
  headers: { get: (k) => (headers && headers[k]) != null ? headers[k] : null },
  json: () => typeof body === "string" ? Promise.reject(new SyntaxError("Unexpected token < in JSON")) : Promise.resolve(body),
});
const ok200 = () => res(200, GOOD);
const html429 = (headers) => res(429, "<html>429 Too Many Requests</html>", headers);
const html502 = () => res(502, "<html>502 Bad Gateway</html>");
const netErr = () => () => Promise.reject(new TypeError("Failed to fetch"));

function makeEnv(script, rnd) {
  let now = 0, seq = 0;
  const timers = [], calls = [], warnings = [];
  const M = Object.create(Math); M.random = () => (rnd === undefined ? 0.5 : rnd);   // 0.5 = jitter factor exactly 1.0
  const env = {
    DAEMON: "https://x.test/commons", Math: M, Promise, Error, TypeError, SyntaxError, parseInt,
    netId: "old-id", netSecret: "old-secret", netReady: false, netRegion: "region", otherOccupants: [1, 2],
    console: { warn: (...a) => warnings.push(a.join(" ")), error: () => {}, log: () => {} },
    setTimeout: (fn, ms) => { timers.push({ at: now + ms, ms, fn, id: seq++ }); return seq; },
    fetch: (url, opts) => {
      calls.push({ url, opts, at: now });
      const step = script.length > 1 ? script.shift() : script[0];        // the last scripted answer repeats
      return step();
    },
  };
  vm.createContext(env);
  vm.runInContext(CODE, env);
  async function advance(ms) {                                            // move the fake clock, firing due timers in order
    const end = now + ms;
    await flush();
    for (;;) {
      timers.sort((a, b) => a.at - b.at || a.id - b.id);
      if (!timers.length || timers[0].at > end) break;
      const t = timers.shift(); now = t.at; t.fn(); await flush();
    }
    now = end; await flush();
  }
  return { env, timers, calls, warnings, advance, get now() { return now; }, delays: () => timers.map((t) => t.ms) };
}

(async () => {
  // ---- the ordinary case: a 200 with a credential
  {
    const t = makeEnv([ok200()]);
    const p = t.env.netMint(); await flush();
    check(t.env.netReady === true && t.env.netId === GOOD.id && t.env.netSecret === GOOD.secret, "a 200 with {id, secret} sets the credential and netReady");
    check(t.calls.length === 1 && t.calls[0].url === "https://x.test/commons/presence/guest" && t.calls[0].opts.method === "POST", "one POST to <daemon>/presence/guest");
    check(t.timers.length === 0, "and no retry is scheduled");
    let resolved = false; p.then(() => { resolved = true; }); await flush();
    check(resolved, "the returned promise resolves once the credential is in hand");
    check(t.env.netMintPending === null && t.env.netMintAttempt === 0, "the loop state is clean afterwards");
  }

  // ---- a failed answer is never read as a credential, and is retried (the point of the whole change)
  const bad = {
    "a 429 with nginx's HTML page": html429(),
    "a 502 while the daemon restarts": html502(),
    "a 500": res(500, { id: "x", secret: "y" }),                        // even a JSON body on an error status is refused
    "a network error (fetch rejects)": netErr(),
    "a 200 whose body is not JSON": res(200, "<html>oops</html>"),
    "a 200 JSON error body with no credential (the old loop)": res(200, { ok: false, reason: "rate_limited" }),
    "a 200 with an id but no secret": res(200, { id: "g_1" }),
    "a 200 with a secret but no id": res(200, { secret: "s" }),
    "a 200 with an empty id": res(200, { id: "", secret: "s" }),
    "a 200 with a numeric id": res(200, { id: 7, secret: "s" }),
    "a 200 with a null body": res(200, null),
  };
  for (const [name, step] of Object.entries(bad)) {
    const t = makeEnv([step, ok200()]);
    t.env.netMint(); await flush();
    check(t.env.netReady === false, name + ": netReady stays false");
    check(t.env.netId === "old-id" && t.env.netSecret === "old-secret", name + ": nothing is written from the bad answer");
    check(t.timers.length === 1 && t.timers[0].ms === 2000, name + ": one retry is scheduled, in 2s");
    await t.advance(1999); check(t.calls.length === 1, name + ": nothing before the 2s are up");
    await t.advance(1);
    check(t.calls.length === 2 && t.env.netReady === true && t.env.netId === GOOD.id, name + ": the retry succeeds and the credential arrives");
  }

  // ---- the backoff: 2s, 4s, 8s, 16s, then capped at 30s
  {
    const t = makeEnv([html429()]);
    t.env.netMint(); await flush();
    const seen = [];
    for (let i = 0; i < 9; i++) { seen.push(t.timers[0].ms); await t.advance(t.timers[0].ms); }
    check(JSON.stringify(seen) === JSON.stringify([2000, 4000, 8000, 16000, 30000, 30000, 30000, 30000, 30000]), "backoff doubles from 2s and caps at 30s: " + JSON.stringify(seen));
  }
  {
    const lo = makeEnv([html429()], 0); lo.env.netMint(); await flush();
    const hi = makeEnv([html429()], 1); hi.env.netMint(); await flush();
    check(Math.abs(lo.timers[0].ms - 1600) < 1e-6 && Math.abs(hi.timers[0].ms - 2400) < 1e-6, "jitter is +/-20% (1.6s..2.4s at the first retry): " + lo.timers[0].ms + ".." + hi.timers[0].ms);
    for (let i = 0; i < 8; i++) { await lo.advance(lo.timers[0].ms); await hi.advance(hi.timers[0].ms); }
    check(hi.timers[0].ms <= 36000.001 && lo.timers[0].ms >= 23999.999, "and the capped delay stays within 24s..36s (" + lo.timers[0].ms + ".." + hi.timers[0].ms + ")");
  }

  // ---- Retry-After, if a server ever sends one
  {
    const t = makeEnv([html429({ "Retry-After": "10" })]); t.env.netMint(); await flush();
    check(t.timers[0].ms === 10000, "Retry-After: 10 waits at least 10s (not the 2s backoff): " + t.timers[0].ms);
    const u = makeEnv([html429({ "Retry-After": "999999" })]); u.env.netMint(); await flush();
    check(u.timers[0].ms === 120000, "an absurd Retry-After is capped at 120s: " + u.timers[0].ms);
    const v = makeEnv([html429({ "Retry-After": "abc" })]); v.env.netMint(); await flush();
    check(v.timers[0].ms === 2000, "a Retry-After that is not a number is ignored");
    const w = makeEnv([html429({ "Retry-After": "1" })]); w.env.netMint(); await flush();
    check(w.timers[0].ms === 2000, "a Retry-After shorter than the backoff does not shorten it");
  }

  // ---- a success resets the counter
  {
    const t = makeEnv([html429(), html429(), html429(), ok200(), html429()]);
    t.env.netMint(); await flush();
    await t.advance(2000); await t.advance(4000);                       // third failure is now scheduled at 8s
    check(t.timers[0].ms === 8000, "after three failures the next wait is 8s");
    await t.advance(8000);
    check(t.env.netReady === true && t.env.netMintAttempt === 0, "a success resets the failure count");
    t.env.netHandleAuthFailure(); await flush();                        // the daemon restarted: remint, and it fails first
    check(t.timers.length === 1 && t.timers[0].ms === 2000, "the next outage starts again at 2s, not 8s");
  }

  // ---- one mint loop at a time
  {
    const t = makeEnv([html429(), ok200()]);
    const a = t.env.netMint(), b = t.env.netMint(), c = t.env.netMint(); await flush();
    check(a === b && b === c, "concurrent callers get the SAME promise");
    check(t.calls.length === 1, "and only one request is in flight");
    const d = t.env.netMint(); await flush();
    check(d === a && t.calls.length === 1, "a caller arriving between retries joins the loop instead of adding a request");
    await t.advance(2000);
    check(t.calls.length === 2 && t.env.netReady === true, "the loop then finishes normally");
    const e = t.env.netMint(); await flush();
    check(e !== a && t.calls.length === 3, "after a success, the next call starts a NEW mint");
  }

  // ---- netHandleAuthFailure: the re-auth path
  {
    const t = makeEnv([html502(), html502(), ok200()]);
    t.env.netReady = true;
    t.env.netHandleAuthFailure(); await flush();
    check(t.env.netReady === false && t.env.netRegion === null && t.env.otherOccupants.length === 0, "a bad credential drops netReady, the region and the occupants at once");
    check(t.env.netReauthInFlight === true, "and marks a remint in flight");
    t.env.netHandleAuthFailure(); t.env.netHandleAuthFailure(); await flush();
    check(t.calls.length === 1, "repeated auth failures do not start more mints");
    await t.advance(2000);
    check(t.env.netReauthInFlight === true && t.env.netReady === false, "the flag stays set while the daemon is still down (two failures so far)");
    await t.advance(4000);
    check(t.env.netReady === true && t.env.netReauthInFlight === false, "when the credential arrives, netReady is true and the flag clears");
    check(t.env.netRegion === null, "the region stays null so the next frame tick rejoins on its own");
  }

  // ---- it cannot become a request storm
  for (const [name, r, lo, hi] of [["typical jitter", 0.5, 18, 25], ["worst-case (fastest) jitter", 0, 20, 32]]) {
    const t = makeEnv([html429()], r);
    t.env.netMint(); await flush();
    await t.advance(10 * 60 * 1000);
    check(t.calls.length >= lo && t.calls.length <= hi, "ten minutes of a permanently failing mint makes " + t.calls.length + " requests (" + name + "; expect " + lo + ".." + hi + ")");
  }
  {
    const t = makeEnv([html429()]);
    t.env.netMint(); await flush();
    await t.advance(60 * 60 * 1000);
    check(t.calls.length <= 130, "an hour of failure is at most about two requests a minute (" + t.calls.length + ")");
  }

  // ---- console noise is bounded
  {
    const t = makeEnv([html429()]);
    t.env.netMint(); await flush(); await t.advance(30 * 60 * 1000);
    check(t.warnings.length >= 1 && t.warnings.length <= 8, "console warnings stay rare: " + t.warnings.length + " in 30 minutes of failure");
    check(/attempt 1\b/.test(t.warnings[0]) && /local-only/.test(t.warnings[0]), "the first warning says what is happening (local-only, attempt 1)");
  }

  // ---- source-level facts about the page itself
  check((html.match(/presence\/guest/g) || []).length >= 1 && !/return r\.json\(\); \}\)\s*\n\s*\.then\(function \(j\) \{ netId = j\.id/.test(html), "the old status-blind mint is gone from the page");
  check(!/staying local-only this session/.test(html), "the 'stay local-only for the session' dead end is gone");

  console.log("netmint_unit: " + checks + " checks, " + failed + " failed");
  process.exit(failed ? 1 : 0);
})().catch((e) => { console.log("FATAL " + (e && e.stack || e)); process.exit(2); });
