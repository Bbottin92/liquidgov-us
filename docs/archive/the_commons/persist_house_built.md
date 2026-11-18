# Persist the bound house — built, local-tested, not deployed

**2026-09-17 update: must-fix found and fixed before deploy.** See
"Must-fix: walking out the door never snapshotted" below -- the
original test in this doc called `/presence/leave` directly, which is
not what the real client does when you walk out the door (that's a
rekey). Fixed; re-verified using the actual rekey path.

Brandon: "build" for `grok_v5_answer.md`'s named increment. Safe thread,
exactly as scoped: no `http_server.cpp`, daemon + `commons/index.html`
only. Demo and lab stay exactly as they were -- shared, RAM, wiped on
restart. Only `instance:house:<username>` is durable now.

## The hole this closes

`grok_v5_answer.md`: "You have a door. The room is still a hotel." The
house door and the bound identity behind it were both real, but
`PresenceStore`/`PlacedStore` are RAM -- a daemon restart (or the 30s
idle reap) emptied the house and dropped the chalk. Durability is what
actually makes it *yours*, not just yours-shaped.

## What's actually new

**Civic:** nothing. Untouched.

**Daemon, new `house.hpp`/`house.cpp` (`HouseStore`):** the first code
in this whole project to actually write `schema.sql`'s `players`,
`map_instances`, and `placed` tables -- all three existed, unused,
since the schema was written.

- `house_instance_for(username)`: find-or-create the `players` row
  (`lg_username` unique) and the `map_instances` row for their house
  (`map_id` from the existing `'house'` template row seed.sql already
  seeds, `owner_player_id` = their `player_id`) -- idempotent, called
  on every join/rekey into a bound player's own house.
- `existing_house_instance_for(username)`: the read-only twin, for
  `/placed/` GET -- never creates a house just because a public GET
  looked. Returns nothing until the owner has actually entered once.
- `last_house_position(username)` / `snapshot_leave(...)`: the
  `players.last_x/last_y/last_facing/last_instance_id` columns
  `presence.md` already specified, wired for the first time. Only ever
  reads/writes when the snapshot's own `last_instance_id` really is
  that player's house -- a snapshot taken from outdoors, or no
  snapshot at all, correctly falls through to the ordinary fixed entry
  tile.
- `get_wall_slot(instance_id)` / `set_wall_slot(instance_id, item)`:
  `schema.sql`'s real `placed` table, scoped by the numeric
  `instance_id` -- clear-then-insert wrapped in `BEGIN`/`COMMIT`
  (schema.sql's own stated rule for any multi-row mutation).

**Daemon, `main.cpp`:**
- `house_username_from_region(region)`: the same "is this a house key"
  check used everywhere a route needs to branch demo/lab (RAM,
  unchanged) vs. a personal house (SQLite, new).
- `/presence/join` and `/rekey`: when the target region is
  `instance:house:<username>`, first an ownership check -- a guest, or
  a *different* bound player, is refused outright (`403
  not_your_house`) before anything is created or touched. This is new:
  the RAM-only house never needed it (nothing durable to protect), but
  `players`/`map_instances`/`placed` rows are real data now, worth
  actually guarding on the mutation routes (GET stays public and
  unscoped, matching `/placed/`'s own long-standing "an item slug is
  not sensitive" stance -- unchanged, not part of this round).
  On a real owner, `house_instance_for()` ensures the durable rows
  exist, and `last_house_position()` is checked -- if a snapshot
  exists, it silently replaces whatever entry position the client
  optimistically sent, and the response echoes it back as
  `resume_x`/`resume_y`/`resume_facing`.
- `/presence/leave`: if leaving from a bound player's own house,
  captures that occupant's current position (looked up from
  `PresenceStore` before the row is removed -- `leave`'s own request
  body never carried x/y) and snapshots it via `HouseStore`.
- `/placed/` GET/POST: branches to `HouseStore` for a house key,
  `PlacedStore` (unchanged) for everything else. POST additionally
  gets the same ownership check as join/rekey -- placing into a house
  now mutates durable, private data, not a shared low-stakes slot.

**Client (`commons/index.html`):** `netHandleJoinResponse` applies
`resume_x`/`resume_y`/`resume_facing` when present, repositioning the
player instead of leaving them at the fixed `DEMO_ENTRY` tile the door
interaction always requests optimistically. Guarded on `mode ===
"house"` at the moment the (async) response actually lands, so a fast
door-in-door-out before the response arrives can't reposition a player
who has already walked away to somewhere else entirely.

## What's still NOT here (deliberately)

- **The ~30s periodic snapshot.** `presence.md` names both "on leave"
  and "~every 30s" for bound players; only the leave half is wired.
  The periodic half needs an actual timer/poll loop in a daemon that's
  otherwise purely request-driven -- a real, separate addition, not
  implied by "snapshot-on-leave rides that same BUILD."
- **In-place `rebind()`, the `lg_session` cache.** Both re-confirmed
  "no" in `grok_v5_answer.md`, unchanged from every prior round.
- **Guest persistence of any kind.** Explicitly ruled out
  ("Guests: still RAM, still vanish. Do not invent guest
  persistence.") -- guests never get a `players` row from this code;
  `find_or_create_player` is only ever called with a real, cookie
  resolved username.
- **Read-side access control on houses.** `GET /presence/<region>` and
  `GET /placed/<region>` stay exactly as public and unscoped as they
  were for demo/lab -- this round only adds ownership checks to the
  *mutation* routes (join/rekey/leave/place), matching the existing
  asymmetry the project has kept since `/placed/`'s own original
  "light integrity bar" design. Worth its own explicit round if it
  ever needs to change, not assumed here.

## Tested (local, scratch data only)

The one test that actually matters for this round: signed up a real
bound account, entered its house (no resume fields -- correctly no
snapshot exists yet), placed `chalk_block` in the wall slot, walked to
a distinctive tile, called `/presence/leave` from inside the house,
**then actually killed and restarted the daemon process** -- the exact
failure this build exists to fix. After restart:

- `GET /presence/instance:house:<user>` -- empty, as expected (RAM
  presence correctly does not survive a restart; nothing about that
  changed).
- `GET /placed/instance:house:<user>` -- **still `chalk_block`.** This
  is the actual bug, fixed: it used to come back empty.
- Re-entering the house -- `resume_x`/`resume_y`/`resume_facing` match
  exactly the tile and facing where `/leave` was called, not the fixed
  entry tile.

Also verified:

- A second real bound account, and separately a plain guest with no
  cookie, both get `403 not_your_house` trying to join or place into
  the first account's house -- the ownership check holds both ways.
- Demo house regression: still requires a real guest credential to
  place (unchanged `bad_credential` without one), a real guest still
  can place, RAM as always.
- Client logic (local static server, scratch daemon, `?daemon=`
  override): a simulated join response with `resume_x/y/facing`
  correctly repositions the player; the same response arriving after
  `mode` has already changed away from `"house"` is correctly ignored,
  leaving the player wherever they'd actually gone.

No deploy step run. Real cross-origin cookie -> real house persistence
across a real production restart is still only provable in production
-- next after Brandon says **deploy**.

## Must-fix: walking out the door never snapshotted (found by Grok review, fixed)

`grok_persist_review.md`: the client never calls `/presence/leave` when
a player walks out the house door -- `exitToOutdoor()` doesn't call
`netLeave()` at all; the next frame's normal region-mismatch check
fires a **rekey** instead (V2's own "no explicit network call needed"
design, unchanged since). Snapshot-on-leave, as first built, only ever
ran on `/presence/leave` -- meaning it only fired on an actual tab
close, never on ordinary walk-out-the-door play. This doc's own
original test called `/presence/leave` directly to prove the mechanism
worked, which is real but not what a real session does: enter, place
something, walk back outside, close the tab (or the box restarts)
later, hours after leaving the house -- the snapshot would still be
whatever it was from some earlier tab-close, or nothing at all. Chalk
would survive; the resume position would not.

Fixed:

- **`/presence/rekey`:** if `from_region` is the caller's own house
  (same ownership check as everywhere else -- bound, and the username
  matches), the occupant's current position is read from
  `PresenceStore` and snapshotted via `HouseStore` *before* the rekey
  call erases that row -- the exact same pattern `/presence/leave`
  already used, just triggered by the actual common path instead of
  the rare one. Leave keeps its own snapshot too, for true
  disconnects.
- **`house_instance_for`:** the `map_instances` INSERT and the
  `players` UPDATE are now wrapped in `BEGIN`/`COMMIT`, matching
  `schema.sql`'s own stated rule ("any multi-row mutation ... must be
  BEGIN/COMMIT") that `set_wall_slot` already followed and this
  function didn't. A crash between the two statements previously could
  leave an orphaned `map_instances` row with no `players` row pointing
  at it -- the next entry would then create a *second* house instead
  of reusing the first, stranding the wall-slot item on the orphan
  while every future write landed on the new one. Rolls back (and
  returns a sentinel `-1`) if either statement fails; this genuinely
  should not happen in practice since the schema always seeds the
  `'house'` map row before the daemon accepts any request, but the
  transaction is what schema.sql actually asks for regardless of how
  likely the failure is.

**Verified locally**, reproducing the real play pattern exactly (not
the direct-leave-call shortcut from the first test): entered the
house, placed `chalk_block`, walked to a distinctive tile, then
**rekeyed out the door to an outdoor region -- no `/presence/leave`
call made at all** -- then killed and restarted the daemon. Re-entering
resumed at exactly the tile and facing where the walk-out happened, not
`DEMO_ENTRY`; the chalk was still there. Separately re-confirmed `leave`
(tab-close path) still snapshots correctly too, and that repeated house
entry after all of this remains a single consistent instance -- no
orphan, no stranded wall-slot, matching the transaction fix's intent.

## Deployed 2026-09-17

Brandon: "deploy" -- daemon first (`house.cpp` is new), client second,
exactly as scoped. Drift-checked `main.cpp` against the droplet first:
matched this and the prior "your house" round's changes combined
(neither had been deployed separately), nothing unexpected;
`house.hpp`/`.cpp` correctly didn't exist there yet. Reconfigured
(new source file), rebuilt, restarted `commons-daemon`, confirmed
healthy (guest mint, outdoor poll) *before* syncing the client, per
the sequencing lesson from the cookie-resolver round.

Verified against production with a real throwaway account, through
the real client, not curl -- the actual gap this round's must-fix
closed:

- Entered the house for real, placed `iron_gall_ink_page`, walked to a
  distinctive tile, waited for the client's own move-sync tick to
  actually report that position to the server (confirmed via a direct
  `GET` before proceeding), then triggered the door's real fallback
  exit (`tryInteract()` with nothing in range) and let the client's
  *own* automatic network tick fire the rekey -- no manual daemon call
  from me at any point in this sequence.
- Confirmed after walking out: the house's presence emptied (correct,
  RAM), the placed item was untouched.
- Re-entered through the real door again: the client repositioned
  itself to the exact walk-out tile and facing, read back from the
  daemon's `resume_x`/`resume_y`/`resume_facing` over a real
  cross-origin cookie round trip -- confirmed via both direct state
  inspection and the on-screen nameplate/position.
- Did not force a second production daemon restart just to re-prove
  survival across one -- the identical binary already proved that
  conclusively in the local kill-and-restart test; restarting the
  daemon again here would only have interrupted the real live presence
  already on the box (a real `u:Bbottin` occupant was in the Square
  during this deploy) for no new information.
- Both services' logs clean post-restart. Site health (`/`, `/commons/`)
  unaffected.

Cleaned up the throwaway account (`persistverify9`) via its own
`/settings/delete`. Its game-side rows (`players`/`map_instances`/
`placed`, in the daemon's own SQLite, entirely separate from civic's
account store) are not touched by that deletion and remain as small,
harmless orphaned rows referencing a username that no longer has a
live civic account -- civic-frozen means account deletion has no code
path into the game's own database, by design, so this is an accepted,
inert byproduct of testing with a real account, not a bug.

Live. A bound player's house now survives a restart, for real, through
the actual walk-out-the-door path most sessions will ever use.
