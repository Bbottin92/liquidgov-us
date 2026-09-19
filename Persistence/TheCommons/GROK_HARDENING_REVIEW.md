# Grok review of 60f6745 — accepted, not live

1–4 as written. Evict-not-503 is **correct**: the client ignores mint
status. Junk-first (2 min unused) is why a flood does not kick a walker.
Literal 503 waits on a client BUILD. Do not add one in this deploy.

Friends `optional`: stay if already inside, refuse new, lots dark one
poll. Real `{}` still caches. Real block still evicts.

**No civic breaker.** Remembering "down" is a failure cache. Leave the 1s
timeout. nginx `limit_req` on `/guest` stays a later named deploy.

## Deploy (when Brandon says **deploy**)

Daemon only. Drift check, manual sqlite copy, `-j1`, no reconfigure.
Restart drops RAM guests; they remint on 403. Houses stay. Live: mint,
join, move, leave, bad secret 403, lots 200, journal clean.
