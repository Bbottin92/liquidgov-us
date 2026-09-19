# BUILD hardening — four daemon fixes, not nginx, not tests

Claude's 1–4 are real. I checked `guest.cpp`, `friends.cpp`, `http.cpp`.

## 1. Guest secrets

`random_hex` is `mt19937_64` seeded with 64 bits of `random_device`. The
comment oversells it. Consecutive mint output can recover MT state.
Bound cookies are unaffected. **Fill each secret from `getrandom` (or
`/dev/urandom`)**. Do not keep MT for the hex. Ids can stay `g_` + random.

## 2. Guest registry cap

No size cap; mint/verify scan the map. Cap the registry (e.g. 256 live
guests; refuse mint with 503 when full after reap). **nginx `limit_req` on
`/commons/presence/guest` is a separate named deploy** — not this BUILD.
`civic-ratelimit.md` still forbids `/internal/`.

## 3. Client socket timeouts

`serve_one` `recv`/`send` can block forever. nginx hides it until it
doesn't. `SO_RCVTIMEO` + `SO_SNDTIMEO` a few seconds on `client_fd` after
accept. Close and continue. Do not add a thread.

## 4. FriendsCache must not cache civic failure

`get()` stores whatever `resolve_friends` returns. Empty-on-timeout is
cached 15s as "no friends" → lots dark, visitors `not_welcome`. Session
and block caches already skip failures. **Same here:** no answer → return
last *good* list if still in TTL, else "no answer" (do not evict, do not
write `{}`). Distinguish `{}` (really no friends) from timeout.

## Not this BUILD

Tests-in-CI (5), deploy scripts (6), backup retention (7), splitting
`index.html`, `/health` hash. Those are later names.

Daemon only. No civic, no client, no schema. No deploy until review.
