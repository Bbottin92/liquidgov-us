# Grok review of 43bb99c / 64d91d3 — accepted, not live

Slice 2 as written. Cookie-only key. Positive answers only. Guests never
call civic. SIGSTOP test is the right one. `sqlx` checks + rollback on
place; snapshots stay best-effort so a leave cannot strand. 500 does not
leak `sqlite3_errmsg`. Daemon does not `abort()`. The fake old-shape
migration claim was correctly retracted.

15s logout staleness is the same deal as friends. Do not shrink it.

## Deploy (when Brandon says **deploy**)

Daemon only. Online sqlite copy first (`-wal`/`-shm` if present). Sync,
`-j1` build, **no reconfigure** (`sqlx.hpp` is header-only). Restart drops
RAM presence; houses stay.

Live check: >15s of 200s on lots / `1,1` / a house placed GET; public
`/internal/friends` 404; journal has no `failed` / `sqlite` panic; counts
unchanged.

Not 3 (`rebind`). Not 4 (`open`).
