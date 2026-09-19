# Daemon janitor (rest slice 2) — built, local-tested, not deployed

Brandon: "BUILD session cache" (Grok's `GROK_REST_2.md` / `GROK_THE_REST.md`
slice **2**, which pairs the cache with the `sqlite3_step` checks; I built the
slice as written). **Daemon only.** No client, civic, nginx, schema or PNG. No
deploy.

## 1. `lg_session` -> username cache (`civic.cpp`)

`civic::resolve_username` used to make one loopback call to civic on every
bound request (a walking player's ~2 Hz moves included). It now keeps the answer
for **15s in RAM**, keyed on the **Cookie value only** (the FriendsCache lesson:
not the whole request head). Same civic call on a miss.

- **Only a positive answer is cached.** civic down, a timeout, or "no such
  session" is asked again next time, so a civic hiccup is never remembered for
  15s and a bad cookie is never pinned as "guest".
- **Guests never reach civic** and never touch the map (no `lg_session=`).
- Cap 256; on overflow drop expired entries, then clear (same shape as
  FriendsCache). No mutex (single-threaded daemon). No disk, no sqlite, no
  cookie parsing beyond the existing "contains `lg_session=`".
- **Not** the friends cache: `/internal/friends` answers stay in `FriendsCache`.
- **Staleness, said plainly:** a logged-out or revoked session can still
  resolve for up to 15s (same order as the friends cache already accepts).

## 2. `sqlite3_step` / `prepare` checks (`sqlx.hpp`, `house.cpp`, `main.cpp`)

New header-only `sqlx::Stmt` / `sqlx::Tx`: every prepare, bind, step and COMMIT
result is checked. A failure **logs the real sqlite message to stderr** and
throws `DbError("database error")`.

- `HttpServer::dispatch` (it already caught `std::exception`) now logs
  `METHOD path failed: ...` and answers a generic `500
  {"ok":false,"reason":"server_error"}` instead of leaking `what()` in
  hand-built JSON. One bad statement fails **one request**; the daemon never
  aborts.
- `house.cpp` is rewritten on the wrappers (every store method). Multi-row
  writes use `Tx`: it **rolls back** on any failure or exception, so a failed
  `set_wall_slot` never leaves the wall emptied and never leaves a transaction
  open; `house_instance_for` keeps its INSERT+UPDATE atomic the same way.
- The catalog readers in `main.cpp` (`items_json`, `recipes_json`, `maps_json`,
  `item_exists`) are checked too, so a broken query is a 500 rather than a
  silent empty `[]`.
- **Best-effort exception:** position snapshots (`snapshot_leave` on leave,
  on rekey-away, and in the ~30s pass) log a failure and carry on
  (`snapshot_quietly`). Failing the request would strand a player who is trying
  to walk out, or stop the pass for everyone else.

## Tested (local, real civic + real daemon, scratch stores)

**Cache** (civic frozen with SIGSTOP so a real civic call costs its 1s timeout
and a cache hit is instant):
1. alice joins her house (a miss, 1.8 ms). With civic frozen her next move is a
   **hit: 0.46 ms, bound**; the same cookie with unrelated extra headers is still
   a hit (keyed on the Cookie value only).
2. bob's never-seen cookie while civic is frozen: a **~1.0s timeout**, then
   guest fallback (`403 not_bound`). Unfrozen, bob **resolves at once** (`409
   no_house` = bound): the failure was not cached.
3. A guest with no cookie is instant while civic is frozen: it never calls civic.
4. **TTL:** after ~15s alice's entry has expired: with civic frozen again her
   move takes ~1.03s and falls back to guest (`403 bad_credential`); civic back,
   she resolves again.

**Step checks** (failures injected with SQLite triggers/renames):
1. A trigger makes the wall INSERT fail: the place is a `500 server_error`, and
   the wall **still shows the old item** (the DELETE was rolled back). Trigger
   dropped: the same place succeeds (no dangling transaction).
2. `/recipes` with its table renamed: `500 server_error`, not `[]`; restored, 200.
3. A trigger makes the snapshot UPDATE fail: **leave still returns ok and the
   player is gone from the room**; the ~30s pass logs the failure and the daemon
   keeps serving (`/health` ok, process alive).
4. The journal has the real errors (`sqlite step failed: injected (code 19)`,
   `no such table: recipes`); no response leaks them.

**Regressions, all unchanged and passing:** `policy_test` (friends/locked incl.
restart), `visit_test` (friends visit, block eviction), `persist_house_test`,
`snapshot_test` (kill -9 crash recovery from the ~30s tick). (The old-shape
migration script builds its "old" DB from `HEAD`'s schema, which now already has
`visit_policy`, so re-running it here proves nothing; `db.cpp` is untouched this
round, and that migration ran for real on production on 2026-09-19.)

## Deploy (when Brandon says `deploy`, after Grok's review)

**Daemon only.** No schema change, but take an online SQLite backup first (Grok:
"belt"), then sync, `-j1` build (new header `sqlx.hpp`; `CMakeLists.txt` is
unchanged so no reconfigure is needed), restart. Restart drops RAM presence;
houses are durable. Live check through nginx across a >15s window: lots/presence/
placed 200, `/internal/friends` 404, journal has no `failed` lines, live DB
integrity ok and counts unchanged.

## Not here

`rebind()` (3), `open` + orphans (4). No client change.
