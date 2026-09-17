# Grok review of commons-daemon (ba296e8 / 476653d)

Local sibling process: accept the **scope**. Civic frozen. No SSH, no
systemd, no nginx, loopback bind, no LGP1, collision stays in JS, cookie
forward parked. Seed matches atlas + chalk-only. Caps 24 / 96. WAL.
That is the right slice.

Do **not** deploy this commit.

## Accept

- Separate CMake from `liquidgov-platform/`
- `127.0.0.1` only
- Catalog GET + RAM presence POST
- Restart: disk stays, RAM dies
- Friends / `/canvas-frame` / nginx left unwired

## Must-fix (closed)

1. Duplicate `join` / `total_` — 92e379d
2. `rekey` decrement on a miss — 92e379d
3. Uncapped `Content-Length` — d2b5bf4 413; 9f7d609 truncates leftover
   bytes after the header split to exactly `need`

## Should-fix (deploy gate, still not nginx)

- `move` never writes `last_update_ms`. Occupant JSON looks frozen.
- No idle reap. A crashed tab holds a cap slot until process restart.
  `presence.md`: drop if `last_update_ms` older than ~30s.
- Client-chosen `id` + `display_name` is impersonation once nginx is
  in front. Server-issued guest token (or cookie bind) before public.
- Two `schema.sql` files (`Persistence/TheCommons/` and
  `commons-daemon/`). One source of truth.
- 9.5 MB `sqlite3.c` in git is legal (amalgamation, public domain) but
  painful. Prefer CMake FetchContent + SHA256 pin; keep the vendor
  story, drop the blob if you can.
- The 9f7d609 notes bullet landed under **Shared pad**, not Game. Move
  it if you touch notes anyway. Not a code fail.

## Fail if “fixed” by

- systemd / nginx / SSH / `listen 0.0.0.0`
- Opening `liquidgov_accounts.lgp`
- Wiring friends / cookie / wall-slot (civic still frozen)
- A framework HTTP stack / npm
- Putting `commons.sqlite` under the static alias

Deploy stays a later, explicit BUILD.

---

## Follow-up (d2b5bf4 + 9f7d609) — accepted

413 before the append loop. `body.resize(need)` after the read so a
pipelined first packet cannot inflate a legal Content-Length. Presence
untouched. Civic untouched.
