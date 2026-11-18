# Presence — The Commons

Who's where, live. RAM only. Never a `schema.sql` table, never a civic
(`liquidgov_http`) write — presence and the civic store share nothing but the
read-only friends query and the session cookie check (see `glue.md`).

## Shape

One in-memory map keyed by **region id**, not one global broadcast:

- Outdoor region id = the `(grid_col, grid_row)` pair from `atlas.md` (12
  keys, fixed).
- Interior region id = `map_instances.instance_id` (one key per live house
  copy, plus one for the lab, plus one for the demo house).

Each region's value is a small list of active occupants:

```
{ player_id_or_guest_id, display_name, x, y, facing, last_update_ms }
```

`x, y` are tile coordinates local to that region (not supermap-absolute) —
the client already knows which region it's rendering. Crossing an outdoor
AABB **rekeys** the occupant from one region's list to the neighbor's; do
not leave a ghost in the old list.

## Tick rate

**Per-region ~2 Hz**, not a global tick and not per-player. A region with
nobody in it broadcasts nothing — don't run 12 empty loops at 2 Hz for no
reason.

The lock is per-region *tick*, not per-region *vision*. A client subscribes
to every region whose AABB intersects the camera. On a 24×20 region vs a
15×10 (desktop) or 10×15 (phone) viewport that is typically 1–4 outdoor
regions. **Not** "one tile-row of the neighbor" — that hides walkers at the
seam and is the FireRed failure mode visiting is supposed to beat.
Interiors: subscribe only to that instance. Neighbor outdoor occupants ride
their own region's 2 Hz feed. This is still not a global 10 Hz flood.

**Not 10 Hz.** The droplet this is meant to eventually run beside is 512 MB /
1 vCPU (same box `liquidgov_http` runs on, per `notes.md`'s standing pin) —
2 Hz keeps "someone else is here and moving" readable for a walking-speed
game without competing for that CPU.

## Guest ids vs `lg_username`

- A guest gets an ephemeral id (session-local counter or UUID) the moment
  they pick a throwaway name, with no `players` row required yet in V1 (see
  `v1.md` — V1 doesn't wire SQLite for guests at all, presence *is* their
  entire state).
- Once cookie glue exists (post-V1) and a guest binds a LiquidGov session,
  the *same* presence entry is relabeled with `lg_username` in place — a
  promotion, not a disconnect/rejoin. Position, facing, and any in-flight
  state carry over untouched.
- A bound player who reconnects is looked up by `lg_username`, not by
  re-picking a name.

## Snapshot / persistence boundary

Presence itself is never written to SQLite. The only disk touches:

- **On clean leave** (tab close, explicit logout): if the player is bound
  (has a `players` row), their last position is written onto that row
  (`last_grid_*` / `last_instance_id` + `last_x` / `last_y` / `last_facing`
  in `schema.sql`) — a single small write, not a stream.
- **Periodic safety snapshot, ~every 30s**, of bound players only, for crash
  recovery — not a general persistence path, and not required for V1 (V1
  has no bound players to snapshot; wire this when glue lands).
- Guests are never snapshotted. Losing a guest's position on crash is fine
  — they were never durable.

## Caps (placeholder numbers — tune at deploy, not measured yet)

- **Per-region cap: 24 occupants.** Past this, a joiner either queues or
  sees a "the Square is full right now" state rather than the process
  accepting unbounded broadcast fan-out for one region.
- **Global cap: 96 concurrent.** Sized to leave headroom next to
  `liquidgov_http` on the same 512 MB / 1 vCPU box, not derived from load
  testing — revisit once there's real traffic data.
- Both caps are config constants, not hardcoded literals, so they can move
  without a redeploy of logic.

## What this explicitly is not

- Not a civic write path. `liquidgov_http` never receives a call because
  someone moved in the Square.
- Not a friend graph. Presence doesn't know who's friends with whom; that
  question is answered by the read-only query in `glue.md` when a client
  asks "is this occupant on my friends list" for a Homestead-door-style
  check, not by presence storing it.
- Not the trades/inventory mutation path — those go through `schema.sql`
  under `BEGIN/COMMIT` when they exist (not in V1).
