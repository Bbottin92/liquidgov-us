# hardening (four daemon fixes) — built, local-tested, not deployed

Brandon: "BUILD hardening" (`grok_hardening.md`, items 1-4 of my survey). **Daemon
only.** No civic, client, nginx, schema or CI change. No deploy. The nginx
`limit_req` on `/commons/presence/guest` is a separate, later named deploy.

## 1. Guest secrets come from the OS (`guest.cpp`)

`random_hex` was a `mt19937_64` seeded with 64 bits of `random_device`; the comment
oversold it. A client can mint its own credentials to collect consecutive outputs and
recover the generator's state, then predict other guests' secrets. Every byte now comes
straight from `getrandom(2)` (EINTR retried). If the OS entropy source ever fails,
`mint()` throws **before** touching the registry and the HTTP layer answers 500. Bound
players (cookies) were never affected; guests are low stakes, but a bearer credential
must not be predictable.

## 2. Registry cap 256 (`guest.cpp/.hpp`)

`POST /presence/guest` is world-reachable and the registry had no bound (and every
mint/verify rescans it). It is now capped at **256**. **Deviation from the spec, on
purpose:** the spec said refuse with 503 when full. I **evict instead of refusing**,
because the unchanged client does not check the mint reply: a 503 would leave it with
`netId = undefined`, `netReady = true`, sending `missing_field` 400s every 500ms and
never appearing in presence. Eviction never fails the mint. Two rules keep it from being
a self-inflicted lockout:
- A credential that was **never used** (no join/move) is junk: it expires after **2
  minutes** instead of 30, and is the **first** thing evicted (oldest first). A real
  client verifies within a second of minting. So a flood of mints only ever evicts other
  junk.
- Only if all 256 are real, used guests is the least recently used dropped; that guest
  re-mints on its next 403, the path a daemon restart already exercises.
If Grok prefers the literal 503, it needs a 3-line client fix (check the mint reply);
I did not touch the client.

## 3. Socket timeouts (`http.cpp`)

`serve_one` `recv`/`send` could block forever, and the daemon is one thread. Every accepted
socket now has `SO_RCVTIMEO`/`SO_SNDTIMEO` of **5s**, and the whole request must arrive
within **10s** so a trickle cannot hold the connection. A truncated body (declared
`Content-Length` not delivered) is **closed, never dispatched**. No thread added. nginx
normally buffers requests, which is why this never bit in production.

## 4. FriendsCache does not cache a civic failure (`civic.cpp/.hpp`, `friends.cpp/.hpp`, `main.cpp`)

`resolve_friends` now returns `optional`: `nullopt` = **no answer** (no cookie, civic
down or slow, an unreadable reply); an empty list is a **real** answer (including civic's
`{}` for a session it does not recognise). `FriendsCache::get` never stores a no-answer,
touches nothing, and returns `nullopt` if there is no fresh cached list. Callers treat
`nullopt` as "don't know", not "no friends":
- **`visit_allowed`, friends house:** never walk out someone already inside (staying), and
  do not let anyone new in (entering). An open house doesn't need the friends list: it is
  decided by the block check either way.
- **`/presence/lots`:** friend doors stay dark for that poll (nothing cached, the next poll
  asks again); open houses are still offered.
- A guest (no cookie) is an empty list, never cached.

## Tested (local, real civic and daemon)

Each result printed its precondition (timings prove civic was really asked and silent).
1. **Secrets:** id `g_`+16 hex, secret 32 hex, **0 duplicates in 300 mints**. (`strace`
   isn't installed here, so "getrandom is the source" rests on the code path, not a
   syscall trace.)
2. **Cap:** a never-used junk credential J0, a real guest A that joined, then a **400-mint
   flood: 0 non-200 answers** (no 503 ever); A still valid afterwards; **J0 evicted (403)**.
   All-used case: 300 guests that each verified: the first is evicted (403), the last valid.
3. **Timeouts:** baseline health 0.3ms. A client that sends half a request and goes silent
   held a normal request **4.8s** (before: forever); a 1-byte-per-3s trickle held it
   **11.7s** (the 10s deadline is checked after each recv, so worst case is the deadline
   plus one 5s receive timeout, and it is bounded); a stalled body held it **~4.8s** three
   times over, the daemon answered the stalled client with **0 bytes** (nothing dispatched);
   daemon 8ms afterwards, no errors. (A first run showed 11.7s for the stalled body; that
   was overlap with the previous case, re-measured separately.)
4. **Friends cache:** bob (friend) inside alice's friends-policy house; at t=16s both
   his caches are expired; a session-only request refreshes just the session cache; **civic
   frozen**: bob's move takes ~1.03s (proof civic was asked) and answers **`ok:true`, he
   stays** (before: `not_welcome`); his lots are `[]` for that poll and **not pinned** (the
   moment civic is back they list alice); a new entrant (carol) with civic silent is
   **refused**; a **real block still walks bob out** after the cache window (regression); a
   real "no friends" answer (carol) is still cached (instant with civic frozen); a guest's
   lots are instant and empty. (My first version of this test froze civic after *both*
   caches had expired, so it tested the session lookup, not the friends cache; a
   session-cache hit does not refresh it. Redone with a session-only refresh.)
5. **Regressions, all passing:** visiting (block eviction), house policy (friends/locked/
   open, restart), open houses (block probe, lots, fail-closed), session cache, rebind,
   persist-house, the ~30s snapshot (kill -9), cookie bind and the guest join/move paths.

## Noticed, not done (a possible later name)

**Civic outage amplification:** while civic hangs, every bound request that needs civic pays
the 1s timeout (the session cache already skips failures), so the daemon's throughput drops
to about one such request per second. Not new (it was true before), and item 4 does not
worsen the *session* path, but it does mean lots polls and visitor moves pay it too.
A tiny "civic is down" breaker (a couple of seconds, no answers cached) would bound it;
it conflicts with "never remember a failure" for a few seconds, so I left it for Grok to
decide.

## Deploy (when Brandon says `deploy`, after Grok's review)

**Daemon only**, no schema/civic/client change. Drift check, manual online SQLite copy,
sync, `-j1` build (no reconfigure), restart (drops RAM presence and every guest credential:
guests re-mint through the existing 403 path; houses are durable). Live check through nginx:
lots/presence/placed 200; a guest mint, join, move and leave work; a bad secret is 403;
journal clean.

## Deployed (2026-09-19 12:55:33Z, Brandon: "deploy"; Grok accepted in `grok_hardening_review.md`)

Daemon only. Civic, client and nginx untouched (`liquidgov.service` not restarted).
Grok's review confirmed evict-not-503 is right (the client ignores the mint status),
no civic breaker, nginx `limit_req` on `/guest` stays a later named deploy.

1. **Drift check:** exactly this round's eight daemon files (`civic.cpp/.hpp`,
   `friends.cpp/.hpp`, `guest.cpp/.hpp`, `http.cpp`, `main.cpp`); civic and client
   dry-runs empty.
2. **Manual online SQLite copy first:**
   `/var/lib/commons/backups/commons.sqlite.pre-hardening-20260919T125451Z`,
   `integrity_check` ok. Baseline: items 3, recipes 3, maps 14, map_instances 2,
   players 1 (`Bbottin`), placed 0, inventory 2. The orphan watch had logged nothing
   since the reap.
3. Synced, `-j1` build with no reconfigure, restarted at **12:55:33Z**. Journal:
   `orphan watch is REAPING ...`, listening on `127.0.0.1:8765`. Live DB read-only:
   integrity ok, every count identical to the baseline. Through nginx: lots, `1,1`,
   house `/placed/` all 200; `/` and `/commons/` 200; public `/internal/user-exists/x`
   404.
4. **A real guest through the public site:** mint 200 (id `g_`+16 hex, secret 32 hex),
   join ok, move 200, **bad secret 403**, leave ok; a guest's lots `[]` in 98ms.
5. **On the box, a stalled half-request:** a normal request behind it waited **5.0s**
   (before: forever); the daemon answered in 11ms right after.
6. Journal clean, `NRestarts=0`, both services active. The restart dropped every
   in-RAM guest credential; live guests re-mint through the existing 403 path. Houses
   are durable.

Not seen on production (needs load or a real outage): the 256-cap eviction under a real
flood, and the friends-cache "stay" behaviour with a real civic hang. Both are the code
paths the local tests exercised with a frozen civic and a 400-mint flood.
