# The Studio, slice 2: charcoal easel and the portrait — built, tested, NOT deployed

Brandon: `BUILD studio 2`, the slice `studio.md` names after the daemon core. **Portrait endpoints, the
starter stub, the easel overlay (including a portrait-phone layout), and the house frame on tiles pinned
against the real house.** Palette is charcoal only. No bench screen, no notebook, no world markers, no
second painting, no droplet.

Slice 1 is untouched as a feature: gather, bench, mix, collect, the frozen catalog, and the orphan reap
(which already deletes `paintings`) stay as `studio-1-built.md` describes them.

## What "the portrait" is

A 32×32 palette-index bitmap, 1 byte a pixel, index 0 empty. The client sends indices, never a PNG.
Nearest-neighbour on the wall, labelled "a small portrait", so it does not read as a broken canvas.

`GET /presence/painting/portrait` and `POST /presence/painting/portrait` are the owner's. Both are bound
only (`403 not_bound`, no guest fallback, no nginx change). The first touch of either creates the blank
portrait and grants the stub, in one transaction. A second touch does not grant again.

`GET /presence/painting/<owner>/portrait` is the visit read. It does not create a row. It is not
`/placed`. See visibility below.

Server clock only. A client `updated_ms` in the body is ignored; the row stores `now_ms()`.

## The stub

2 units of `charcoal_black` (2 × 200 = 400 pixels), once, free. The three greys are not in the stub.
They are on the easel, disabled, until the player actually holds them. Open decision 13: the grant is
the first portrait row, not "only if black is zero", so a player who already benched charcoal still
receives the stub once. The inventory cap of 99 still applies.

## Cost (never refunds)

On save, for each charcoal colour, count pixels whose **new** index is that colour and whose **stored**
index was not. Charge `ceil(count / 200)` units of that colour. Index 0 is free and refunds nothing.
Repainting a pixel with the colour it already has costs nothing. Moving the same count of pixels to a
new place costs again (this is the formula `studio.md` preferred over a net count, which would let you
redraw the portrait from one stub). Recolouring black to grey charges grey only and does not return the
black.

One transaction: reject a locked index before any write; deduct with `UPDATE ... WHERE quantity >= ?`
and roll the whole transaction back if any colour is short. Open decision 11 is the ceil, because
"≈ 200 pixels" did not say what a single pixel costs.

## Unlocks, this slice

Indices 1–4 are the only paint the easel will store. Anything else, even one pixel inside an otherwise
legal bitmap, is `400 locked_index` and charges nothing. A legal index the player does not hold is
`409 insufficient`. Open decision 13: this is not the `discoveries` table. The stub has to be usable
before any collect, and no pigment outside charcoal exists on the easel until a later slice.

Saves are at most one per 5 seconds (open decision 12, the same number as side-mix). A parsed bitmap
counts toward that even if it is then rejected. A malformed body does not.

## Who can see it

Owner always. Otherwise `portrait_readable` asks the existing visit helper, with two extra refusals the
helper alone would not make (open decision 14):

- civic returned no friends list: refuse. A GET is not "already inside". Failing open would show the
  portrait to whoever asks while civic is down.
- `open` policy does not admit a stranger. A friend under `open` still can. `locked` admits nobody but
  the owner. A friend can read it when the owner is offline.

Usernames may contain `.` and `-` (civic's own alphabet). The owner segment is that alphabet only, max
64, then a player lookup. It is never a path.

## The house frame

Pinned against `fillHouse`, not the unverified candidate. The image is tiles (10,0), (11,0), (10,1),
(11,1). The civic frame is the 3×2 drawing centered on the wall slot at (7,0), so x 6–8. Shelves are
(2,1) and (3,1). The lamp is (14,1). The latch is (0,13), the door (7,15). x 10–11 is one tile clear of
the civic frame and three clear of the lamp. The floor tiles under the lower half stay walkable.

The owner's house has a "Draw" interactable on (10,0). The shared demo house does not, so a visit draws
the image and does not open the easel. The workshop easel is the table already at (7,4); Chebyshev
range 1 reaches it from the floor beside the table.

The wall bitmap is not the editor buffer. Loading a friend's portrait cannot become a save of your own.

## The easel

Full-screen DOM overlay. Pen (1 px), smudge, eraser, undo of 16 strokes. No layers, import, text, or
colour picker. Smudge is a 4×4 Bayer dither of the chosen charcoal against the next lighter grey when
that grey is actually held, otherwise against bare paper, which is how a stub of only black reads as a
grey. Erasing does not refund. Undo restores the stroke's pixels and the session budget, because an
undo is not a save.

Keyboard: Esc closes, B pen, M smudge, E eraser, Z undo, 1–4 select a colour you hold. The overlay
takes focus, and movement keys do nothing while it is open. Pointer events cover touch; targets are
44px; the canvas is `touch-action: none`.

Portrait phone: column layout, and the canvas width is `min(92vw, 100vw − padding, 100dvh − 16.5rem)`
so the tools, palette, note and actions stay on a short phone. The overlay scrolls if they still don't.
A short landscape phone puts the tools beside the canvas. There is no headless browser in this
environment; the layout is pinned by those CSS rules in `client_files_unit.js`, not by a screenshot.

Guests: a session stub of 400 black pixels, in this page only. Opening the easel does not call the
painting endpoints. "Keep" copies the bitmap into that session buffer and closes. Nothing is saved.
The "batch ready" HUD line is slice 3; there is no bench on the client yet.

A bound player's stroke is also capped at `units × 200` new pixels of each colour, so a stroke they can
see is one the server can accept. The server remains the authority on save.

## Tests

`tests/test_painting.sh` (**60 checks**), real daemon, civic stub, the test clock for the save spacing:

unbound 403; stranger read does not create the stub; stub is 2 black once and no greys; a friend gets
pixels and not the palette; wrong length; index 5 rejected; 200 new black pixels cost 1 unit and the
same picture again costs 0; moving those 200 costs again; erase refunds nothing and the repaint is
refused; a save that needs black and an unheld grey rolls the black back; 1 pixel costs 1 unit (ceil);
a second save inside 5s is 429; `open` still refuses a stranger and still allows the friend; `locked`
refuses the friend; a client timestamp is not stored; recolouring black to grey charges only the grey;
one locked index inside an affordable bitmap charges nothing; a friend named `a.b` can still read it.

The stranger is carol, and bob is not read until after the friendship exists. The friends cache is 15s
of wall clock, so a "not friends" answer from before the accept would hide it for the rest of a fast test.

**Mutation-checked**, then restored:

- Cost replaced with a net count (`max(0, new_count − old_count)`). "Moving those 200 pixels" no longer
  spent the last unit. Restored.
- Index 5 accepted. Both locked-index assertions failed (the bare pixel, and the one hidden in an
  otherwise affordable bitmap, which then charged the black). Restored.

`client_files_unit.js` (**81 checks, 0 failed**): easel.js is the ninth script, in tag order; the
portrait is the 2×2 at (10,0) on the owner's house only; the workshop table is the easel; the overlay
is a 32×32 canvas with smudge, undo, 44px targets and the phone canvas rule; a guest `openEasel` does
not call `fetch`.

This sandbox's parent `package.json` marks `.js` as ESM, so `node tests/client_files_unit.js` from the
repo fails before any check. CI checks this repo out as the root, where `.js` is still CommonJS. The
81 were run by executing the same file from a directory with no `"type": "module"` ancestor.

`test_studio.sh` and `test_studio_orphans.sh` were re-run after the portrait code and still pass
(88 and 10). So do `test_migrations` (83), `test_house` (51), `test_visiting` (22), `test_policy` (27)
and `test_open` (29). The painting routes did not change gather, bench, mix, collect, or the reap order.

`test_orphans.sh` got to 42 checks and then failed its SUSPECT section, only because this run is root:
the "unwritable backup dir" section is skipped for root, so `userf` is still a living civic account and
"every known account is gone" is no longer true. That skip is older than this slice. The REAPED lines in
the same log do list `0 paintings`, which is the studio extension still firing.

## Limits, said plainly

- No bench, notebook, gather markers, or lab conversion. Those are slice 3.
- No second frame. Slice 4.
- No headless-browser screenshot of the phone layout. The CSS and the unit checks are what pin it.
- Not deployed. Holding for a literal `deploy`. When it is, it is a daemon **and** a client sync: the
  new routes are under `/presence/` (no nginx change), and `commons/js/easel.js` plus the markup have
  to be on the droplet or the frame is a blank wall.

## Deploy

Not deployed. Drift-check, an online backup, then the usual daemon restart plus a client sync. A live
check with no bound account can only show `403 not_bound` on the new routes. Do not do any of that
until Brandon says `deploy`.
