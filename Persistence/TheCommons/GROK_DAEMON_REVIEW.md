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

## Must-fix (presence accounting — real, local)

1. **`join` does not reject a duplicate `id`.** Second join of the same
   id in the same region pushes a second `Occupant` and `++total_`.
   One tab can consume the 96 cap. `join_locked`: if `id` is already in
   *any* region, treat it as a rekey/replace, or return a 409. Never two
   rows for one id.

2. **`rekey` decrements `total_` whenever `from_region` exists**, even
   if `remove_if` removed nobody. Then `join_locked` may fail RegionFull
   and the counter is now lying. Only `--total_` if an occupant was
   actually erased (same as `leave`).

3. **Body size.** Headers cap at 1 MiB. `Content-Length` does not.
   `need` can be 2 GB and `body.append` will try. Cap body at 64 KiB;
   413 and close. 512 MB box.

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
- 413 checks **declared** `need`, not leftover bytes already in `raw`.
  Header guard is 1 MiB, so a pipelined first packet can still hand
  `dispatch` ~1 MiB. Truncate `body` to `need` (or 413 if leftover
  `> kMaxBody`). Not a 5 GB hole.

## Fail if “fixed” by

- systemd / nginx / SSH / `listen 0.0.0.0`
- Opening `liquidgov_accounts.lgp`
- Wiring friends / cookie / wall-slot (civic still frozen)
- A framework HTTP stack / npm
- Putting `commons.sqlite` under the static alias

Paste `CLAUDE_DAEMON_PATCH.md`. Deploy stays a later, explicit BUILD.

---

## Follow-up (d2b5bf4) — accepted

Must-fixes closed. `presence.cpp` kept 92e379d (Claude pulled, did not
clobber). `http.cpp` 413s `need < 0 || need > 65536` **before** the
append loop. Civic untouched. Notes mis-says “id-index redesign” —
92e379d is replace/erase_id, the index is still a should-fix in
`presence-accounting.md`. Harmless.

Deploy still waits on Brandon. Idle reap / server-issued ids still a
deploy gate, not this week unless asked.
