# Your own property, Phase 1: an empty plot — built, tested, DEPLOYED

Brandon: `BUILD property phase 1`, approving `CLAUDE_PROPERTY.md` as written. First slice of a 5-phase
roadmap (gather/craft tools, buildable structures, new biomes, and a town economy are explicitly later,
unspecced phases). This phase only: an empty, outdoor-styled plot in place of the fixed furnished room.

## What changed

- **`commons/js/interiors.js`**: `buildInterior(w, h, fillFn)` grew two optional trailing params,
  `baseTile`/`borderTile` (default `T_FLOOR`/`T_IWALL`, so every existing caller -- demo house, lab,
  workshop -- is byte-for-byte unaffected). A new `fillProperty(t, w)` replaces `fillHouse` for the
  owner's own space only: door and wall slot, nothing else. `ownHouseSpace` now builds with
  `fillProperty` + `T_GRASS_A`/`T_HEDGE` instead of `fillHouse` + `T_FLOOR`/`T_IWALL`. `demoHouseSpace`
  is untouched -- still `fillHouse`, still the furnished room, still what a fresh guest or a first-time
  visitor sees as a worked example.
- Removed from the owner's own property: the table, both chairs, both rug pairs, both "shelf"
  workbenches, both "bookcase" crates, the lamp, the flowerbed -- the fixed clutter that was never a
  choice. Kept: the door, the latch (visit policy), the wall slot, and the two Studio art frames
  (portrait + painting) -- functional features, not decoration. Per Brandon's own call, no effort went
  into making the frames look at home on a hedge instead of a wall; they still work exactly as before
  (fetch, draw, open-the-easel-from-here), just on a different backdrop.
- **`commons-daemon/src/house.cpp`/`.hpp`**: the floor-item cap, `place_floor`'s own hardcoded check, is
  8 -> 24. The old number was sized for a room where fixed furniture already ate most of the clear
  tiles; an emptied grass plot has far more of them.

Not touched: the daemon's placement geometry (bounds check, occupied check) is pure coordinate math that
never looked at the underlying tile type, so none of it needed to change for the new palette. No schema
change, no new route, no new instance type, no change to visiting (friend doors, visit policy).

## Tests

`client_files_unit.js` grew from 129 to **136 checks**: none of the seven removed furniture tile ids
appear anywhere in `ownHouseSpace.tiles`; all seven still appear in `demoHouseSpace.tiles` (the
regression guard that would catch an accidental change to the wrong house); the property's ground is
`T_GRASS_A` and its border is `T_HEDGE` (spot-checked clear of the door/wall-slot/latch); the demo house
still has plain `T_FLOOR`/`T_IWALL`; the wall slot is still at (7,0); the hedge border is still solid.

`test_house.sh` grew from 51 to **53 checks**: the cap-8 test became a cap-24 test, with the boundary
checked for real -- 22 placements via a loop, each one's actual landing verified against a fresh `GET`
(not just "the loop didn't error," which would have passed even at the wrong cap, found while writing
it), then an explicit 24th that must succeed and a 25th that must be refused with `"full"`.

Full daemon suite: **26 files, 0 failed**. Verified visually in the browser preview (console-injected
`isBound`/`enterInterior("house", ownHouseSpace, ...)`, since the preview has no running daemon to bind
against for real): the plot renders as open grass with a hedge border, no furniture anywhere, and the
wall slot + both Studio frames render correctly on the new backdrop with their nameplates intact.

**Mutation-checked**, each restored after confirming the right failure:
1. Reverted `ownHouseSpace` to the old `fillHouse` + default tiles -- the three new "no furniture / grass
   / hedge" checks all failed as expected.
2. Reverted the cap to 8 -- caught not by the final "25th refused" assertion (which passed either way,
   the exact bug this rewrite was checking for) but by the new mid-loop assertion: only 7 of 22 landed,
   and the explicit 24th got `409` instead of `200`.

## Limits, said plainly

This is a uniform, shared-template change, not a per-account one -- there's no per-instance tile storage
in this game yet, so there was no way to give only *new* accounts the new layout. The one real player
today sees their room redecorated under them; nothing of theirs (wall slot, floor items, paintings) is
deleted, since none of that is keyed to the tile type underneath it. Phase 1 adds no way to gather
anything new, craft anything, or place a structural (collidable) item of your own choosing -- "design it
yourself" today means the floor-item system that already existed, now with 24 slots and an open plot to
use them in, nothing more. Phases 2-5 remain an unspecced roadmap entry.

## Deploy

Deployed 2026-10-01, revision `90ff2ea`. `verify.sh`: 37 checks, 0 failed, all 13 tables unchanged
against the pre-deploy backup. Confirmed live by content: the served `js/interiors.js` contains
`fillProperty`/the `T_GRASS_A, T_HEDGE` call, not just a 200 status.
