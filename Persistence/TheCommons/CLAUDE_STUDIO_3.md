# BUILD studio 3 — map sources, bench, notebook, lab stock

Brandon: "lets do map sources and bench" (2026-09-27). This is slice 3 of
`studio.md`. **Build and test locally. Do not deploy.** Slices 1 and 2 are
already in the tree and also not deployed. One later `deploy` covers all
three. No civic edit. No nginx edit. No new schema table.

The gather spots, cooldowns, bench, mix, and collect routes already exist
(`commons-daemon/src/studio.cpp`). This slice is the client, plus one small
addition to `GET /presence/studio` so the notebook can print a line without
a second recipe dump.

## Do not

- Add a second marker on the oak-gall tree, the shell spot, or the orchard tile.
- Grant `cider_apple` from that tile anymore. The keg replaces it.
- Turn copper, iron, or chalk into stock. Those three Looks stay trophy pickups.
- Poll `/presence/studio` on the 2 Hz presence tick.
- Let a guest call gather, bench, mix, collect, or studio.
- Invent a new tile, a PNG, or a crafting minigame. Reuse flowerbed / crate / existing stations.
- Show recipe names for things the player has not collected. Undiscovered lines are `?`.

## Frozen lab table

Interior tiles already in `interiors.js`. Prompt stays Look.

| Tile | Was | Now |
|---|---|---|
| copper (5,7), iron (10,7), chalk (14,7) | trophy lookup | **unchanged** |
| vinegar (3,10) | +1 `vinegar_jar` | no grant. Toast once this session: "Stock. Vinegar is the keg in the Orchard." |
| tannin (6,10) | +1 `oak_gall_tannin` | no grant. Toast once: "Stock. Make tannin at the Workshop." |
| lampblack (9,10) | +1 `lampblack_ink` | gather slug `lab_soot` |
| alum (12,10) | +1 `alum_crystal` | no grant. Toast once: "Stock. Alum stone is on Quarry Ridge." |
| gum (15,10) | +1 `gum_arabic` | gather slug `lab_gum` |

People who already hold the old jars keep them. They still place.

## Frozen map spots

Same tile, new behavior: the interactable becomes a studio gather, not the old
`/presence/pickup` grant. Bound players POST `{spot}`. Guests add 3 to session
RAM and start a local cooldown. They never fetch.

| Spot slug | Tile | Visual | Was |
|---|---|---|---|
| `north_woods_galls` | (17,6) | the existing oak | oak-gall pickup |
| `shore_shells` | (62,73) | the existing shells | seashell pickup |
| `orchard_keg` | (56,10) | the same flowerbed between the rows | cider apples. Prompt still Look. Name in the toast: "The keg." |
| `workshop_iron` | (16,28) | the existing crate | decorative |
| `workshop_copper` | (17,30) | the other crate | decorative |

New markers, placed **after** `scatterRegions()` so scatter cannot paint over them.
If that tile is not grass or dirt, move one tile onto grass or dirt and say so
in the writeup. Do not sit on water, a tree, a wall, or the path.

| Spot slug | Tile | Region |
|---|---|---|
| `north_woods_twigs` | (14,8) | North Woods |
| `garden_madder` | (52,44) | Garden |
| `garden_weld` | (66,44) | Garden |
| `garden_woad` | (52,54) | Garden |
| `quarry_ochre` | (4,74) | Quarry Ridge |
| `quarry_alum` | (18,74) | Quarry Ridge |

New outdoor piles use `T_FLOWERBED` the way the apple pile already does.
Quarry stays on dirt; do not lay flowerbed on the quarry. The Look is enough.

Cooldowns and quantities stay the daemon's: 3 units, 10 minutes, keg 1 day.
On `on_cooldown`, toast "Not yet." Grant nothing.

## Workshop room

Keep the easel at (7,4). Add:

- Bench interactable at (3,2), prompt **Make**. Opens the bench overlay.
- Notebook interactable at (10,2), prompt **Read**. Opens the notebook overlay.
- Sign text becomes "The bench is on the left. The book is on the right."

Both overlays are full-screen DOM, same idea as `#easel` (usable at 160×240).
The bench screen shows this line and nothing that reads like instructions:
"Names and waiting. Not a recipe to follow at home."

Bench contents, bound:

- Open batches (up to 3) with a countdown from the server `ready_ms`. A ready one has Collect.
- Recipes in `makeable` only. Start sends `POST /presence/bench {recipe}`.
- If the server says the bench is full, toast "The bench is full."
- One row for a side mix: up to 4 held items, button Try, `POST /presence/mix`. The reply is a toast. It never starts a batch and never writes a discovery.

Guest on the bench: the same lists from RAM, and the only action is
"Sign in on LiquidGov to start a batch." No POST.

Notebook, bound: one line per discovery (name, the real-range note, the history
line). Then one `?` for each bench recipe they have not collected. No free text.

Guest notebook: "Sign in on LiquidGov to keep a notebook."

## The one daemon change

`GET /presence/studio` discoveries are slugs only today. Extend each discovery
object with `display_name`, `real_range_note`, `history_note` from that recipe
row. Add `recipe_count`: how many bench recipes have a duration. That is the
notebook's length. Do not add a route that lists undiscovered names.

## When to fetch

- Once after a bound join.
- Once on entering the workshop.
- While the bench overlay is open, at most every 30 seconds.
- Never from the presence tick.

If any open batch has `ready_ms` in the past, show one line off the canvas,
same family as the reconnect line: "A batch is ready." Hide it when none are.
Collect still only works from the bench.

## Inventory

The strip already shows counts. New slugs need a color and a short label in the
existing item table so the glyph bake does not throw. Use the generic square.
No new drawings. Galls and shells keep the glyphs they have.

After a successful gather, bench, or collect, trust the server inventory on the
next studio GET (or the grant in the gather/collect body). Do not also POST
`/presence/pickup` for these spots.

## Tests

- Guest Look at the keg and at a gall: inventory moves locally, zero gather/studio requests.
- Bound Look at the keg: one POST `orchard_keg`, not `cider_apple`.
- Vinegar, tannin, and alum stations grant nothing and toast once.
- Gum and soot stations POST `lab_gum` and `lab_soot`.
- Copper, iron, chalk still grant the old trophies.
- No second interactable on (17,6), (62,73), or (56,10).
- Opening the bench does not start a 2 Hz studio poll.
- Flood-fill / scatter still passes. A new marker must not seal a walk.

Write `studio-3-built.md`. Notes bullet under Shared pad. Commit. Do not deploy.
