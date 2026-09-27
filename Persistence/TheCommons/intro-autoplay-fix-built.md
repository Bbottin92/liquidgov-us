# iOS Low Power Mode: intro video autoplay fix — built, tested, DEPLOYED (civic only, verified live)

Brandon: "finish the iOS fix and commit it," after this was found sitting uncommitted and undocumented in the working tree
(stashed on 2026-09-27, no test or doc trail). **civic only** (`liquidgov-platform/src/http_server.cpp`, `intro_overlay_html()`).
**Corrected 2026-09-27**: this notes bullet and status line previously said "NOT deployed"; that was wrong. Fetched live on
2026-09-27 ~16:31 UTC: `https://liquidgov.us/`'s served page contains `vid.play()`, a `.catch(...)` handler, the `vid.paused`
watchdog check and the literal `1500` timeout, confirming this fix is live at commit `aee9a3d`. See the notes.md correction
bullet for the full reconciliation (it also covers a separate, correct bullet about the control-disclosure toggle).

## The bug (real device trace, iPhone Safari, 2026-09-21)

The Genesis homepage's full-screen intro video uses an attribute-only `autoplay` (plus `muted playsinline`), the usual way to
autoplay video on the web. iOS Low Power Mode (and some other autoplay-declining configurations) can silently refuse to start
it: no `error` event fires, nothing observable happens except a black frame sitting there. Before this fix, the only way the
overlay ever went away in that case was the existing 12-second absolute fallback (`setTimeout(finish,12000)`) — 12 seconds of
a stuck black screen on first load, which is exactly what "the site is broken" looks like to a visitor.

The live trace also showed a second, rarer shape of the same failure: the play `Promise` **resolves** and `currentTime` even
advances a little (0.445s in that trace), then playback silently freezes with no further event of any kind. A plain
"did play() reject" check would miss this.

## The fix (3 lines, `intro_overlay_html()`)

```js
var playPromise=vid.play();
if(playPromise&&playPromise.catch)playPromise.catch(finish);
setTimeout(function(){if(vid.paused)finish();},1500);
```

- Calling `vid.play()` explicitly (redundant with the `autoplay` attribute when it works — calling `.play()` on an already-
  playing video is a spec-defined no-op) is what gives the page a `Promise` to observe at all.
- A **rejected** promise (definitively blocked autoplay) calls `finish()` immediately — the fast path.
- A **1.5s watchdog** checks `vid.paused` on its own, independent of whether the promise rejected or resolved — this is what
  catches the second, rarer shape (a resolved promise that silently stalls). 1.5s is ample time for any browser that is
  really going to play the video to have left the paused state by then; the pre-existing 12s fallback is untouched as the
  final catch-all (e.g. if `vid.play()` itself is unsupported and returns nothing truthy).
- `finish()` already guards `if(done||!el)return;`, so nothing here needed a new idempotency guard — confirmed below.

Everything else in `intro_overlay_html()` — the gate, the fade-out-then-remove on `finish()`, the `ended`/`click`/`Skip`
triggers, the 12s fallback — is unchanged.

## Verified against the real compiled binary (not just read for correctness)

Built locally and run against a throwaway store; curled `/` and extracted the **actual served** `<script>` block (anchored on
`lgintrovid`, since the homepage has four self-invoking `<script>` blocks and a looser regex silently grabs the wrong one —
found this the hard way mid-verification: my first extraction attempt matched an unrelated script, and the resulting "test
failures" were a harness bug, not a script bug). `node --check` on the extracted script: valid syntax.

Then a fake-clock behavioral harness (same style this project already uses for the commons client's JS, e.g. `netstatus_unit.js`)
loaded that **real extracted script** into a `vm` context with a controllable `<video>` stub and drove three scenarios — 13
checks, all passing:
- **Normal playback** (the common case): the new code changes nothing — the overlay is not triggered early at any point, and
  the pre-existing 12s fallback + 720ms fade-out-then-remove still work exactly as before.
- **Blocked autoplay** (`play()` rejects): triggers on the very next microtask tick — not the 1.5s or 12s timers — and exactly
  one fade-out removal is scheduled.
- **Silent stall** (`play()` resolves, `vid.paused` never clears — the rarer live-observed shape): triggers right at the 1.5s
  watchdog, not the old 12s fallback. This is the exact live bug, proven fixed: 12s of stuck black screen becomes 1.5s.
- **Idempotency**: forcing both the rejection path and the 1.5s watchdog to fire for the same overlay schedules only one
  fade-out removal, via `finish()`'s pre-existing `done` guard.

**Mutation check**: re-ran the same harness against the script with the fix's 3 lines removed (the pre-fix version).
Correctly fails exactly the checks the fix addresses (the fast rejection path, the fade-out scheduling that follows it, and
the silent-stall watchdog) and throws on the idempotency scenario (no 1.5s timer exists at all without the fix) — proof the
test is exercising the real behavior, not passing vacuously.

civic has no CI; this repo's convention for it is local build + verification, which is what was done here (and, with the
JS specifically, this fake-clock harness — go further than a curl check for a change like this, since the whole point is
timing behavior a simple render diff can't see).

## Deploy — done, verified live 2026-09-27

Deployed since this was written (the section above was wrong for a while). Verified by fetching the live homepage and
confirming the served script contains the fix's own three lines (`vid.play()`, `.catch(finish)`, the `1500`ms `vid.paused`
watchdog), not by trusting a status line. No further action needed here.
