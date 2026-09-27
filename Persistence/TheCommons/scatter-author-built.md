# Scatter, authored not hand-listed — built, tested, NOT deployed

Grok's spec (relayed by Brandon, built on his own bare `build it`): the old `SCATTER` array in
`commons/js/world.js` was a hand list of ~202 props, padded well past `world_scatter_unit.js`'s own
`total >= 150` floor -- which is exactly why the Square, Hall Lawn and the Orchard ended up dressed
*twice* (once by `dressRegions()`'s own set piece, again by `SCATTER`). Replaced with the output of a
deterministic generator. **The game still just reads the `SCATTER` array at load time — nothing is
rolled in the browser.** Client only. No civic, no nginx, no daemon, no schema.

## The script: `commons-daemon/tools/scatter_author.js`

Plain CommonJS, no dependencies. Run with `node` from the repo (the same "no `type: module` ancestor"
constraint `world_scatter_unit.js` already has).

```
node commons-daemon/tools/scatter_author.js          # regenerate and overwrite commons/js/world.js
node commons-daemon/tools/scatter_author.js --check  # exit 0 only if a fresh run matches exactly
```

**Seed**: the constant `20260927` (mulberry32). Changing the seed, or a rule in the script, moves a
prop; hand-patching a coordinate in `world.js` is exactly what `--check` exists to catch — it fails the
moment the written array and a fresh run disagree.

**Seeing the real ground**: loads `world.js` in a `vm`, the same way `world_scatter_unit.js` already
does, with the `SCATTER` array in the *source text* replaced by `[]` before evaluating it. Since
`dressRegions()`, `addStudioGatherMarkers()`, `layApproachPaths()` and `vergePaths()` are all
unconditional calls inside the file's own load-time IIFE, loading the emptied source *is* "dress, lay
the gather markers and the approach paths, then look at what's left" -- no separate orchestration needed
to reproduce that sequence.

## Legality (`legal(x, y, tileType, region)`)

Every rule the spec named, checked against the *emptied* world's own `tiles`/`solid` arrays:

- Inside the prop's own region.
- Ground: the final tile is grass, dirt or the Lookout plateau -- or water, but only for a bench, and
  only in Creek or Shore.
- Not a "clear" tile: region-edge seams, the region-name label strip, the path spine, the bridge jog,
  a 1-tile ring around every interactable (including every `studioGather` marker), a 2-tile ring around
  the spawn, a 1-tile ring around each passerby waypoint. This map is built with the *identical* logic
  `world_scatter_unit.js`'s own `mark()` uses -- deliberately duplicated rather than shared, so a change
  to one without the other shows up as a generator/test disagreement, not a silent drift.
- Not an orthogonal neighbour of a path or bridge tile -- except a bench, which may sit on that shoulder.
- Chebyshev distance ≥ 2 from every solid tile already in the emptied world (a 3×3 box around the
  candidate, including the candidate's own tile, must be entirely non-solid) **and** from every prop
  already accepted this run, region or no.

## Per-region recipes

`RECIPES` in the script is the caps table verbatim. Orchard, the Square and Hall Lawn get an empty
recipe — `dressRegions()` already gave each its own set piece (six trees, two benches, two hedges), and
doubling that was the entire "why does it look padded" complaint. A ranged cap (`4, 8`) draws its actual
count from the seeded RNG each run; a `0, N` cap is a ceiling, not a promise, so several regions came
back under their maximum (see Findings below) — that is a real, seed-determined outcome, not a bug.

Four caps ask for a **shape** (a row, a line, a broken run), which a single flat shuffle over the whole
region cannot produce on its own. Each of those has its own restricted candidate space, still drawn from
the same seeded shuffle-then-greedy algorithm, just over a narrower set of tiles:

- **Garden flowerbeds** (4–8, "one or two horizontal rows"): 1 or 2 row `y` values are drawn from the
  region's own legal middle band, then flowerbeds are placed only on those rows.
- **Quarry Ridge low walls** (4–8, "one broken run, with a gap where the lane is"): candidates come from
  a hand-defined band tracing the pit's rough perimeter — not the whole 24×20 region. The *gap* itself is
  not special-cased: the universal path-adjacency rule already excludes anything touching the mine lane,
  so the run breaks there on its own.
- **Creek and Shore trees** (4–6 each, "one line"): a single row is drawn per region, restricted to
  clearly dry ground (see Findings — this needed a real fix, not just a description).

Workshop Lane's corner-stamp exclusion (`x ≤ 3, x ≥ 21, or y ≥ 36`) and Homestead Row's door-front
exclusion (`y = 46` or `48` at any of the 8 door x-positions) are similar dedicated filters, applied to
that region's *general* candidate pass rather than the whole-region default.

## The two branches, straightened

`3121a46`'s Workshop and Lab branches both doglegged a tile or two to avoid scatter crates the old hand
list put in their way. Those specific crates don't exist in the generated list — nothing can regenerate
a prop onto a path or its shoulder — so both branches are now a single straight row: Workshop's crossbar
runs the full `(11,32)`–`(34,32)`, the Lab branch the full `(12,48)`–`(34,48)`. The mine branch, the Hall
branch, `layApproachPaths()` and `vergePaths()` are untouched.

## A real bug the generator itself found

The first run gave Shore **zero** trees against a required 4–6. Root cause: the "one line" row for
Creek/Shore trees was drawn from `y0 + randInt(2, 5)`, and both regions' water band starts at `y0 + 6`.
Water is a solid tile — a row at `y0 + 5` sits directly against it, and the Chebyshev-≥2-from-solid rule
then rejects **every single tile in that row**, since each one has a solid water tile directly south.
Creek's roll for this seed happened to land on a safer row and passed; Shore's did not. Fixed by
narrowing the draw to `y0 + randInt(1, 3)`, keeping the line's own southern neighbour at least two rows
clear of the water regardless of which value the seed draws. Found by actually running the script and
reading its output, not by inspecting the rule in isolation — the same discipline this project's test
suite has always insisted on.

## Result, this seed

**48 props total** (down from ~202): North Woods 6 trees; Lookout 2 trees (both x ≤ 29, off the
plateau); Workshop Lane 1 crate + 1 tree; Lab Yard 1 crate + 1 bench + 2 trees; Homestead Row 3 trees;
Garden 7 flowerbeds (one row) + 1 tree + 1 bench; Quarry Ridge 8 low walls (the perimeter band) + 2
crates; Creek 4 trees (one line) + 1 bench; Shore 6 trees (one line) + 1 bench. Orchard, the Square and
Hall Lawn: zero, on purpose. Several `0, N` ceilings came back low (Workshop's crates 1 of 4, Homestead's
benches 0 of 2) — that is this seed's own greedy walk running out of room or luck within its single
shuffle pass, not a second attempt being owed to it ("no attempt budget," per spec).

## Tests

`world_scatter_unit.js`: the old `total >= 150` floor is gone. In its place:

- **Caps**: every `(region, tile)` pair in the spec's own table, asserted independently against the
  actual `SCATTER` contents — including every explicit zero (Orchard/Square/Hall Lawn get nothing;
  Workshop Lane gets no benches/flowerbeds/low walls; and so on for every region's own excluded types).
- **Lane sentinels**: nine fixed coordinates, one per lane (`Workshop (25,32)`, `Lab (25,48)`,
  `Hall (50,30)`, `mine (20,69)`, `Lookout (35,18)`, `Orchard (50,9)`, `woods (4,24)`, `Garden (45,48)`,
  `ochre (10,71)`), each asserted to still be `T_PATH` or `T_BRIDGE` after everything (dressing, gather
  markers, approach paths, verges, scatter) has run. This is a check the old hand-list-based test could
  not have had at all — there was no generator to prove wrong.
- **`scatter_author.js --check`**, run as a real subprocess from inside the unit test, so "the array in
  `world.js` is exactly what this seed produces" is proven every run, not assumed once at authoring time.

**33 checks, 0 failed** (`test_world_scatter.sh`'s own wiring checks — `scatterRegions()` still the last
statement of `dressRegions()`, no prop refused — also still pass unchanged). `client_files_unit.js`:
105 checks, 0 failed, unaffected by this slice.

**Mutation-checked**, each restored after confirming the right failure:

1. `groundOk()` forced to always return `true` — no effect on this seed's actual output (the shuffle
   order happened to satisfy every region's quota using only genuinely legal tiles before ever reaching
   an invalid one). Not a wasted check: it's an honest negative result, not a false confidence — the
   *next* two mutations are what actually exercise the safety net.
2. A stray coordinate hand-inserted into `world.js`'s `SCATTER` array — `--check` and
   `world_scatter_unit.js` both failed on exactly "DRIFT (a fresh run does not match commons/js/world.js)".
3. North Woods' recipe temporarily capped at `0, 1` (violating its own spec minimum of 4) — the new caps
   check failed with the precise count and range: `North Woods/T_TREE: 1 not in [4,8]`.
4. The Hall lane's crossbar shortened by 8 tiles, uncovering lane sentinel `(50,30)` — the lane-sentinel
   check failed naming that exact coordinate and its actual (wrong) tile id.

## Limits, said plainly

- "One shuffle, then greedy, no attempt budget" is followed literally: several `0, N` caps landed under
  their ceiling for this seed. A different seed would very likely fill them out further; this one is not
  specially chosen.
- The row/line/run shapes (Garden, Quarry, Creek, Shore) are each their own hand-written candidate-space
  function, not a single generic "shape" primitive — four separate small functions, not a framework. The
  Quarry perimeter band in particular is a hand-authored polyline, not derived from anything; its
  resemblance to the old hand list's own rough footprint is deliberate (it was always tracing the same
  pit) but the specific tiles that get a low wall are the seed's choice, not copied.
- No visual/screenshot test exists for "does this look coherent" — same limit every prior slice in this
  project has had for anything not text or a collision array. Checked with a local static preview
  (`commons-client` on :8090) by eye, not asserted by a test.

## Deploy

Not deployed. Client-only (`commons/js/world.js` and `commons-daemon/tools/scatter_author.js`, the
latter never shipped to the droplet at all — it's a repo tool, not runtime code). Whenever a client sync
happens next, this rides along with it; nothing here needs its own separate deploy step.
