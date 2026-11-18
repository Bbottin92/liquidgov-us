# Connection status ("Reconnecting…") — built, tested, DEPLOYED 2026-09-19 (client only)

Brandon: "BUILD connection status: A, as written" (`claude_status_question.md`, answered A by `GROK_STATUS_ANSWER`-style relay: a DOM badge
outside the canvas, one line, observe only; no extra "must stay clear" timer; the machine in `js/net.js`; palette from the existing CSS; do not cover the
d-pad; no new requests). **Client only.** Built and held for review; Brandon then sent `deploy` (plan: drift check showed exactly index.html, js/main.js, js/net.js, style.css; then sync).
**Deployed from bd50119**, client tree only, daemon not restarted. Verified: the four files served are byte-identical to the repo (sha256), the served page has `#connStatus` and 8 scripts, `drift-check.sh --tree client` = no drift at bd50119, `verify.sh` 23/23. CI on bd50119 was still running when it was synced (the previous head 6401058 was green).

## What it is

A small pill at the top centre of the page: **"Reconnecting…"**, shown only when the client cannot reach the daemon for a noticeable time, gone on the next
success. Before this a visitor got no signal at all: moves failed silently, the player kept walking a local copy of the world and vanished for everyone else.

- `<div id="connStatus" role="status" aria-live="polite" title="Can't reach the Square. You can keep walking, but others can't see you until it reconnects."></div>`
  in `index.html`, EMPTY until needed (CSS `:empty{display:none}`; a live region announces the text when it is written).
- CSS in `style.css`: `position:fixed` top centre (`top:calc(.6rem + env(safe-area-inset-top))`), `z-index:15` (above the d-pad 10 and Enter button 11, under the name
  overlay 20), the page's own `--paper` background and `--ink` text/border, `pointer-events:none` (it never takes a tap). The d-pad is anchored to the bottom, the badge to the top.
- The state machine, in `js/net.js` between `netStatus:begin`/`netStatus:end` markers (so the unit test runs the shipped code): shown when EITHER a mint loop
  (startup, or a remint after a 403) has been pending **more than 4 s**, OR **6 consecutive heartbeats** have failed; hidden as soon as neither holds, i.e. on the next
  success. No hysteresis timer: the two thresholds are the anti-flap. `frame()` calls `netStatusTick(t)` once per frame.
- **Observe-only:** the block sends no request and starts no timer (the test checks the source), and no retry or backoff changed. `netMove` reports whether the daemon
  answered (`r.ok || r.status === 403`: a 403 means it answered, and the remint path handles it) and reports a network error before re-throwing it to the same silent `catch` as before.

## One deliberate deviation from the wording, flagged for review

Grok wrote "6 consecutive move/poll failures". **The heartbeat is the move POST only; polls are not counted.** A tick sends one move but 1-4 region polls, so counting every
fetch would show the badge after about a second of an ordinary daemon restart, which is exactly the blip it must not flash on. Counting the move (one per tick) makes 6 failures about 3
seconds at 2 Hz. A failed join or rekey is still covered, because the client sets its region optimistically and the moves that follow then fail. The test pins this: `netMove` is the only reporter
(three occurrences of `netStatusReport(`: the definition and two calls) and `netPollRegions`/`netPollCurrentRegion`/`netPollPlaced`/`netPollLots`/`netJoin`/`netRekey`/`netLeave` contain none.

## Tests (in CI via `run_all.sh`; CI has node)

- `tests/netstatus_unit.js` (**31 checks**): thresholds; the ordinary path never writes to the DOM; a mint pending 3999 ms / exactly 4000 ms / 4001 ms; the badge text is a real
  ellipsis and the element is EMPTY when clear; hides on the very next tick when the credential arrives; a later remint gets its own fresh 4 s; the 4 s counts from when the mint was first
  seen; 5 failures no, 6 yes, a success hides it and resets; 5-bad-1-good repeated never shows it; an ordinary restart (2 failures then the daemon answers) never shows it; a huge
  count still clears on one success; both causes at once hide only when both are clear; no DOM writes while it stays shown; no element (an old cached page) does not throw; and the block contains
  no `fetch`/`XMLHttpRequest`/`sendBeacon`/timer.
- `tests/test_netstatus.sh` (**29 assertions**): the wiring above, the element and its CSS (z-index 15, `pointer-events:none`, `:empty`, palette variables, top not bottom), and the unit.
  `test_client_files` (53 checks) also requires the markers, the element and `netStatusTick`/`netStatusReport` after load.
- **Mutation-checked, 14 breakages** (6 failures -> 3, 4 s -> 1 s, "more than" -> "at least", no reset on success, the mint clock from page load, a 403 counted as failure, hiding leaves text,
  the frame never ticks, polls also report, a network error not re-thrown, the badge sending a request, z-index above the overlay, taps allowed, a non-empty element): each fails the tests.

**A real bug in my own test wrappers, found by that mutation run:** with Windows `node.exe` under WSL, `test_netmint.sh`, `test_client_files.sh` and the new `test_netstatus.sh` read the
unit test's exit code with `${PIPESTATUS[0]}` after a `$(... | tr ...)`, which is always `tr`'s 0, so a FAILING unit test passed the wrapper locally (five mutations the unit catches showed
28/0). CI (Linux node, plain `$?`) was never affected. Fixed in all three: node's output goes to a file so `$?` is node's own exit code, and each wrapper also requires the unit's own
`0 failed` summary. Re-run: all 14 mutations fail the wrappers.

## Real browser, real daemon (the frame loop driven from a timer, see limits)

A throwaway daemon was killed for 8 s, then for 0.8 s, while the page recorded the badge, the credential state and the failure counter. **Outage:** the first failed heartbeat was
122 ms after the kill; the badge appeared at `fails=6`, 4.4 s after the kill (the second failure took 2.5 s: WSL's localhost relay is slow to refuse a connection, so this is slower than
the 3 s a clean refusal gives); the daemon came back and the badge hid **257 ms later**, on the first answer (a 403: the daemon answered), and the credential was back 65 ms after that.
**Blip:** the daemon was down 0.8 s, but the new daemon's first replies to the remint were two `ERR_EMPTY_RESPONSE` (WSL's relay accepting a connection before the new daemon was listening),
so the existing mint backoff (2 s, then 4 s) made the remint take 5.5 s and the badge showed for about 1.5 s of it: it told the truth, but a blip that also fails its first remint attempt CAN show it.
**Remint refused** (mint answered with an HTML 429 like the nginx `/guest` limit): hidden at 3.5 s, shown at 5 s, cleared once the mint worked again, the player rejoined and was listed by the daemon.
Layout: on a 879x706 desktop and a 375x812 phone the badge (about 119x28) sits at y 10-37, the d-pad at the bottom (y 554-694 and 659-799), no overlap, it fits the width,
z-index 15, and a tap at its centre lands on the page below (not on the badge). Screenshots showed the pill on the game with the d-pad untouched.

## Limits, said plainly

- The built-in browser pane was hidden, so the browser paused `requestAnimationFrame` (`document.hidden`); the test drove the game's OWN `frame()` from a timer (rAF made a no-op so
  callbacks would not pile up). That runs the real page code end to end (real fetches and failures, the counter, the tick, the DOM, the CSS) at the normal 2 Hz heartbeat, but it is not
  the browser's own frame scheduling. The threshold timing is proven by the fake-clock unit tests.
- In a background tab the frame loop stops, so the badge stops updating there; nobody is looking at it then.
- The remint backoff (2 s, 4 s, ... 30 s) can itself keep a recovering client "pending" past 4 s when the first attempts fail. That is the existing netMint behaviour, not the badge.
- CI still has no real browser: it checks the state machine and the wiring, not the rendering.

## Deploy (when Brandon says `deploy`, after review): client only, the two-step protocol

Step 1 read-only: `drift-check.sh --tree client` will list `index.html`, `style.css`, `js/main.js` and `js/net.js` and nothing else. Step 2: `sync-client-to-droplet.sh` (two phases,
`index.html` last), then verify (the four files identical to the repo on disk and as served, the page loads with 8 scripts, `verify.sh`, a real join in a browser, the daemon not restarted).
Visitors mid-session keep the old page until they reload. Rollback: re-sync the previous files (the old page has no `#connStatus`; the new scripts tolerate its absence).
