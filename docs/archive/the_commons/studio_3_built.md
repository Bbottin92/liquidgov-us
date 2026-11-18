# The Studio, slice 3: map sources, the bench, the notebook — built, tested, NOT deployed

Brandon: "lets do map sources and bench" (`BUILD studio 3`), the trigger `claude_studio_3.md` itself
names. **Client only, plus the one daemon field that file names.** Slices 1 and 2 are untouched as
features: gather/bench/mix/collect, the frozen catalog, the orphan reap, the portrait endpoints and the
easel all work exactly as `studio_1_built.md` / `studio_2_built.md` describe them. No new schema table,
no new route, no civic edit, no nginx edit, no 2Hz studio poll.

## The one daemon change

`GET /presence/studio`'s `discoveries` objects gained `display_name`, `real_range_note` and
`history_note` (a plain join onto `recipes`, `StudioStore::discoveries_of`), and the response gained a
top-level `recipe_count` (`StudioStore::recipe_count()`: `COUNT(*) FROM recipes WHERE duration_s IS NOT
NULL`, i.e. the 13 bench recipes, not per-player). This is the notebook's whole data source: a
discovered line prints straight from the response, and `recipe_count - discoveries.length` is how many
"?" lines to print, with no second route that would leak undiscovered names.

## The frozen lab table

`commons/js/interiors.js`'s `LAB_RECIPES` shrank to the three untouched trophy lookups (copper, iron,
chalk — prompt still Look, same grant as always). The other five stations changed behavior on the same
tiles, no new interactable type invented beyond what each actually needed:

- **vinegar / tannin / alum** grant nothing anymore. A new `labStock` interactable type just shows a
  fixed toast, **once per station per session** (a small `labStockToasted` map in `interact.js` — the
  reading taken for "toast once this session": a second Look at the same station is silent rather than
  repeating the line every time, since claude_studio_3.md doesn't say whether a repeat should re-toast).
  People who already hold `vinegar_jar` / `oak_gall_tannin` / `alum_crystal` keep them; those item rows
  are untouched and still placeable.
- **lampblack / gum** became `studioGather` spots (`lab_soot` / `lab_gum`), the same mechanic as an
  outdoor marker: a bound Look posts to `/presence/gather`, a guest gets a local session cooldown.

## The frozen map spots — six coordinates nudged, all others exact

`commons/js/world.js`'s three existing pickups reuse their exact tiles as `studioGather` markers, no
second marker: `north_woods_galls` (17,6), `shore_shells` (62,73), `orchard_keg` (56,10) — the orchard
tile stops granting `cider_apple` (people who already hold one keep it) and now gives `vinegar` on a
1-day cooldown, toast "The keg." Two Workshop Lane crates (16,28) and (17,30), previously decorative,
became `workshop_iron` / `workshop_copper` with no tile change.

The frozen table's six *new* spots landed at their literal coordinates inside
`world_scatter_unit.js`'s own 1-tile clearance around a pre-existing P3 scatter prop (a flowerbed, a
bench, or a low wall) — three of the six literal coordinates are themselves scatter-flowerbed tiles.
Per `claude_studio_3.md`'s own fallback rule ("move one tile onto grass or dirt and say so in the
writeup"), each was nudged a few tiles onto clear ground in the same region:

| Spot | Spec coordinate | Built at | Why moved |
|---|---|---|---|
| `north_woods_twigs` | (14,8) | (5,9) | inside the scatter bench's clearance at (15,7) |
| `garden_madder` | (52,44) | (49,44) | that tile IS a scatter flowerbed itself |
| `garden_weld` | (66,44) | (69,47) | also a scatter flowerbed tile itself |
| `garden_woad` | (52,54) | (49,54) | inside a scatter low-wall's clearance |
| `quarry_ochre` | (4,74) | (8,72) | inside the low walls fencing the quarry pit |
| `quarry_alum` | (18,74) | (14,72) | same low-wall clearance |

Quarry's two still get no tile change (dirt stays dirt, "the Look is enough"); the other four use
`T_FLOWERBED`, the same coloured-pile tile the Orchard's apple pile already used. Placed from a new
`addStudioGatherMarkers()`, called right after `dressRegions()` (not folded into it):
`test_world_scatter.sh` pins `scatterRegions()` as the literal last statement of `dressRegions()`, and
that invariant is about scatter's own "only paints plain ground" guarantee, which this code doesn't
touch — it earns its own name instead of being smuggled onto the end of that function.

`WORLD_PICKUPS` is gone outright, not left dead: all three of its old entries converted to
`studioGather`, and nothing else read it.

## The Workshop room

Two more interactables on the existing workbench tiles (no new tile, no new art): **Bench** at (3,2),
prompt Make, and **Notebook** at (10,2), prompt Read. The sign now reads "The bench is on the left. The
book is on the right." Both overlays are the same full-screen-DOM shell the easel already uses
(`#easel, #bench, #notebook` share one CSS block); Esc closes whichever is open, and `input.js`'s
movement-suppression check (already gating on `#easel.open`) now also gates on `#bench.open` /
`#notebook.open`.

**When the client fetches** (`commons/js/studio.js`'s `netFetchStudio()`, guarded on `isBound` so a
guest never calls it): once after a bound join (`net.js`'s `netApplyJoinJson`, mirroring
`inventoryLoaded`'s own once-per-bind shape); once on entering the Workshop
(`interact.js`'s `workshopDoor` branch); and at most every 30s while the bench overlay is open
(`tickStudioOverlays`, ticked from `main.js`'s `frame()` like `tickToast`/`tickPlaceName` — never from
the 2Hz presence tick). Opening the bench itself does not force an extra fetch: by the time it's
reachable, the workshop-enter fetch has already run.

**"A batch is ready."**: a new `#batchReady` line, same pill family as `#placeName`/`#connStatus`,
driven only by the cached `studioState` (`tickBatchReady()`, also ticked every frame) — never its own
fetch. Collect still only works from the bench.

**The bench**, bound: open batches with a countdown from the server's own `ready_ms` (Collect button
once `ready_ms <= now`); every `makeable` recipe with a Start button (`POST /presence/bench`); a side-mix
row. **The mix row's reading taken**: rather than free-form text entry, it's a chip per held item (from
the studio GET's own `inventory` list) that toggles into the selection (up to 4, the same item clickable
more than once since the daemon's own canonical key keeps duplicates); Try sends the array and shows the
returned text as a toast, then clears the selection. `POST /presence/mix` never starts a batch and never
writes a discovery either way.

**Guest bench**: claude_studio_3.md says "the same lists from RAM, and the only action is 'Sign in on
LiquidGov to start a batch.'" A guest has no client-side recipe catalog at all (the whole point of "let a
guest call gather... studio" being a Do-Not is that nothing here ever reaches the daemon) — the reading
taken is that "the same lists" means the same rendering path, fed from empty guest state, plus that one
line; there is nothing to actually list. Same reading for the **guest notebook**: "Sign in on LiquidGov
to keep a notebook." with no discovery list.

**The notebook**, bound: one line per discovery (`display_name -- real_range_note history_note`, printed
straight from the daemon's own text, no client-side rewording), then one `?` line for each of
`recipe_count - discoveries.length` undiscovered recipes. No free text anywhere new.

## Inventory glyphs

`PLACED_ITEM_INFO` gained 27 entries — the 11 gatherable raw materials plus all 16 bench
pigments/intermediates, since a player can reach any of them this slice via the bench UI, not just the
11 the map hands out directly. Colour is the catalog's own `pigment_hex` where one exists, a plain
earth/ink tone otherwise; no new bake-time art was added to `sprites.js`'s `paintItemAtlas` for any of
them, so they all fall through to its existing generic textured square, same as any slug that was never
special-cased there.

## Tests

`world_scatter_unit.js` (**31 checks**, up from 27): all thirteen studio-gather markers present with the
right spot/x/y (the outdoor eleven plus the two lab ones once `interiors.js` loads on top), the workshop
fixed to name its four interactables (sign/easel/bench/notebook) instead of a stale
"exactly one interactable" assertion that has been silently wrong since slice 2 added the easel and
nobody updated this file. `test_world_scatter.sh`'s own wiring check (`scatterRegions()` is the literal
last statement of `dressRegions()`) is what caught the marker code being in the wrong place the first
time; moving it to `addStudioGatherMarkers()` fixed it.

`client_files_unit.js` (**96 checks**, up from 81): the tenth script tag (`js/studio.js`) in order; the
bench/notebook markup and the `#batchReady` line exist; `openBench`/`closeBench`/`openNotebook`/
`closeNotebook`/`netGather`/`netFetchStudio` all exist after load; `WORLD_PICKUPS` is gone, no
`worldPickup` interactable survives; all eleven outdoor + two lab `studioGather` spots are wired; the lab
table's three categories (`labStation` / `labStock` / `studioGather`) match the frozen table exactly; a
guest opening the bench, the notebook, or gathering never calls `fetch` at all.

`test_client_files.sh`'s python catalog-parity check now reads `STUDIO_GATHER_SPOTS` from `studio.js`
instead of the retired `WORLD_PICKUPS`, and asserts 3 lab + 13 gather = 16 client slugs, all real catalog
rows, `lab` and `gather` disjoint.

`test_studio.sh` (**92 checks**, up from 88): the four new assertions are the one daemon change --
a discovery's `display_name`/`real_range_note`/`history_note` and the fixed `recipe_count: 13`.

Full suite: **26 test files, 0 failed, 344s total**, run after every fix below (not just once at the
end).

## Two real things the tests caught, not just asserted

1. Six of the frozen map table's literal coordinates conflict with `world_scatter_unit.js`'s existing
   1-tile clearance rule around every interactable — three of them are themselves pre-existing scatter
   flowerbed tiles. Found by running the test, not by inspection; fixed by nudging each onto clear
   ground (table above) rather than weakening the clearance rule, which exists to keep a marker legible
   and not visually buried in decoration.
2. `test_world_scatter.sh`'s own wiring assertion ("`scatterRegions()` is the last thing `dressRegions()`
   does") turned my first draft (marker code appended inside `dressRegions()`, after the
   `scatterRegions();` call) into a failure. The fix was a genuine improvement, not a workaround: a
   separate `addStudioGatherMarkers()` function, called right after `dressRegions()` returns, keeps that
   function's own contract intact and gives the new code its own name instead of hiding inside a
   P3-era function it has nothing to do with.

## Limits, said plainly

- No pixel-perfect bench/mix layout pass — chips and rows, not a polished UI. Same "no headless-browser
  screenshot" limit slice 2 already had: the structure is pinned by `client_files_unit.js`, not a
  rendered screenshot.
- The mix row's chip-toggle selection (pick from held items, click again to remove) is the reading taken
  for "up to 4 held items" — not a free-text or drag-and-drop picker.
- Six map coordinates differ from `claude_studio_3.md`'s own literal table (moved onto clear ground; see
  above). Everything else — the lab table, the two crate spots, the three reused outdoor spots, the
  bench/notebook tiles — matches the spec exactly.
- No client change deploys anything by itself: `commons/js/studio.js` and the `commons/index.html` /
  `style.css` additions are inert until the client bundle is synced.

## Deploy

Not deployed. `studio.md`'s own line: slices 1, 2 and 3 deploy together, one later `deploy` word. That
deploy is daemon **and** client: drift-check, an online DB backup, the daemon restart (picks up the
schema/catalog via its own `ensure_*`/`ensure_catalog` — slice 3 added no schema at all), and a client
sync so `commons/js/studio.js` and the new markup actually reach the droplet. A live check with no bound
account can only show `403 not_bound` on the studio routes, same limit slice 1 and 2 already had.
