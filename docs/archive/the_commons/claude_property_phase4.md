# Property phase 4: a real new region (the Dunes) — proposed

Brandon: expand the world for real, rather than re-skin an existing region. Checked before proposing
anything (see the chat record, not repeated here): the grid math (`regionAABB`, `regionAt`,
`regionsIntersectingCamera`) is parameterized off `GRID_COLS`/`GRID_ROWS`/the `REGIONS` array, not
hardcoded in dozens of places -- the actual cost of a new region is authoring its content (tiles, dressing,
a path in), the same amount of work any one of the existing 12 regions already took, not fragility risk.
Also confirmed: the world fills entirely with a default tile first (`T_GRASS_A`, one exception for Quarry
Ridge's own dirt), then each named region dresses itself on top -- an unfilled grid cell is just plain
grass, not a hole. That means a new row doesn't need three new regions to fill it, only one.

## Decision 1 — grow the grid by one row, not a column

`GRID_ROWS` 4 -> 5. `WORLD_H` (`GRID_ROWS * REGION_H`, 20 tiles/region) grows from 80 to 100 tiles --
`WORLD_W`/camera/presence-polling math all derive from these constants already, nothing else to touch for
the resize itself. A row, not a column, because the natural neighbor for a desert is Quarry Ridge (0,3) --
arid rock giving way to open sand reads better than appearing next to the Orchard or the Square.

## Decision 2 — one new region this pass: "the Dunes," at (0, 4)

South of Quarry Ridge, matching this game's existing short-name convention ("the Square," "the Lookout,"
"the Orchard"). Columns 1 and 2 of the new row 4 (south of Creek and Shore) stay undressed default grass
for now -- confirmed architecturally harmless, and two more full regions is real additional authoring work
better left for a later pass once this one is proven, not bundled in sight-unseen.

## Decision 3 — one new tile, `T_SAND`

No sand/desert tile exists today (checked `world.js`'s tile enum directly). A flat warm tan fill with the
same simple dot-lattice texture technique `T_GRASS_A`/`T_GRASS_B` already use (`paintTile`'s own grass
branch) -- reused, not reinvented. Not solid, not tall -- plain walkable ground, same shape as grass.

## Decision 4 — a path in, from the start

Brandon's own earlier rule, already applied to every other region this session: nothing gets built without
a way to reach it on foot. A dirt/path strip runs from Quarry Ridge's south edge into the Dunes' north
edge, the same `T_PATH`/`vergePaths()` mechanism the rest of the world already uses.

## Decision 5 — scope: the space exists; content is a later pass

This phase is the world-expansion work only -- a real, named, walkable, visually distinct region, reachable
by a path, visible to the camera/presence system like any other. **No new gather spots, no new materials,
no scatter-generator rules for it yet.** Matches how phase 1 (an empty plot) shipped before phase 2 (gather
and craft) was layered onto it -- proving the space first, deferring what goes in it. A bare, open expanse
of sand is itself a reasonable first look at a desert, not an unfinished one.

## Decision 6 — daemon: one new `maps` row, for parity, not because anything requires it

Presence treats a region key as an opaque string (confirmed in `presence.hpp`) -- the daemon does not
actually need a `maps` row to let people stand in "0,4". Adding one anyway (`slug 'dunes'`, `kind
'outdoor'`, `grid_col 0`, `grid_row 4`, width/height matching `REGION_W`/`REGION_H`) keeps the table an
accurate record of what exists, matching every other region's own row.

## Out of scope, said plainly

No second or third new region this pass (columns 1-2 of row 4 stay grass). No desert-specific gather spots,
items, or recipes -- phase 2's gather/craft loop is not being extended here. No new creature, weather, or
mechanic tied to the biome. No change to any existing region. Mountains (Quarry Ridge) and beach (Shore)
are not getting additional dressing in this pass either -- Brandon's earlier framing accepted those as
already-existing analogs; only the genuinely missing one (desert) is being built.

## Tests

- `world_scatter_unit.js`/the scatter author: no new assertions needed if the Dunes gets no scatter rules
  this pass -- confirmed the existing `CAPS` table only ever asserts about regions that have rules.
- `client_files_unit.js`: `GRID_ROWS === 5` and `WORLD_H === 100`; the Dunes appears in `REGIONS` at
  `(0, 4)`; a path tile connects Quarry Ridge's south edge to the Dunes' north edge (not just two
  disconnected patches of ground); the Dunes' own ground tiles are `T_SAND`, not left as the default
  `T_GRASS_A` the rest of row 4 keeps.

## Deploy

Client-only except the one new `maps` row (no schema change, no new route). Holds for a literal `deploy`,
same as always.

---

**Say `BUILD property phase 4` to build this as written, or tell me what to change first.**
