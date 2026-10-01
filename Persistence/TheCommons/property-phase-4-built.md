# Property phase 4: a real new region (the Dunes) — built, tested, NOT deployed

Brandon: `BUILD property phase 4`, approving `CLAUDE_PROPERTY_PHASE4.md` as written -- itself written after
he pushed back on an overstated "riskier" claim and asked for specifics, which turned out to matter: the
grid math was already parameterized, and the real cost was authoring content, not fragility.

## What changed

- **`commons/js/world.js`**: `GRID_ROWS` 4 -> 5 (`WORLD_H` 80 -> 100, derived automatically). One new
  region, `"the Dunes"` at (0, 4), added to `REGIONS`. One new tile, `T_SAND` (id 41, the next free cell),
  reusing `paintTile`'s existing grass speckle technique for its own colour -- no new drawing path. The
  Dunes' AABB gets filled with `T_SAND` in the same base-fill loop Quarry Ridge's own `T_DIRT`
  special-case already used. Columns 1-2 of the new row fall through to the existing default `T_GRASS_A`
  fill, confirmed (not assumed) to be architecturally harmless before this was written.
- **A path connecting Quarry Ridge into the Dunes**, laid before `dressRegions()` so a scatter prop can
  never land on it -- Brandon's own standing rule, already applied to every other region this session.
- **A real bug found while making this change, not by inspection**: the existing north-south spine path's
  own loop ran `vy <= WORLD_H - 1`, which was fine when that equaled Shore's own south edge (79) but would
  have silently extended the spine 20 more tiles into the new, intentionally-undressed row once `WORLD_H`
  grew to 100. Pinned to `shore.y1` explicitly instead.
- **`Persistence/TheCommons/art/make_art.py`**: the tileset generator has its own *hardcoded* `NTILES = 41`
  constant -- not derived from parsing `world.js`, despite everything else in that script being parsed
  from it. Adding `T_SAND` without updating this failed the script's own internal consistency assertion
  immediately (caught by running it, not by reading it first). Fixed to 42, added a `DRAW["T_SAND"]` entry
  reusing the same `grass()` function. Regenerated `commons/tiles.png`.
- **`commons-daemon/catalog.sql`**: one new `maps` row (`dunes`, outdoor, grid 0,4) -- presence treats a
  region key as an opaque string and doesn't actually need this to function, added for parity with every
  other region's own row, via `INSERT OR IGNORE` so an existing production database picks it up on its
  next ordinary start, the same way every other catalog addition here already does.

## Tests

`client_files_unit.js` grew from 143 to **148 checks**: `GRID_ROWS === 5 && WORLD_H === 100`; the Dunes
exists in `REGIONS` at (0, 4); a path tile sits on both sides of the Quarry Ridge / Dunes boundary (not two
disconnected patches of ground); the Dunes' own ground is `T_SAND`; columns 1-2 of the new row are
confirmed still plain `T_GRASS_A`, not accidentally dressed too.

`world_scatter_unit.js`: unchanged count (33), rerun to confirm the hand-picked path x-coordinate (14)
doesn't collide with Quarry Ridge's existing scatter props -- it doesn't, confirmed by the test, not by
eyeballing the coordinate list.

Full suite: **26 files, 0 failed**.

Verified visually in the browser preview: teleported into the Dunes via the console -- the live tile sheet
showed a black patch exactly where the new tile should be (the preview's own stale-PNG caching, the same
artifact hit earlier this session with JS files, confirmed by forcing the procedural fallback
`tileSheet = null` and rebaking, which rendered the sand correctly). Walked the path from Quarry Ridge's
dirt straight into the Dunes' sand with no gap and no collision with the quarry's own crate/low-wall
scatter.

**Mutation-checked**, each restored after confirming the right failure:
1. Reverted `GRID_ROWS` to 4 -- all four new region-existence/path/tile assertions failed together
   (the whole region collapses out of existence, as expected).
2. Moved the connector path off-map (`dunesPathX = 999`) -- only the path-specific assertion failed,
   isolated correctly from the grid-size and tile-content checks.

## Limits, said plainly

Columns 1-2 of row 4 (south of Creek and Shore) are still plain, undressed grass -- open space with no
name, no content, left for a later pass. The Dunes itself has no scatter dressing, no gather spots, no
interactables -- a real, walkable, visually distinct place with a way in, nothing more. North Woods
(forest) and Shore (beach) got no additional dressing either, per Brandon's own earlier framing that those
already count. `tiles.png`'s own regeneration script had a real latent bug (the hardcoded `NTILES`) that
this build happened to be the first change in a long time to actually exercise.

## Deploy

Client-only except the one new `catalog.sql` row (no schema change, no new route). Binary asset change
(`commons/tiles.png`) included in the client sync. Holds for a literal `deploy`, same as always.
