# Region scatter (BUILD objects, step B) — built, tested, DEPLOYED 2026-09-19 (d2b6859, client only)

Brandon asked to "finish populating the game with objects" and answered the question doc (`CLAUDE_OBJECTS_QUESTION.md`) with "all of that";
Claude proposed the order **B (client-only scatter) -> A (lab lookups) -> C (house furniture) -> D (pick-ups)**, and Brandon's `build` was taken as
starting step 1. **Client only.** Held for review, then deployed on Brandon's `deploy` (d2b6859): served files byte-identical to the repo, drift-check clean, `verify.sh` 23/23, CI green, daemon not restarted. (Later scatter work, e.g. the Creek/Shore banks and pier, is documented separately.)

## What it is

**168 more props** across the 10 land regions and the water, using the six tiles that already exist (tree, sit-log bench, crate, flower bed, low wall,
hedge): no new art, no new collision rule, no daemon change. The list is DATA (`SCATTER` in `js/world.js`: `{ region, tile, at: [[x, y], ...] }`) and
`scatterRegions()` places it as the last step of `dressRegions()`. It fills plain ground only (grass, dirt, the Lookout plateau; a driftwood bench may sit on water,
as the existing Creek and Shore ones do); anything else is REFUSED and listed in `SCATTER_REFUSED`, never silently overwritten.

Per region (count): North Woods 16, Lookout 12, Orchard 13, Workshop Lane 19, the Square 14, Hall Lawn 14, Lab Yard 16, Homestead Row 20, Garden 21, Quarry Ridge 19,
Creek 2, Shore 2 (the Lab Yard finally has the "couple of exterior barrels/crates" `atlas.md` promised, which the code never drew). The Creek and Shore are water:
the only additions are four driftwood benches; the bridge stays bare.

## What the scatter is not allowed to touch (enforced by a test, not by care)

The path spine (x 33-36), the bridge jog (y 68-71), every region's edge tiles (the seams between regions), the strip where a region's name is stamped, one tile around every
door / sign / interactable / the lab and mine doors, the spawn (+-2) and the passerby's waypoints. And a 4-connected flood fill of the real collision array from the spawn must reach
EVERY walkable tile (nothing sealed off) and a neighbour of every interactable.

## Tests (run by `run_all.sh`, CI has node)

- `tests/world_scatter_unit.js` (**14 checks**; loads just `js/world.js` under a stub DOM and reads the real arrays) + `tests/test_world_scatter.sh` (**7 assertions**: the table is declared before the world is built,
  `scatterRegions()` is called once and last, refusals are recorded, plus the unit's exit code and summary).
- **Mutation-checked, 9 breakages**, each fails: a tree on the spine, a crate on the Hall door, a tree on a seam, a bench under a region label, a prop outside its region, a tile listed twice,
  a tile sealed in by four trees (my first version of that mutation put a tree on the centre tile too, so it sealed nothing and "passed"; redone properly it fails with the sealed tile named), water used as scatter, a prop beside the spawn.

## A bug found on the way, in the live client (the split), fixed here

While looking at the page in a real browser the console showed `ReferenceError: forceRedraw is not defined` from `sprites.js`. The two sheet `onload` handlers assign `forceRedraw`, which was DECLARED in
`main.js`, the last script. The browser runs the `<script>` tags one after another with the event loop free in between, so an already-cached image can fire `onload` after `sprites.js` and before `main.js`; in strict mode assigning
an undeclared name throws. **Effect on visitors: benign** (the tile rebake and the sprite draw happen before the throwing line, and `main.js` starts `forceRedraw` true), but the live page has it on a fast reload.
Fix: `var forceRedraw = true;` moved to `sprites.js`, the first file that can touch it from an async callback (one declaration, none left in `main.js`).
My load-order test could not see it (its stub `Image` never fired `onload`): `client_files_unit.js` (now **55 checks**) fires every image `onload` after EACH script in order, with the sizes the game checks for; before the fix it fails naming both handlers, after it passes.
A scan for the same pattern (a name assigned in an earlier file than the one that declares it) found nothing else.

## Real browser

Served the client, rendered the baked world (`baked`, the same canvas the game blits): 168 placed, 0 refused, both halves of the map look right (trees, logs, crates, flower beds, low walls, hedge frame at the Hall, boulders at the Quarry, driftwood in the water). The region-by-region
check is the flood fill; the visual check is an overview, not a walk-through with a player (no daemon was involved; nothing about networking changed).

## Deploy (when Brandon says `deploy`, after review): client only, the two-step protocol

Step 1: `drift-check.sh --tree client` will list `js/world.js`, `js/sprites.js`, `js/main.js`. Step 2: `sync-client-to-droplet.sh`, then verify (served files byte-identical, page loads, `verify.sh`, no daemon restart).
`index.html` and `style.css` do not change. Rollback: re-sync the previous client revision.
