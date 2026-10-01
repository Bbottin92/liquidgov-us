# Your own property, Phase 2+3: gather, craft a tool, build a real fence post — proposed

Brandon's own words: "gather things... to make tools myself, and then with those tools I should be able
to build a home or fence or whatever." The roadmap named gather/craft (phase 2) and building structures
(phase 3) separately. Proposing them as **one slice**: a tool with nothing to unlock is a half-finished
feature on its own, and "build something real" is the actual payoff being asked for.

## What's already there, checked before proposing anything new

- Gather -> bench -> collect already exists and is generic over the catalog -- adding a new recipe needs
  no code change in `studio.cpp`, only new `catalog.sql` rows. `willow_twigs` (gather spot
  `north_woods_twigs`) and `scrap_iron` (`workshop_iron`) already exist and are already thematically
  wood/metal -- no new gather spots needed for this slice.
- Floor placement (`place_floor`, used in Phase 1) is **not a security boundary and does not consume
  inventory** -- confirmed by reading `house.cpp` and `net.js`'s `netPlaceFloor` directly: placing an item
  never deducts it from `inventory`, and the daemon never checks you actually hold what you're placing
  (an existing, deliberate "low stakes, a display string" design, not an oversight). This matters a lot
  for scoping: once you've crafted one of something, you can place copies of it forever, the same way a
  seashell already works today.
- Floor items have **zero collision** today -- `boxBlockedAt`/`tileBlockedAtPixel` (`actors.js`) only ever
  check the tile grid, never `placedFloor`. A fence that doesn't block anything isn't a fence; this is the
  one genuinely new mechanic this slice needs.

## Decision 1 — two new catalog items, reusing existing materials

- **`hammer`** (category `material`, not meant to be floor-placed itself): bench recipe from
  `willow_twigs` + `scrap_iron`. The tool. Never consumed, never placed -- holding one is a permanent
  unlock, the same shape the Studio's own pigment discoveries already use.
- **`fence_post`** (category `placeable`): bench recipe from `willow_twigs` alone. The thing you actually
  put down. Non-consuming like every other floor item (see above) -- craft one, hold it, place as many
  copies as the 24-item cap allows.

No new gather spots, no schema change -- two `catalog.sql` recipes, following the exact pattern the
existing 13 already use (duration_s, tier, real_range_note, history_note in the same voice). Proposed
durations: hammer 1h (a simple tool, not a long process), fence_post 30 min (matching the fastest existing
recipe, `carbon_ink`). `test_studio.sh`'s own catalog-wide assertions ("13 Studio recipes...") become 15.

## Decision 2 — the hammer gate is client-side, matching the existing trust model

`place_floor` doesn't check inventory for anything else; adding server-side enforcement here would be a
new, inconsistent security boundary for one item type in a system that's deliberately "low stakes"
everywhere else. Proposed: a small client-side `STRUCTURAL_ITEMS = { fence_post: true }` table (same
shape as `ITEM_CATEGORY`); `tryFloorPlace()` (`interact.js`) refuses with a toast ("Need a hammer.") if the
item being placed is structural and `inventory.hammer` isn't held. Bypassable by a determined direct API
call, exactly like every other floor-item rule already is -- consistent, not a regression.

## Decision 3 — real collision, client-side only

`tileBlockedAtPixel` (`actors.js`) grows one more check: is there a `placedFloor` entry at this tile whose
item is in `STRUCTURAL_ITEMS`? If so, blocked, same as a solid tile. No daemon change -- the daemon
already has no concept of collision at all (it's baked into the client's own tile-solidity array), so this
stays consistent with where that logic already lives.

## Decision 4 — no new rendering pass

`fence_post` gets an icon the same way every other item does -- a procedural category glyph (the
backpack's own `ITEM_CATEGORY`/bundle-or-twig shape), drawn flat on the tile via the existing
`drawItemGlyph()`. The thing that makes it read as a fence is that it blocks you, not a taller sprite.
Visual distinctiveness (a real post-shaped sprite, something you can line up into an actual fence line) is
a reasonable later refinement once the core loop is proven, not part of this slice.

## Out of scope, said plainly

No "build a home" (walls, a roof, an enclosed structure) -- fence posts only, the smallest real
"something you built, that works" unit. No new gather spots. No server-side enforcement of the hammer
gate. No tool durability or multiple tool tiers. No limit on *where* a post can go beyond the existing
floor-item rules (any clear interior tile on your own property) -- no "must be near the border" logic for
this pass. Phases 4 (new biomes) and 5 (the town) untouched.

## Tests

- `test_studio.sh`: the catalog-wide recipe-count assertions move from 13 to 15; a new section gathers
  willow_twigs + scrap_iron, benches + collects a hammer and a fence_post for a fresh account.
- `client_files_unit.js`: `STRUCTURAL_ITEMS` exists and contains `fence_post`; `tryFloorPlace` refuses
  without a hammer (mutation-testable: a fresh inventory with `fence_post` but no `hammer` must be
  refused); `tileBlockedAtPixel` treats a structural `placedFloor` entry as solid (a direct function call
  with a synthetic `placedFloor` entry, same shape as the wear-a-pigment tests' synthetic
  `nearestInteractable`).

## Deploy

Client-only except the two new `catalog.sql` rows (no schema change, no new route, no C++ logic change in
either `house.cpp` or `studio.cpp`). Holds for a literal `deploy`, same as always.

---

**Say `BUILD property phase 2` to build this as written, or tell me what to change first.**
