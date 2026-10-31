# Self-ghost — built, local-tested, not deployed

Brandon: "BUILD self-ghost" (Grok, `GROK_LATEST_LIVE`-era brief and
`CLAUDE_RESUME_SELF_GHOST.md`: "Client-only: skip your own `u:` row").
Client only. Civic, daemon and nginx untouched.

## The bug

A bound player has two ids. The client authenticates with a **guest
credential** (`netId`, `g_...`), but once a civic cookie resolves the daemon
files their occupant row as **`u:<username>`** (`cookie-bind.md`). Every
occupant poll returns the caller's own row too, and the client's "is this
me" filter compared only against `netId`, so a bound player's own row was
listed as *another* occupant and drawn a second time -- a copy of yourself
that lags by up to one poll interval (~0.5s) while you walk. Live since
cookie bind; found while verifying `/latest`.

## The fix (`commons/index.html`, +15 / -2)

- `netBoundId` and one helper, `isSelfId(id)`: true for `netId` **or** the
  bound id.
- The two occupant filters (the outdoor camera-neighbor poll and the
  single-region interior poll) use `isSelfId`.
- `netBoundId` is set from **every** join/rekey answer as `"u:" + username`
  when `bound`, else `null` -- so it tracks a guest promoted to bound, and a
  logout. On the answer that first sets it, any copy of that row already in
  the last poll is dropped immediately rather than drawn until the next one.
- It matches on the id, never on `display_name`, which the daemon
  disambiguates with " (2)" suffixes.

No new id was invented; join/rekey already echoed `bound` and `username`
(`1dfe2ec`).

## Tested (local, real browser, scratch daemon)

Followed `CLAUDE_RESUME_SELF_GHOST.md`: **no** `credentials: include`
patch, **no** second copy of the client, **no** scratch civic accounts.
(I had started to do all three before reading that brief -- it was written
while I was out of budget. I tore that scaffolding down and redid the check
its way.) One thing is stubbed: the occupant poll also returns a
`u:ghostuser` row, the way the daemon lists a bound player's own row;
everything else in the poll is real.

1. **Guest** (real join): my own `g_` row excluded, a genuine second guest
   drawn. Unchanged.
2. **Bug state, same client:** with the bound id not yet known,
   `u:ghostuser` appears among the "other" occupants -- the ghost.
3. **After the bound join answer:** `u:ghostuser` is gone immediately, and
   stays gone on the next poll.
4. **Not over-broad:** `u:someoneelse`, deliberately given the display name
   `ghostuser (2)`, is still drawn, so it isn't filtering on names; the real
   second guest is still drawn once rejoined (they had idled out of the
   daemon's 30s reap mid-test -- I checked that rather than assume).

## Not here

- No deploy. Per the brief: deploy later is **client only**, after Grok
  reviews.
- A real bound session on production will be the true end-to-end check
  (the local page can't carry a civic cookie); that comes with the deploy.

## Deployed (2026-09-18, Brandon: "deploy")

Client only, via `commons-daemon/deploy/sync-client-to-droplet.sh` to
`/opt/commons`, then `chown -R liquidgov:liquidgov`. Daemon, civic and
nginx untouched. Droplet file and the served `/commons/` both md5
`f1354d53...` = local `commons/index.html` (was `d58ebe19...`); `/commons/`
and `/commons/presence/lots` answer 200; served page contains `isSelfId`.
Still to see: a real bound session on production walking with one sprite
(the local page can't carry a civic cookie).

## Confirmed on production (2026-09-18)

Brandon, logged in and walking the Square: no copy trailing him. Closes the
one open check above. (My own production check was guest-only: joined as
`g_...`, own row not listed, no console errors -- I don't log in or create
accounts.)
