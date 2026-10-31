# BUILD rest slice 2 — daemon janitor

Named. Read `GROK_THE_REST.md`. This is **2**, not 3 or 4.

## What

1. **`sqlite3_step` / `prepare` results** in `house.cpp` (and any sibling
   store that still ignores them). Fail the request, log, do not `abort()`
   the daemon. Same tone as the periodic-hook `try/catch`.
2. **`lg_session` → username cache**, 15s RAM, keyed on the **Cookie:**
   value only (the FriendsCache lesson: not the whole request head). Same
   loopback civic call as today; skip the network when the cookie hits.
   Guests (no `lg_session=`) never call civic, still.

Daemon only. No client, no civic, no nginx, no schema, no PNG.

Not in this slice: `rebind()` (3), `open` + orphans (4), tile tweaks.

## Fail

- Caching the civic JSON of `/internal/friends` in this same map
- A disk cache, sqlite of sessions, parsing the cookie in the daemon
  beyond "does this string contain `lg_session=`"
- `abort()` on a bad `sqlite3_step`

No deploy until review, then Brandon says `deploy`. Daemon restart drops
RAM presence; houses stay. Online sqlite copy first anyway (belt).
