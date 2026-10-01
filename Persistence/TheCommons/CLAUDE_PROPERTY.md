# Your own property, Phase 1: an empty plot — proposed (awaiting Brandon's sign-off)

Brandon's full vision: your own property (friends visit it from the main world, which could have several
environments -- forest, desert, mountains, beach -- plus a town to buy and sell in). Confirmed as a
multi-phase roadmap, not one feature:

1. **Your own empty plot** (this doc)
2. Gather + craft tools (reuses the Studio's existing gather->bench->collect loop)
3. Build structures on your plot (fences, etc. -- tool-gated, real collision)
4. New biomes in the main world (forest exists in some form already as North Woods; desert/mountains do
   not exist at all)
5. A town with buying and selling -- this is the one that reverses "no currency," confirmed everywhere
   else in this project including `CLAUDE_IDEAS.md`'s own Rejected list. Brandon said it himself this
   time, not relayed, so it's a real, deliberate reversal when we get there -- but it gets its own
   dedicated conversation before anything is built, not a line item here.

Phases 2-5 are a named roadmap, not specced. This doc is Phase 1 only: it has to exist before anything
else does.

## What's already there, found by reading the code, not assumed

The actual surprise while scoping this: "design your own space" is **already fully built** and already
deployed -- `HouseStore::list_floor`/`place_floor`/`take_floor` (`house.cpp`), a cap of 8 items, scoped per
`map_instances.instance_id`, visible to visitors, owner-only to change. Wired client-side too
(`interact.js`'s `tryFloorPlace()`: standing in your own house holding an item, Enter places it on any
clear interior tile). `CLAUDE_IDEAS.md`'s item 3 ("place more than one thing") described this as unbuilt
-- it wasn't a gap in the game, it was a gap in that survey. Phase 1 does not need to build this. It needs
to give you an empty place to use it in.

The other finding, more important for scoping: **every player's house is the same hardcoded room shape.**
`ownHouseSpace`/`demoHouseSpace` (`interiors.js`) are built *once*, at script load, from one shared
`fillHouse()` function -- what makes a house "yours" is only the data layered on top (your wall slot, your
8 floor items, your two Studio paintings), never a different physical layout. There is no per-instance
tile storage anywhere in this game yet. That rules out "new accounts get an empty plot, old ones keep
their furnished room" as a real option -- the shape is shared, so changing it changes it for everyone who
hasn't placed anything incompatible with it, which today is close to everyone (the backup's own row counts
put total live players at one).

`place_floor`'s own refusal check (`house.cpp`) is pure coordinate math -- bounds and "is another placed
item already there" -- it never looks at what tile is actually drawn underneath. Changing the room's visual
tile palette costs nothing on the daemon side at all.

## Decision 1 — what "empty" means

Keep: the door, the friends/open/locked latch sign, the wall slot, and the two Studio art frames (portrait
+ painting) -- these are built features with their own purpose, not decoration-for-decoration's-sake, and
nobody asked for them to go. Remove: the table, both chairs, both rug pairs, both "shelf" workbenches, both
"bookcase" crates, the flowerbed -- the fixed clutter that was never a choice. What's left is the existing
floor-item system doing the actual "design it yourself" job it was already built for, just with room to
use it in (today, the fixed furniture already eats most of a 14x14 interior).

## Decision 2 — outdoor styling, not just an empty room

Brandon's own word is "property," and he described friends visiting from "the main world" -- not from a
featureless box. Swap the base ground tile from `T_FLOOR` to grass (`T_GRASS_A`/`T_GRASS_B`, the same
tiles the outdoor world already uses -- `drawTile()`'s grass branch picks a variant cell per-tile, not from
neighbor context, so it renders correctly baked into an interior-shaped space with no special-casing) and
the border from `T_IWALL` to `T_HEDGE` (already solid, already tall-sorted, already used outdoor at Hall
Lawn -- the same collision/rendering, just the boundary of a plot instead of a wall of a room).

One exception: the portrait and painting frames are drawn as image overlays anchored to the wall row
(`STUDIO_PORTRAIT`/`STUDIO_PAINTING`, y=0, x 4-5 and 10-11). A framed painting hanging on a hedge reads
wrong. Keep a short run of real wall tile (x 4-11, y=0 -- exactly the span both frames already occupy) as
a small fixed shed-back wall; hedge everywhere else on the border. Everything about the two frames
(fetching, drawing, who can open the easel from where) is completely unchanged -- only the tile under them
stays what it already was.

## Decision 3 — this changes the shared room for every player, not per-account

Because the physical layout isn't stored per-instance, this *is* a uniform visual/layout change -- not a
"new accounts only" feature. Already-placed floor items are unaffected (`placed` rows are coordinate-keyed,
not tile-type-keyed, so they just sit on grass instead of floor now) and the wall slot/portraits are
unaffected (they don't depend on the base tile at all). The only live player right now sees their room
redecorated under them, not reset -- nothing of theirs is deleted. Demo house is explicitly NOT touched
(stays the furnished example space a fresh guest or a first-time visitor sees, same as today, so there's
still a "here's what a lived-in one could look like" reference somewhere).

## Decision 4 — the floor-item cap

Currently 8, sized for a small cluttered room that only had a few clear tiles anyway. An emptied, grass
plot has dramatically more clear space. Proposed: raise the cap to 24 (`place_floor`'s own hardcoded `>= 8`
in `house.cpp`) -- a number, not a promise; easy to retune later once someone's actually filled one.

## Out of scope, said plainly

No gather spots on the plot, no tools, no buildable fences, no new instance type, no schema change, no new
daemon route. No change to visiting (friend doors, visit policy) -- that system already works and nothing
here touches it. No change to the demo house, the lab, the mine, or the workshop. Phases 2-5 are a roadmap
entry, not a spec.

## Tests

- `test_studio.sh`/house-area daemon tests: `place_floor`'s cap enforces at 24, not 8 (mutation-check: the
  25th placement still refuses).
- `client_files_unit.js`: the owner's house no longer contains `T_TABLE`/`T_CHAIR`/`T_CRATE`/
  `T_WORKBENCH`/`T_FLOWERBED` (new assertion against `ownHouseSpace.tiles`); the demo house still does
  (unchanged, so a regression there is caught too); the shed-wall span (x 4-11, y 0) is still solid and
  still `T_IWALL` there specifically; everywhere else on the border is `T_HEDGE`; the portrait/painting
  frame coordinates are unchanged from `CLAUDE_STUDIO_4.md`'s own pinned values.

## Deploy

Client-only except the one daemon constant (the floor-item cap). Holds for a literal `deploy`, same as
always.

---

**Say `BUILD property phase 1` to build this as written, or tell me what to change first.**
