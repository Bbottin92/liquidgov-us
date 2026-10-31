# Bound inventory — built, local-tested, not deployed

Brandon: "BUILD bound inventory" (`GROK_V6_ANSWER.md`: "bound lab pickups
still vanish unless they're already on the wall... `inventory` has never
been written"). The last safe-thread scrap: daemon + client only, no
civic, no nginx.

## What's new

A bound player's lab lookups are now saved and restored -- across a
refresh, a new session, and a daemon restart.

- **`schema.sql`:** `inventory` gains `updated_at` (unix **ms** of the
  last pickup of that item). The client's only use of inventory is
  `lastPickedItem` -- "the most recent lookup is what the wall places" --
  and the table had no way to say which was most recent. Milliseconds so
  two pickups in one second still order.
- **Startup migration (`Database::ensure_inventory_recency`):**
  `migrate()` only runs `schema.sql` on a brand-new DB, so production
  will not pick the column up by itself. This idempotent step adds it if
  `pragma_table_info` doesn't show it; fresh DBs already have it and it's
  a no-op.
- **`HouseStore::add_pickup` / `inventory_of`:** find-or-create the
  `players` row (the same one a house entry later reuses -- `lg_username`
  is unique), then an upsert: quantity + 1, capped at 99, `updated_at`
  set; unknown slugs are rejected. `inventory_of` returns most recent
  first.
- **Routes**, under `/presence/` so the existing nginx location (which
  already forwards `Cookie`) covers them -- **no nginx change**:
  `POST /presence/pickup` and `GET /presence/inventory`. Identity is the
  forwarded session cookie only: no id, no secret, no username
  parameter, **no guest fallback**. Unbound: pickup is `403 not_bound`,
  inventory is `[]`. `GET` is registered before the `/presence/<region>`
  prefix route (first-match dispatch).
- **Client:** a bound player's lookup also POSTs the item; on the first
  bound response of a session it loads saved pickups once, fills
  `inventory`, and sets `lastPickedItem` to the most recent -- but only if
  nothing was picked this session yet, so a lookup made while the request
  was in flight wins. Guests send nothing and fetch nothing.

Pickups are still **not consumed** by placing (the lab stays real-name
flavor, not a crafting economy -- `pushback.md`), and counts are shown
nowhere; the observable effect is that the wall places what you last
looked up even after a refresh.

## Not here

- Guest persistence -- explicitly ruled out ("do not invent guest
  persistence"). Guest pickups stay RAM.
- An inventory menu / picker, consumption, trading -- none of it exists;
  `schema.sql`'s reserved `trades` note is untouched.
- The ~30s snapshot, `rebind()`, session cache, reaping orphans -- still
  parked.

## Tested (local)

Against an **old-shape DB** built from the previous `schema.sql` (the
shape production has), real civic accounts, real daemon:
- Startup added `updated_at`; a second startup on the migrated DB ran
  clean (idempotent).
- Chalk, chalk, ink -> inventory reads ink x1 first, chalk x2; unknown
  item 400; a guest's pickup 403 and a guest's inventory empty; a second
  bound account sees nothing of the first's.
- **Killed and restarted the daemon:** same items, same order.
- Re-picking chalk moved it to the front (qty 3). 105 copper pickups
  capped at 99.
- Entering her house afterward worked, with **one** `players` row and one
  house (pickup and house share the row); the other account has no
  inventory rows. `/presence/1,1` still returns occupants.

Client (real browser, scratch daemon; the daemon's answers stubbed where a
local page can't carry the civic cookie): a guest pickup counts locally
and sends **no** request; a bound response fetches saved pickups **once**
(a repeat doesn't refetch), restores the counts and `lastPickedItem`; a
bound pickup POSTs the right item; a pickup made before the load lands is
not overwritten; and the restored item is what the wall really placed
(read back from the daemon).

## Deployed 2026-09-18

Brandon: "deploy" -- backup, then daemon, then client; no civic, no
nginx (`liquidgov.service` never restarted).

1. **Backup first.** SQLite's own online backup (not a file copy, so it is
   consistent with the daemon running in WAL mode) to
   `/var/lib/commons/backups/commons.sqlite.pre-inventory-20260918T204938Z`:
   `integrity_check` ok, old-shape `inventory` (no `updated_at`), baseline
   counts recorded -- items 3, recipes 3, maps 14, map_instances 4,
   players 2, placed 2, inventory 0. Left in place.
2. **Drift check:** the daemon side was purely additive; the only line the
   client diff removed was the comment this round rewrote.
3. **Daemon** rebuilt (no CMake change) and restarted. Read-only check of
   the *live* DB afterward: `updated_at` present, `integrity_check` ok,
   and every pre-existing row count identical to the baseline -- the
   migration ran on real production data. New routes answered through
   nginx: guest inventory `[]`, guest pickup 403, `/presence/1,1` still
   occupants.
4. **Client** synced last; served page carries the new code.

Verified in production with a throwaway account in a real browser:
- Bound, in the real lab: looked up chalk then copper. Two
  `POST /commons/presence/pickup` -> 200, and `GET
  /presence/inventory` returned copper first, chalk second.
- **Reloaded the page** (client state confirmed empty), walked in
  without touching the lab: the client restored both items and set the
  wall default to copper. Entered its house and placed -- the wall took
  `copper_acetate_bottle`, read back from production.
- Deleted the account, then as a guest in the same browser: a lab lookup
  counted locally and sent **no** pickup and no inventory request.
- Both services active, integrity ok, `/` and `/commons/` 200, public
  `/internal/friends` still 404.

The deleted account's two `inventory` rows (and its `players`/house/wall
rows) remain as orphans by design; nothing deletes game data when a civic
account goes away.

Live. The backup stays on the box; it can be removed once you're
comfortable.
