# netMint check — built, tested, DEPLOYED 2026-09-19 ~16:47Z (client only)

Brandon: "BUILD netMint check" (the parked polish from `grok_sidecar_review.md`; the intent is from my own
`nginx_guest_built.md`: "`netMint()` should check `r.ok` and retry after a delay"). **Client only** (`commons/index.html`);
no daemon, civic, nginx or droplet change; deployed afterwards (see "Deployed" at the bottom).

## The bug it fixes (two dead ends, one of them latent)

The old `netMint()` never looked at the status: `fetch(...).then(r => r.json()).then(j => { netId = j.id; ...; netReady = true })`.

1. **Failure = local-only until reload.** Any failed mint (a 429 from the new nginx `/guest` limit, a 502 while the daemon
   restarts, a network blip) threw in `r.json()`, the catch logged "staying local-only this session", and nothing ever
   retried: the player kept playing but was invisible and could see no one until they reloaded the page. This also hit the
   re-auth path: after every daemon restart (a deploy), a client whose re-mint landed in the restart window stayed
   local-only.
2. **A JSON error body would have been read as credentials** (`netReady = true`, `id` undefined, then 403s in a loop).
   nginx's default HTML 429 only avoided this by accident (`r.json()` throws); that is why the nginx limit deliberately has no
   custom body. Now the client is correct on its own.

## What it does now (`netMint:begin` / `netMint:end` block in `index.html`)

- **A credential is accepted only from a 200 answer whose body has a non-empty string `id` and `secret`.** Anything else (a
  non-2xx status even with a JSON body, an HTML page, a 200 without a credential, a null body, a numeric id, a network error)
  is a failure that writes nothing.
- **A failure retries with capped exponential backoff, forever but quietly:** 2s, 4s, 8s, 16s, then 30s, each with +/-20% jitter
  (so a room of clients behind one address does not retry in lockstep). The game keeps running local-only meanwhile, and the
  next `frame()` tick joins the moment a credential arrives (the same path as a late first mint).
- **`Retry-After`, if a server ever sends one, is honoured** (never shortens the backoff; capped at 120s; garbage is ignored).
  nginx does not send one today.
- **One mint loop at a time:** startup and `netHandleAuthFailure()` share it (concurrent callers get the same promise; three
  simultaneous 403s make one mint). `netMint()` resolves only once a credential is in hand, so `netReauthInFlight` stays set for
  the whole outage and cannot stack remints. A success resets the failure count.
- **Bounded noise:** one console warning on the first failure and every 10th after; at most about two requests a minute per tab
  in a permanent outage.

**The numbers are a decision (no doc gave any); veto if wrong.** 2s base, 30s cap, +/-20%: after a deploy restart a client
recovers in a couple of seconds; against the nginx limit (one token per 3s per address) a class of clients converges within
a minute without a storm; a dev page with no daemon costs about two failed requests a minute and a quiet console. Retrying
forever (not N times) is deliberate: giving up is the bug being fixed.

## Tests (in CI via `run_all.sh`: `test_netmint.sh`; CI has node, and a missing node there is a failure)

`tests/netmint_unit.js` (**88 checks**) extracts the SHIPPED block from `index.html` and runs it in a vm with a fake clock and a
scripted fake `fetch`, so it tests what players load, not a copy: the 200 path; 11 bad answers (429 HTML, 502, 500 with a JSON
body, network error, non-JSON 200, JSON error body with no credential, missing/empty/numeric id, missing secret, null body), each
writing nothing and succeeding on the 2s retry; the exact backoff sequence and cap; jitter bounds; Retry-After (honoured, capped,
ignored when junk or shorter); the counter reset; single-flight (concurrent and between retries); the re-auth path and its flag;
ten minutes and an hour of permanent failure make a bounded number of requests; console warnings stay rare.
**Mutation-checked, 11 breakages** (no status check, a credential-less 200 accepted, no backoff, no cap, no single-flight,
Retry-After ignored / uncapped, counter never reset, give up on failure, no jitter, re-auth flag never set): each fails the test.
`test_netmint.sh` also pins the markers and the two call sites.

**Verified in a real browser against a real daemon** (throwaway daemon, then cleaned up): the normal path mints, joins and is
listed; with the re-mint answered by two injected HTML 429s the attempts came at 0ms, 1744ms, 6482ms (2s and 4s backoff), local
play never stopped, then a new credential, a rejoin, and the daemon lists the new id; and a real daemon restart (down 4s,
every RAM credential wiped) gave three simultaneous 403s, exactly ONE mint (a 200 five milliseconds later), a rejoin, and the
restarted daemon lists the player.

## Deploy (when Brandon says `deploy`): client only

`git log commons/` before this change: last touched by **d9a36e5 Open houses (4a): civic /internal/blocked, daemon open policy + lots discovery, cl**; the droplet has no `.deployed-client-rev` marker yet
(the client was last deployed before the scripts existed), so the first client drift check needs that baseline:
`drift-check.sh --tree client --baseline <the commit whose commons/ is live>` (read-only), then `sync-client-to-droplet.sh`
(which writes the marker), then a browser check: `/commons/` loads, a mint and join work, and the existing 5-mint/lifecycle
verify (`verify.sh`). `deploy-daemon.sh --with-client` would also restart the daemon for no reason; use the two client scripts.
No daemon restart, no nginx change. A cached old `index.html` is possible for visitors mid-session (`Cache-Control: no-cache`
is set, so a reload picks it up).

## Limits, said plainly

- The retry has no UI: a player in a long outage plays local-only with only a console warning. Adding a visible status is a
  product decision, not part of this build.
- Not covered by CI: a real browser (the vm harness runs the shipped code; the browser check above was manual).
- Still parked: the `index.html` split, the `/health` commit hash. The `/commons/recipes` 404 poll is left alone.

## Deployed (2026-09-19 ~16:47Z, Brandon: `deploy` twice, plan then sync; Grok had accepted the build and the backoff numbers)

Client only. **Step 1 (read-only):** `drift-check.sh --tree client --baseline d9a36e5`: no drift (the droplet held exactly what
`d9a36e5` shipped), one file to change, `index.html`. The tree guard also blocked the first attempt: an untracked
`commons-daemon/deploy/__pycache__/*.pyc`, bytecode my own retention unit test had created by importing
`deploy/prune-backups.py`. I deleted that one file and fixed the cause (`b04970e`: the test sets `sys.dont_write_bytecode`,
`DAEMON_EXCLUDES` gains `__pycache__` and `*.pyc`, `.gitignore` ignores them, `test_deploy.sh` +2 assertions, mutation-checked).
**Step 2:** `sync-client-to-droplet.sh`: `/opt/commons/index.html` now byte-identical to the repo's (sha256 `8a74adbb...f67f71`), `tiles.png` and
`sprites.png` unchanged, chowned to `liquidgov`, marker `/opt/commons-daemon/.deployed-client-rev` = `b04970e` (outside the
served directory; over https it is a 404). **No daemon restart, no build, no nginx:** `commons-daemon` `ActiveEnterTimestamp`
15:24:54 UTC and `NRestarts=0` before and after; nginx, civic and the daemon all active.
Verified through https: `/commons/` 200, `Cache-Control: no-cache`, the served bytes equal the repo's and contain the new
`netMint:begin` block (the old page had 0); `verify.sh --guest-name x` **22 passed, 0 failed** (civic `/` 200, public `/internal/`
404, lots 200, one guest named `x` minted/joined/moved/left, journal clean). Visitors mid-session keep the old page until they
reload; the next `deploy-daemon.sh` needs no `--baseline` for the client tree either (the marker exists).
