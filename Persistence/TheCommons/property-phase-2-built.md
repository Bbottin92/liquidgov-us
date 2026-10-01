# Your own property, Phase 2+3: gather, craft a tool, build a real fence — built, tested, NOT deployed

Brandon: `BUILD property phase 2`, approving `CLAUDE_PROPERTY_PHASE2.md` as written. Phases 2 (gather/craft
tools) and 3 (build structures) shipped together -- a tool with nothing to unlock would be half-finished
on its own.

## What changed

- **`commons-daemon/catalog.sql`**: two new items (`hammer`, category `material`; `fence_post`, category
  `placeable`) and two new bench recipes, reusing materials that already existed -- no new gather spot.
  `hammer` (willow_twigs + scrap_iron, 1h, tier 1). `fence_post` (willow_twigs alone, 30 min, tier 1). Pure
  data; zero C++ changes in `studio.cpp` -- gather/bench/collect is already generic over the catalog.
- **`commons/js/interiors.js`**: `hammer`/`fence_post` added to `PLACED_ITEM_INFO`, `ITEM_CATEGORY`
  (fence_post reuses "twig"; hammer gets a new category, "tool") and `ITEM_DESC`, same full treatment
  every other Studio item already gets. New `STRUCTURAL_ITEMS = { fence_post: true }` -- a real table, not
  a single hardcoded check, so a second structure type later is one more line.
- **`commons/js/sprites.js`**: one new procedural glyph category, "tool" (a plain T -- a head bar over a
  haft). Every other new item this build adds reuses an existing category.
- **`commons/js/actors.js`**: `tileBlockedAtPixel()` now also blocks on a structural `placedFloor` entry.
  The one genuinely new mechanic -- floor items had zero collision before this (confirmed by reading the
  function directly, not assumed). Needed a `typeof mode/placedFloor !== "undefined"` guard:
  `assertSpawnIsFree()` calls this function at script-load time, before `interact.js`/`net.js` (where
  those are declared) have loaded -- same shape `easel.js` already uses for the same reason, caught by the
  very first client test run, not by inspection.
- **`commons/js/interact.js`**: `tryFloorPlace()` refuses a structural item ("Need a hammer.") unless
  `inventory.hammer > 0`. Client-side only, matching `place_floor`'s own existing stance -- it has never
  checked inventory for anything, a deliberate "low stakes" design, not a gap this build should be the
  first to break.
- **`commons/js/studio.js`**: `hammer`/`fence_post` added to `RECIPE_OUTPUT_ITEM`/`RECIPE_DURATION_MS` so
  the bench shows their icon and progress bar like every other recipe, not a silent fallback.

## Tests

`client_files_unit.js` grew from 137 to **143 checks**: `STRUCTURAL_ITEMS` exists and marks `fence_post`;
holding `fence_post` without a `hammer` is refused client-side with no fetch; holding both succeeds;
`tileBlockedAtPixel` treats a structural floor item as solid and an ordinary one (seashell) as not.

`test_studio.sh` grew from 129 to **145 checks**: the catalog-wide recipe-count assertions moved from 13
to 15 (two places -- the duration_s count and `recipe_count` in the notebook response, found the second
one by running the test, not by inspection); a new section drives a fresh account through a real
gather -> bench -> collect chain for both new recipes (not a database shortcut), confirming the leftover
willow_twigs from gathering (3) minus the hammer's own use (1) correctly carries over to fund the
fence_post recipe without a second gather.

**`test_migrations.sh` also needed updating** -- its own separate hardcoded catalog totals (38 items, 21
recipes, checked in two places: an existing migrated DB and a fresh one) were not something
`CLAUDE_PROPERTY_PHASE2.md` anticipated, and this doc's own plan didn't check for them ahead of time. Not
caught by inspection -- caught because the full suite was actually run before calling this done, which is
the entire reason that run happens. Fixed to 40/23 in both places. Swept the rest of the test directory
afterward for the same two numbers; nothing else was hiding them.

Full suite: **26 files, 0 failed** (second run, after the migrations fix; the first full run genuinely
failed on exactly the above, not a flake).

**Mutation-checked**, each restored after confirming the right failure:
1. Hammer-gate check removed from `tryFloorPlace` -- the "refused without a hammer" assertion failed.
2. Structural check removed from `tileBlockedAtPixel` -- the "blocks the tile it sits on" assertion failed.

## Limits, said plainly

Fence posts count toward the same 24-item floor cap as decorations -- no separate structural budget.
Placement has no restriction beyond the existing floor-item rules (any clear interior tile on your own
property); nothing enforces "posts in a line" or "only near the border." No visual distinction between a
fence post and any other floor item beyond its glyph -- it reads as a fence by blocking you, not by a
taller sprite. No tool durability, no second tool tier. "Build a home" (walls, a roof) is still out of
scope; this is fence posts only, the smallest real unit. Phases 4 (biomes) and 5 (the town) untouched.

## Deploy

Client-only except the two new `catalog.sql` rows (no schema change, no new route). Holds for a literal
`deploy`, same as always.
