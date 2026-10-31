# The Studio, slice 4: the rest of the ladder, and the second frame — built, tested, NOT deployed

Brandon: `BUILD studio 4`, the trigger `CLAUDE_STUDIO_4.md` itself names. Daemon and client, exactly
that spec's four decisions. No civic edit, no nginx edit, no new schema table, no new `kind`, no catalog
edit. Slices 1-4 are all in the tree and still not deployed; one later `deploy` covers all four.

## Decision 1 — the palette indices, now all thirteen

`commons-daemon/src/studio.cpp`'s old four-entry `CHARCOAL[]` grew into a fixed 13-entry `Pigment`
table (index, slug, hex, unlock recipe), the exact numbers `CLAUDE_STUDIO_4.md` specifies, declared once
and mirrored verbatim on the client (`commons/js/easel.js`'s `STUDIO_PIGMENTS`). Indices 1-4 keep their
old meaning exactly (`unlock_recipe = nullptr`, never discovery-gated -- the stub still has to be
paintable before any collect); 5-13 each name the **recipe** slug that unlocks them, not the item slug,
since those diverge twice in `catalog.sql` (index 8 is item `carbon_ink_black`, recipe `carbon_ink`;
index 9 is item `iron_gall_blue_black`, recipe `iron_gall_ink_studio`).

`save_portrait`'s cost/lock loop became one shared private method, `save_canvas(username, kind, pixels,
now_ms)` -- `save_portrait` and the new `save_painting` are both one-line wrappers over it, per the
spec's own instruction not to copy the loop. The lock check: any index not in the table (14 and up) is
`locked_index` unconditionally; 1-4 are always legal; 5-13 need this player's own `discoveries` row for
that pigment's recipe. The cost formula is untouched (`ceil(pixels/200)`, no refund, one transaction,
short stock rolls the whole save back) -- it now just runs over up to 13 slugs instead of 4.

`palette_held()` (renamed from `charcoal_held`) returns 1-4 always, plus each of 5-13 only if
discovered -- this is what both canvases' own GET/POST responses send back, and what the client's
`renderEaselPalette()` actually renders as swatches (never the fixed `STUDIO_PIGMENTS` table itself,
which exists only so the canvas can render ANY stored byte correctly, including a friend's undiscovered
pigment).

**A real rendering bug found while doing this, not asked for by the spec but implied by it**: the old
`fillIndexed()` fell back to hardcoded black (`"#1a1a1a"`) for any nonzero index `easelHex()` didn't
recognize. That was harmless while 1-4 were the only legal indices ever stored, but decision 1's own
line ("an index the client table does not know draws as paper, not as black") makes it a real bug now a
13-entry table exists and a stray or future byte could still fall outside it. Fixed: unknown nonzero
draws as paper.

**A real interaction bug found the same way**: `smudgeValue()`'s Bayer dither was always applied by
`easelStamp()` whenever the tool was "smudge", regardless of the selected colour. For colour 5 and up,
`smudgeValue`'s own "lighten within 1-3" branch never fires (its guard is `colour >= 1 && colour < 4`),
so it silently returned 0 (erase) on every dark cell of the 4x4 pattern -- selecting a lake with the
smudge tool would have painted a half-erased stroke instead of the solid stamp decision 4 asks for
("a selected index of 5 or more stamps solid, same as the pen"). Fixed in `easelStamp()`: smudge only
dithers for colour < 5.

## Decision 2 — the second frame at (4,0)

`commons/js/interiors.js` gained `STUDIO_PAINTING = { x: 4, y: 0, w: 2, h: 2 }` and one interactable,
`{ type: "studioPainting", x: 4, y: 0, prompt: "Draw" }`, on `ownHouseSpace` only -- the demo house does
not get it, same split the portrait already has. No tile changed: y=0 is still the solid wall (overlay
only, exactly like the portrait), (4,1) and (5,1) stay plain floor. A visit toasts "A small painting."
and does not open the easel.

The wall bitmap is a second, independent buffer end to end -- `commons/js/easel.js`'s `portraitWall` /
`paintingWall` state objects (`{buf, shown, gen, frame}`), picked by `wallStateFor(kind)`. `loadStudioWall`
and `paintWallState` are the one shared implementation both `drawStudioPortrait` and the new
`drawStudioPainting` call, each against its own state -- CLAUDE_STUDIO_4.md decision 2's own line ("do
not reuse the portrait's wall buffer") is enforced structurally, not by convention: there is no code path
that could write one canvas's bytes into the other's `buf`.

## Decision 3 — the second canvas on the daemon

`commons-daemon/src/studio.hpp` gained `painting_of()` (read-only, `find_player_id` not
`find_or_create_player` -- a mere read must never conjure a row) and `save_painting()`. Routes in
`main.cpp`:

- `GET /presence/painting/painting` -- bound only, never calls `ensure_portrait` or its equivalent,
  never inserts. No row: the route itself (not the store) fills in 1024 zeros for the response, since
  `painting_of` correctly reports `found=false` with an empty `pixels` vector and the caller is what
  decides what a "no row yet" body looks like.
- `POST /presence/painting/painting` -- no ensure step at all. `save_canvas`'s own existing
  not-existed-then-INSERT branch is what creates the row, and only on an **accepted** save (even an
  all-zero one, which costs nothing) -- a rejected save (`locked_index` / `insufficient`) inserts
  nothing, proven by test, not just asserted.
- The visit route (`GET /presence/painting/<owner>/portrait|painting`) is one function for both kinds
  now: exact 9-character suffix match (`/portrait` and `/painting` are both 9 characters), same
  `portrait_readable` visibility rule, `404 no_painting` for a friend who never touched their second
  canvas. That GET does not insert either.

`last_paint_ms` (the 5-second save gap) is the same shared `std::map<std::string, long long>` for both
kinds, unchanged code -- a portrait save blocks a painting save and the reverse, proven by test.

## Decision 4 — which canvas the easel edits

`openEasel(kind)` now takes the canvas: the workshop table and the portrait frame both call it with no
argument (defaulting to `"portrait"`, so every slice-2 call site is untouched), the new frame calls
`openEasel("painting")`. `#easelTitle` flips between "A small portrait" and "A small painting" on open.
One overlay, one `#easel` element, one `easelBuf` -- opening either canvas loads only that bitmap; saving
writes only that wall buffer. Guests never reach `openEasel("painting")` in practice (they have no own
house, so the interactable is never reachable as its owner), and the code path stays portrait-only for
guests regardless.

Swatch labels: 1-4 stay Black/Grey/Light/Pale (unchanged), 5-13 use the catalog's own display names via
`STUDIO_PIGMENTS`. The budget note keeps the old collective "Charcoal left: about N pixels" line while
the open palette is still charcoal-only (`easelPalette.length <= 4`), and switches to naming the
selected pigment ("Shell white left: about N pixels.") once it can hold more than that -- the reading
taken for the spec's own "once the open palette can hold more than charcoal" qualifier.

## Tests

`test_painting.sh` grew from 60 to **106 checks**. The new ones: unbound 403 on both new painting
routes; own GET with no row (1024 zeros, no insert, no charcoal grant -- checked against alice's actual
pre-existing charcoal balance, not an assumed zero, since her portrait tests earlier in the same file
already spent and restocked it); index 14 locked on either canvas; index 5 (shell white) locked with no
discovery even though it is now a real table entry, `409 insufficient` (not locked) once discovered but
out of stock; the first **accepted** painting save is what inserts the row, a rejected one inserts
nothing; a painting save never touches the portrait blob and the reverse; the shared 5-second gap blocks
across kinds in both directions; the full portrait visit-rule set repeated for the painting kind (friend,
stranger, `open`, `locked`, civic frozen fails closed, a friend who never touched the canvas gets `404`
and that GET does not insert).

`client_files_unit.js` grew from 96 to **105 checks**: `drawStudioPainting` exists; the second frame is
pinned at (4,0) with `prompt: "Draw"`; the owner's house has it and the demo house does not; (4,1)/(5,1)
stay non-solid; the 13-pigment table's first and last entries match the daemon's; a guest's palette is
exactly the four charcoal indices; opening from the workshop table fetches the portrait URL and opening
the new frame fetches the painting URL (checked by URL, not by trusting the code); a guest asking for
`openEasel('painting')` still never calls `fetch`.

`world_scatter_unit.js` unaffected (31 checks, unchanged) -- this slice touches no map tile or outdoor
interactable.

**Mutation-checked**: the discovery-lock check (`if (p->unlock_recipe && !has_discovery(...))`) disabled
-- 6 of `test_painting.sh`'s new checks failed exactly where they should (the undiscovered-index-5 cases
and the ones that depend on it staying locked); restored and reconfirmed green.

Full suite: **26 test files, 0 failed**, run after every fix above, not just once at the end.

## Two real things the tests (and re-reading the spec closely) caught

1. My first draft of two "no charcoal grant" assertions assumed alice's `charcoal_black` balance was 0
   at that point in the file. It wasn't -- her own portrait tests earlier in the same run had already
   spent it down and restocked it to 2. Fixed by capturing her actual balance before the check and
   asserting it stays unchanged, not asserting a guessed absolute number (the same "assert the
   precondition" lesson this suite has hit before).
2. The frozen-civic "fails closed" check first came back `200`, not `403`. Root cause: `bob` (alice's
   established friend) already had a hot 15-second friends-cache entry from the visibility checks run
   moments earlier in the same file, so freezing civic afterward tested nothing -- the daemon just reused
   the cached "yes". Fixed by using `dana`, a freshly created account whose very first visit check is the
   frozen one, mirroring how `test_open.sh` already does this correctly for its own fail-closed case.

## Limits, said plainly

- No pixel-level rendering test for the two client-side fixes (paper-not-black fallback, smudge
  restricted to charcoal). Both are code-review-correct and match the spec's own wording; the stubbed-DOM
  harness records draw calls, not pixel output, so neither was independently exercised by a check --
  the same limit the original easel build already had for anything canvas-pixel-level.
- No screenshot of the second frame's phone layout, same limit slice 2's own portrait frame had: the
  shared `#easel`/`#bench`/`#notebook` CSS shell is unchanged by this slice.

## Deploy

Not deployed. Slices 1-4 deploy together on one later bare `deploy`: drift-check, an online DB backup,
the daemon restart (no schema change to pick up this time), and a client sync (`commons/js/easel.js` and
`commons/js/interiors.js` both changed; without the sync the new frame is a blank wall and the workshop
easel still only offers charcoal). A live check with no bound account can only show `403 not_bound` on
the new painting route, same limit every prior slice had.
