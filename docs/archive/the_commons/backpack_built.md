# The backpack — built, tested, DEPLOYED

Brandon: `BUILD backpack`, approving `claude_backpack.md` as written (currency dropped per the earlier
AskUserQuestion answer, "Skip it for now" — no economy system exists anywhere in this game and none was
added here). Client only: no schema, no daemon route, no new external art file.

## What changed

- **`commons/js/interiors.js`**: two new tables next to `PLACED_ITEM_INFO` (not folded into it) --
  `ITEM_CATEGORY` (slug -> one of twig/leaf/stone/powder/bundle/swatch, for the ~24 slugs that used to
  fall through to sprites.js's plain generic square) and `ITEM_DESC` (one sentence per slug, all 38,
  several reusing the same real-world grounding the recipe `history_note`s already use). `renderInvHud()`
  grew one line: the new Bag button now follows the strip's own empty/non-empty rule.
- **`commons/js/sprites.js`**: `paintItemAtlas()` grew six new shape branches reading `ITEM_CATEGORY`
  (crossed-bar twig, pointed leaf, hexagon-ish stone, triangular powder heap, tied-lump bundle, and a
  highlighted swatch square). Two existing bespoke branches were extended by one slug each instead of
  duplicated: `vinegar` joins the jar/bottle glyph (`vinegar_jar`, `copper_acetate_bottle`), and
  `carbon_ink_black`/`iron_gall_blue_black` join the ink-page glyph (`iron_gall_ink_page`,
  `lampblack_ink`) -- both are the Studio-era version of the same real substance the old glyph already
  drew, not a new shape. `oak_gall`/`oak_gall_tannin`/`gum_arabic` keep their own existing bespoke art
  untouched (they were never falling through to the square, so there was nothing to fix).
- **`commons/js/backpack.js`** (new, 11th script): `openBackpack`/`closeBackpack`/`renderBackpack` (a
  grid, sorted by category-then-label, not insertion order), the shared `#itemTip` tooltip
  (`showItemTip`/`hideItemTip`, wired via `pointerover`/`pointerout` for a mouse and `click` for touch --
  click both selects the chip, matching the strip's own existing behaviour, and shows its tooltip; a
  second tap of the same chip hides it), and the `I`-key/Escape toggle (its own keydown listener, same
  shape as `studio.js`'s `wireStudioOverlays()` -- not gated behind `typingInField()`, since that function
  now gates movement OFF while the backpack is open, not the backpack's own open/close).
- **`commons/js/input.js`**: `typingInField()` grew a `#backpack.open` check, alongside the existing
  easel/bench/notebook ones, so movement is suppressed while the backpack covers the screen.
- **`commons/js/studio.js`** (bench redesign, decision 6): two small fixed tables mirroring
  `catalog.sql`'s own `recipes`/`recipe_outputs` rows -- `RECIPE_OUTPUT_ITEM` (recipe slug -> its output
  item, for the icon) and `RECIPE_DURATION_MS` (recipe slug -> its `duration_s`, for the progress bar).
  Neither `/presence/studio` response carries an output-item slug or a `started_ms` (checked against
  `main.cpp` directly: batches only ever send `batch_id`/`recipe`/`ready_ms`), so a batch's start time is
  derived client-side as `ready_ms - the recipe's own known duration` -- no route change, no new field,
  just what the daemon's own catalog already fixes for all 13 recipes. `renderBench()` grew section
  headers ("Open batches" / "You can make" / "Try something"), an icon per row (via `benchIcon()`), and a
  progress bar alongside each open batch's existing exact-time text (not replacing it --
  `claude_studio_3.md`'s "Names and waiting," is still followed to the letter).
- **`commons/index.html`**: the Bag button, `#itemTip`, and the `#backpack` overlay (title/empty-note/grid/
  Close, sharing the exact `#easel`/`#bench`/`#notebook` overlay shell) -- `js/backpack.js` added as the
  eleventh script tag, load order otherwise unchanged.
- **`commons/style.css`**: `#bagBtn`, `#itemTip`, `#backpackGrid`/`.bpChip` (a real CSS grid, not the
  strip's wrapping row), `#backpack` folded into every shared `#easel, #bench, #notebook` selector, and
  the bench's new `.benchSection`/`.benchProgress`/`#benchMix` background tint.

## Tests

`client_files_unit.js` grew from 105 to **113 checks** (`ORDER` grew to eleven files; two pre-existing
message strings that said "ten"/"nine" were corrected to "eleven" while touched):

- the backpack entry points exist after load (`openBackpack`/`closeBackpack`/`renderBackpack`/
  `backpackOpen`);
- the Bag button, `#itemTip`, `#backpackGrid` and `#backpackClose` markup exist in `index.html`;
- every `PLACED_ITEM_INFO` slug has a non-empty `ITEM_DESC` entry;
- every `PLACED_ITEM_INFO` slug either already has bespoke atlas art (a named list of the 14 slugs that
  do) or an `ITEM_CATEGORY` mapping -- nothing silently falls through to the plain square unaccounted for;
- all 13 bench recipes have both a `RECIPE_OUTPUT_ITEM` and a `RECIPE_DURATION_MS` entry;
- a guest opening and closing the backpack never calls `fetch` (pure client-side `inventory`, same as the
  strip).

Full suite: **`client_files_unit.js` 113 checks, 0 failed**; the daemon's own `run_all.sh` (29 files, this
feature touches none of them) also finished clean -- this feature has no server-side surface, so nothing
there could regress.

**Mutation-checked**, each restored after confirming the right failure:
1. Deleted `woad_blue`'s `ITEM_DESC` entry -- the "non-empty ITEM_DESC" check failed.
2. Deleted `willow_twigs`'s `ITEM_CATEGORY` mapping -- the "bespoke art or a category mapping" check
   failed.
3. Deleted `woad_blue`'s `RECIPE_DURATION_MS` entry -- the "13 bench recipes" check failed.

**Manually verified live** in the browser preview (`commons-client`, port 8090), with `inventory` and
`studioState` set directly through the console (no daemon running, so no bound player was actually
reachable):
- the Bag button appears/disappears with the strip, opens a category-sorted icon grid;
- hovering a chip shows its real description above it; clicking a chip both selects it (border, and the
  strip re-sorts it to the front on close, exactly like the strip's own existing "held" behaviour) and
  shows the same tooltip;
- `I` opens and closes the backpack; `Escape` closes it;
- the bench, with two open batches and two makeable recipes: icons render per row, an in-progress batch
  (`charcoal`, 5h of 8h left) shows a partially-filled bar with its exact "5 h" text intact, a ready batch
  (`verdigris`) shows a full bar, "Ready", and its Collect button, and the mix row sits in its own visibly
  tinted box below both list sections.

## Limits, said plainly

- The six new procedural shapes are simple pixel-block glyphs (crossed squares, stepped diamonds, filled
  blobs), not hand-tuned pixel art -- "a small set of shape categories," exactly as scoped, not "38 bespoke
  icons." They are a real improvement over one flat square each, not a finished art pass.
- The bench's progress bar is derived from a client-side duration table, not a server-sent `started_ms`.
  If a recipe's `duration_s` ever changes in `catalog.sql` without this file's `RECIPE_DURATION_MS` being
  updated to match, the bar would drift (though the exact-time text next to it, which comes straight from
  the server's own `ready_ms`, would still be correct either way -- the text is the source of truth, the
  bar is a visual aid alongside it).
- Live verification used console-injected fake state, not a real bound session against a running daemon
  (this build touches no daemon route, so there was nothing new to verify server-side); the actual
  gather/bench/collect flows themselves were already verified live in slices 1-5.

## Deploy

Deployed 2026-09-28, revision `9a0896b`. The daemon cycle (backup/sync/build/restart) ran as it always
does even though nothing in `commons-daemon/` changed -- the drift check confirmed that explicitly ("this
deploy changes nothing here"). `verify.sh`: 37 checks, 0 failed, database unchanged across all 13 tables.
The client sync (`commons/` -> `/opt/commons`, `--with-client`) then pushed the seven changed files
(`index.html`, `js/backpack.js`, `js/input.js`, `js/interiors.js`, `js/sprites.js`, `js/studio.js`,
`style.css`).
