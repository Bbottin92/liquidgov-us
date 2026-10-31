# BUILD studio 4 — the rest of the ladder, and the second frame

Brandon asked for this file as a spec (2026-09-27). It is **not a build**.
`studio.md` names slice 4 and stops. Slices 1–3 are in the tree and not
deployed. Do not deploy this either. No civic edit. No nginx edit. No new
schema table, no new `kind`, no catalog row.

The 13 bench recipes, the bench overlay, and `GET /presence/studio` already
exist. A player can already collect every pigment. This slice is what lets
those pigments be **painted**, plus the second canvas the `paintings` table
already allowed (`kind` `'portrait'` or `'painting'`, `UNIQUE(player_id, kind)`).
Nothing reads or writes a `'painting'` row yet.

## Do not

- Retune cooldowns, yields, `PIXELS_PER_UNIT` (200), the 5-second save gap, or recipe times. That is slice 5.
- Move the portrait off tiles (10,0), (11,0), (10,1), (11,1), or move the civic frame.
- Grow the house. Drop the second frame. Add a third `kind`.
- Give the second canvas a charcoal stub, or grant anything from opening it.
- Derive palette indices from `unlocked_palette()` (that query is `ORDER BY i.slug` and would move charcoal off 1–4).
- Show an undiscovered pigment's name on the easel. Visit GETs stay pixels only: no palette, no quantities.
- Let a guest call any painting route. No 2 Hz poll. No refunds.
- Rebuild the bench, the notebook, gather, or the lab. Slice 3 already starts any of the 13 recipes.
- Edit `catalog.sql`. Binder, tannin, and iron acetate stay unpaintable (`pigment_hex` NULL). They get no index.

## Decision 1 — palette indices

**Recommended default: one fixed index per pigment, declared once, same numbers on the daemon and the client.**
Not a per-player map.

Rejected: discovery-order assignment. Stored portraits already use 1–4 as charcoal. A visitor colours the bitmap without the owner's inventory, so the index has to mean the same hex in every house. A per-player map would have to ride along on the visit GET, and a later discovery must never renumber old bytes. Not worth it. Append-only per player still desyncs two clients.

Indices 1–4 **stay** `CHARCOAL[]` in `commons-daemon/src/studio.cpp`. Do not renumber them. `save_portrait` becomes one function that takes `kind`; do not copy the cost loop.

| Index | Slug | Hex | Tier | Unlock |
|---|---|---|---|---|
| 0 | *(empty)* | paper, not a pigment | — | always. Costs nothing. Refunds nothing. |
| 1 | `charcoal_black` | `#1a1a1a` | 1 | always a legal index. Not discovery-gated. |
| 2 | `charcoal_grey_1` | `#4d4d4d` | 1 | same |
| 3 | `charcoal_grey_2` | `#7a7a7a` | 1 | same |
| 4 | `charcoal_grey_3` | `#adadad` | 1 | same |
| 5 | `shell_white` | `#f2ede4` | 2 | discovery of recipe `shell_white` |
| 6 | `yellow_ochre` | `#c9963a` | 3 | recipe `yellow_ochre` |
| 7 | `red_ochre` | `#9c4a2e` | 3 | recipe `red_ochre` |
| 8 | `carbon_ink_black` | `#0d0d0d` | 3 | recipe `carbon_ink` |
| 9 | `iron_gall_blue_black` | `#1c1c24` | 4 | recipe `iron_gall_ink_studio` |
| 10 | `verdigris_green` | `#3f8f5f` | 5 | recipe `verdigris` |
| 11 | `madder_red` | `#a72a2a` | 6 | recipe `madder_lake` |
| 12 | `weld_yellow` | `#e0c04a` | 6 | recipe `weld_lake` |
| 13 | `woad_blue` | `#2e4c72` | 6 | recipe `woad_blue` |

Order is tier, then catalog order, skipping the three NULL-hex outputs. Hex values are the catalog's. A test should fail if the daemon table drifts from `items.pigment_hex`.

**Lock.** Walk the 1024 bytes first. Index 0: skip. Any index with no row in the table above (including 14 and up): `locked_index`, and the whole save is rejected before any charge. Indices 1–4 are never `locked_index` for lack of a charcoal discovery (open decision 13 stands, so the stub still works before any collect). Indices 5–13 are `locked_index` unless this player has a `discoveries` row for **that** recipe. Holding the stack without the discovery does not unlock. One locked pixel rejects the bitmap; nothing is deducted.

**Cost.** Unchanged formula, now over the whole table. For each index `c` from 1 to 13, count pixels whose new value is `c` and whose stored value was not. Charge `ceil(count / 200)` of that slug. Same transaction. `UPDATE ... WHERE quantity >= ?`. A short stack is `409 insufficient` and rolls every deduction in that save back. Repainting a pixel the same colour costs nothing. Erase to 0 costs nothing and refunds nothing. Recolour charges the new colour only. A portrait save does not read or write the `'painting'` row, and the reverse. There is no remainder bank.

**What the easel is offered.** Own GET and own POST, for either kind, return `palette`: indices 1–4 always (qty may be 0), then each of 5–13 **only if discovered** (qty may be 0). That is the list. Do not add the undiscovered nine. `GET /presence/studio`'s existing `palette` field stays as it is; the easel does not take indices from it.

## Decision 2 — where the second frame hangs

**Recommended default: tiles (4,0), (5,0), (4,1), (5,1).** Do not enlarge the house. Do not drop the frame.

`fillHouse` is still 16×16. y=0 is solid `T_IWALL`. A 32×32 image is 2×2 tiles. The portrait did not change tiles; this frame does not either. Overlay only. (4,1) and (5,1) are `T_FLOOR` today. They stay walkable. Do not set them solid.

North band, from `fillHouse` plus the civic draw in `main.js` (3×2 centered on the wall slot, pixels covering x 6–8, y 0–1) plus `STUDIO_PORTRAIT`:

| x | y=0 | y=1 |
|---|---|---|
| 0, 15 | corner wall | corner wall |
| 1 | wall | floor |
| 2–3 | wall | shelves, `T_WORKBENCH`, solid |
| 4–5 | wall | floor |
| 6–8 | civic frame | civic frame |
| 9 | wall (the portrait's gap) | floor |
| 10–11 | portrait | portrait overlay, still floor, walkable |
| 12–13 | wall | floor |
| 14 | wall | lamp, `T_LAMP`, solid |

The only other 2-wide slot that also misses those fixtures is x 12–13. It sits flush against the portrait and flush against the lamp. Rejected: the two studio images would share an edge and read as one wide picture, and the lamp is the brighter collision.

x 4–5 is the old unverified candidate (`studio.md` before slice 2: "later, x 4-5"). Checked against this house, it overlaps nothing. It is **flush**, not gapped: the shelf ends at x=3 and the civic frame starts at x=6. The gaps left on this wall are exactly two tiles, and the frame is exactly two tiles. A one-tile gap like the portrait's gap at x=9 does not exist for a second 2×2. That is not a reason to grow the room.

Nameplate "a small painting", same call the portrait uses (`y + 2*TILE + 8`, centered). It lands on empty floor at y=2, under the civic mat, not on the shelf and not on the portrait's plate.

Owner's house only: interactable `{ type: "studioPainting", x: 4, y: 0, prompt: "Draw" }`. The demo house does not get it. A visit draws the image and does not open the easel (toast "A small painting."). Same split the portrait already has. `STUDIO_PAINTING = { x: 4, y: 0, w: 2, h: 2 }`.

Draw it in the house/visit pass, after the civic frame, from its own bitmap. Do not reuse the portrait's wall buffer.

## Decision 3 — the second canvas on the daemon

**Recommended default: the same path shape, `kind` as the last segment. No stub. Same visit rule, called out here so it is not implied.**

Not a query string. Not a new prefix. The prefix route that already matches `/portrait` learns `/painting` the same way. Any other suffix stays 404. Do not let it fall through to region presence.

| Route | Who | What |
|---|---|---|
| `GET /presence/painting/portrait` | owner | Unchanged, including the stub. First touch still inserts the blank portrait and grants 2 `charcoal_black` once. Palette grows as in decision 1. |
| `POST /presence/painting/portrait` | owner | Unchanged call to `ensure_portrait` before the save. Cost and lock are decision 1. Still the portrait row only. |
| `GET /presence/painting/painting` | owner | Bound only, `403 not_bound`. Does **not** call `ensure_portrait`. Does **not** insert. Does **not** grant. No row yet: `"ok":true` and 1024 zeros, plus palette. |
| `POST /presence/painting/painting` | owner | Save. Does **not** grant. The first **accepted** save inserts the `'painting'` row, even if every pixel is 0 (that save is free). A rejected save inserts nothing. |
| `GET /presence/painting/<owner>/portrait` | visit | Unchanged. |
| `GET /presence/painting/<owner>/painting` | visit | Same function the portrait uses: `portrait_readable`. |

**Visibility, explicit.** Owner always. A friend under `friends` or `open`, including while the owner is offline. `locked` admits nobody else. An `open`-policy stranger never, even though `visit_allowed` would let them into the house. Civic returned no friends list: refuse. A GET is not a stay. Failing open would show the picture to whoever asks while civic is down. Not the public `/placed` read. Visit body is pixels, width, height. No palette. No row: `404 no_painting`, and the client draws paper without a toast. The visit GET does not insert.

One `last_paint_ms` per player, shared by both POSTs, still 5 seconds (open decision 12). A portrait save blocks a painting save and the reverse. A body that parses as 1024 indices counts, even if it is then `locked_index` or `insufficient`. A malformed body does not. Server clock only. A client timestamp is ignored.

Guests: `403 not_bound`. No guest fallback. No nginx change.

## Decision 4 — which canvas the easel is editing

**Recommended default: no switch inside the overlay. Where you opened it is the canvas.**

- Workshop easel (7,4) and the portrait frame (10,0) edit the portrait. Title stays "A small portrait". Save posts `/presence/painting/portrait`.
- The new frame (4,0) edits the painting. Set `#easelTitle` to "A small painting" while that canvas is open, and back when it closes. Save posts `/presence/painting/painting`.
- One overlay, the same `#easel`. Opening one loads only that bitmap. Do not copy the portrait into the painting. Saving updates only that wall bitmap.
- Tools stay pen, smudge, eraser, undo of 16. Smudge stays the 4×4 Bayer dither **among charcoal 1–4**. A selected index of 5 or more stamps solid, same as the pen. Do not mix two lakes.
- Swatches come from the own-GET `palette`, in index order. Button labels for 1–4 stay Black, Grey, Light, Pale. The rest use the item's display name (Shell white, Yellow ochre, Red ochre, Carbon ink, Iron-gall ink, Verdigris, Madder lake, Weld lake, Woad blue). A swatch with qty 0 is disabled. Keys 1–4 still select those charcoal indices when held. No new digit keys. Targets stay 44px. The phone layout already scrolls; do not invent a second overlay.
- An index the client table does not know draws as paper, not as black.
- Empty-swatch note is "No more of that." Do not say "charcoal" for a lake. The budget line names pixels of the selected colour, not "Charcoal left", once the open palette can hold more than charcoal.
- Guests never enter painting mode. The workshop easel stays the session charcoal portrait. Opening it still does not fetch. Guests have no own house, so they never reach (4,0) as owner.

A visit on (10,0) still toasts "A small portrait." and does not open the easel.

## Tests

Daemon, real binary, the painting script extended rather than a second harness:

- Portrait indices 1–4 still save with no discovery. Index 5 with no `shell_white` discovery is `locked_index` and charges nothing, including when it is one pixel inside an otherwise affordable bitmap.
- After that discovery, new shell-white pixels cost `ceil(n / 200)` `shell_white` and do not touch charcoal. Qty 0 is `insufficient`, not locked.
- Index 14 is `locked_index`. An existing portrait blob is still charcoal under 1–4.
- `GET` and `POST` `/presence/painting/painting` grant no `charcoal_black` and do not insert a portrait. Own GET with no row inserts nothing. The first accepted POST inserts one `'painting'` row. A locked POST inserts nothing.
- A painting save does not change the portrait blob, and the reverse.
- The second POST inside 5 seconds of a portrait save is 429.
- Visit: friend yes; stranger no; `open` stranger no; `open` friend yes; `locked` friend no; no friends list no. Friend body has pixels and no palette. `404` when no row, and that GET does not insert.
- Unbound is 403 on both new routes.

Client: the new interactable is (4,0) on the owner's house only; (4,1) and (5,1) are not solid; the overlay from the workshop still posts the portrait URL; the overlay from (4,0) posts the painting URL; a guest `openEasel` still does not fetch.

## When this is actually built

Write `studio-4-built.md`. Notes bullet under Shared pad. Commit. Do not deploy. Slices 1–4 still wait on a literal `deploy`.
