# Property phase 5: a shop in the Square — proposed

Brandon: a town, where you buy and sell. This is the phase that reverses "no currency." That reversal is
his, not a relay, and it is confined to this phase. `CLAUDE_IDEAS.md` still lists shops under Rejected.
Leave that bullet until a `BUILD` actually ships this. A proposal does not un-reject it.

Checked before proposing anything, by reading the code and by loading `world.js` and printing the Square:

- There is no coin, price, shop, or balance anywhere in `schema.sql`, `catalog.sql`, `studio.cpp`, or the
  client. The word does not exist yet.
- The Square (`REGIONS`, col 1, row 1, tiles x 24-47, y 20-39) is the spawn, a notice-post at (36, 33),
  two benches, and the path spine. It has no interactable of its own. It is not an empty lot.
- A path already runs north through that Square on x=34 and x=35, from the north edge (y=20) down to the
  spine (y=30). Printed, not assumed. (36, 22), one tile east of that path, is grass. Nothing sits on it.
- `place_floor` (`house.cpp`) does not read inventory and does not deduct it. A placed stool is a display
  row. That trust model is deliberate and it is the wrong model for money.
- The bench is the opposite. `StudioStore::start_batch` (`studio.cpp`) deducts inputs and inserts the
  batch in one transaction, and it refuses if the player does not hold the inputs. Gather for a logged-in
  player is the same kind of check (`gather_cooldowns`, a grant of 3, a cooldown). Guests never call
  those routes.
- `players` already grew two columns the same way: `ensure_visit_policy()` and `ensure_shirt_pigment()`
  (`db.cpp`), each one a `pragma_table_info` check and an `ALTER` at startup. No hand migration.
- The Workshop and the mine are interiors with a door and an instance key, and neither has a `maps` row.
  `seed.sql` and `catalog.sql` were both searched. Presence does not need one (`presence.hpp` treats the
  region key as an opaque string). Phase 4 added a `maps` row for the Dunes because every outdoor region
  has one. A room does not have to.
- Studio pacing, as it stands in `catalog.sql` and `studio.cpp`: the twelve gather spots other than the
  keg wait 600000 ms and grant 3. The keg (`orchard_keg`) waits 43200000 ms (12h) and grants vinegar.
  Bench recipes run from 1800 s (`carbon_ink`, `fence_post`) to 604800 s (verdigris, 7 days). The bench
  holds 5 batches. `CLAUDE_STUDIO_5.md` shortened the keg because vinegar feeds two of those long chains.
  Buying vinegar, or buying a finished pigment, would undo that.

## Decision 1 — the town is the Square. The new thing is a door.

Not a new region. Phase 4 grew the grid because a desert needs open ground. A shop is a room, and the
game already has rooms: `buildInterior`, a solid door tile, `enterInterior`, one shared instance key.
The Workshop is that shape. The Shop is the same shape.

Not a stall standing in the Square. The notice-post was moved off the spawn because a solid tile there
traps a new arrival (`dressRegions`, the comment above the notice-post). The Square's job is arrival
and the path through it. A counter in the middle would be a second version of that bug.

The door is `T_DOOR_DEMO` at (36, 22), the grass tile beside the path that is already there. You stand
on (35, 22), which is already `T_PATH`, and Enter. No new path, no new tile id, no grid change.
`GRID_ROWS` stays 5. Columns 1 and 2 of row 4 stay grass.

Inside: one small shared room, instance key `instance:shop`, same as `instance:workshop`. One counter,
the existing `T_TABLE`. Enter on the counter is the shop. No shopkeeper sprite, no dialogue. The only
walker in this game is the passerby, and it has no lines. A person behind the counter would be a new
actor and new art. The counter is the seller.

## Decision 2 — the currency is a number on the player, and the server is the only writer

`players.coins INTEGER NOT NULL DEFAULT 0`, added by `ensure_coins()` in the same shape as
`ensure_shirt_pigment()`. Not an item. If coins were a catalog slug, `place_floor` would let you stick
your money to the grass, and it would not deduct it. A column cannot be placed.

This is a security boundary. `place_floor` is not the model to copy. A client that is trusted to name
its own balance can mint. Buy and sell are one server transaction each, the same shape as the bench
deduction: the price is looked up from a fixed server table, the slug the client sends is only a key,
and a price the client sends is ignored. Coins are not put on the presence broadcast. The shirt colour
is public. A balance is not.

A guest can walk in and read the counter. Buying or selling toasts "Log in on LiquidGov to trade." and
calls nothing. Same split as the bench's own guest note. No session purse. A purse that survived a
refresh would be the guest persistence this game already refuses.

## Decision 3 — an NPC counter, fixed prices. Not player to player.

One counter. Two sides, one route, `POST /presence/shop` with `{ "side": "buy"|"sell", "slug": "..." }`.
Each call moves one item. The table is fixed in the daemon the way `gather_spots()` is fixed. Adding a
third thing for sale later is a row in that table, not a new route.

Player-to-player trade is not this phase. It needs an offer, a second player, and something to hold the
goods in between. Nothing in `placed` or `inventory` is that. It is a different feature. It is not
bundled in here, and a `BUILD` of this doc does not start it.

## Decision 4 — what is for sale, and what coins are not allowed to buy

Coins buy nothing a recipe consumes, and nothing a recipe produces. That is the whole rule that keeps
`CLAUDE_STUDIO_5.md`'s waits intact. No vinegar, no copper, no shells, no ochre, no twigs, no galls, no
alum, no madder, no weld, no woad, no soot, no gum, and none of the inks, lakes, or intermediates. The
hammer and the fence post stay crafting. You cannot buy your way past a cooldown.

What the counter sells, this pass, is two new placeables that have no gather spot and no recipe:

| slug | name | price |
|---|---|---|
| `stool` | Stool | 9 |
| `lantern` | Lantern | 9 |

Both `category` `placeable`, `wall_slot_ok` 0, not in `STRUCTURAL_ITEMS` (they do not block movement and
they do not ask for a hammer). Icons come from the existing procedural glyph buckets (`ITEM_CATEGORY`).
No new art file, no new tile id.

Nine is three gathers. A gather grants 3, and the spots below wait 10 minutes, so 9 coins is three
trips and about 30 minutes of cooldown, in the same band as the fence post (1800 s) and nowhere near
verdigris (7 days). It is a number, not a promise. Retune it later the way the floor cap was retuned.

One purchase is a permanent unlock to place copies, because `place_floor` still does not consume
inventory. That is the rule the fence post already lives under. This phase does not invent a second
rule where each stool on the grass costs another 9. The floor cap (24) is still the limit.

## Decision 5 — you get coins by selling surplus raw gathers, not by selling the craft

Sell list, 1 coin each, one per call, server deducts the inventory row the same way the bench deducts
an input:

`oak_gall`, `willow_twigs`, `seashell`, `ochre_earth`, `alum_stone`, `madder_root`, `weld`,
`woad_leaves`, `scrap_iron`, `copper_sheet`, `gum_arabic_raw`, `soot`.

Those twelve are the gather spots whose cooldown is 600000 ms. Vinegar is a gather too, and it is not
on this list. Its cooldown is 12 hours, and it is the input the long chains share. Paying a high price
for it would turn the keg into a coin printer. Paying the same 1 coin as a shell would be a lie about
what it cost. Leaving it off the list means the keg stays a crafting ingredient.

Nothing crafted is on the list. Selling a finished lake for coins you could only spend on a stool does
not skip a wait, but it also does not need to be in the first version. The sell list is the raw
10-minute gathers and nothing else. If you sell a shell, you no longer have that shell. That is a
choice between a pigment and a stool, not a way to have both sooner.

## Out of scope, said plainly

No new region, no new tile, no new path, no shopkeeper. No haggling. No player-to-player trade. No
guest purse. No coins on the presence broadcast. No buying or selling of anything a recipe consumes or
produces, including vinegar. No change to gather cooldowns, bench durations, or the 5-slot cap. No
change to `place_floor`'s refusal to deduct. The Dunes, the property plot, and the hammer gate stay as
they are.

## Tests

- `test_studio.sh` (or a sibling next to it): a fresh bound account sells one `seashell` it was granted
  and holds 1 coin; selling with none of that slug returns a refusal and does not change the balance; a
  second account cannot buy a `stool` with 0 coins; with 9 coins the buy deducts 9 and grants one
  `stool`; a buy of `vinegar` or of `shell_white` is refused (the client does not get to name a price
  or a slug outside the table); a guest call is `not_bound`.
- `client_files_unit.js`: the Square has a door interactable at (36, 22) whose stand tile (35, 22) is
  `T_PATH`; `GRID_ROWS` is still 5; the shop interior exists and its counter is the only shop
  interactable; a guest at that counter is told to log in and does not call fetch.

## Deploy

Daemon plus client. One new column, added at startup by `ensure_coins()`, not by a hand migration. One
new route under `/presence/`, so the existing `/commons/presence/` proxy already covers it. Two new
`catalog.sql` item rows, no recipe rows. No `maps` row, matching the Workshop and the mine. No nginx
change. Holds for a literal `deploy`, same as always.

---

**Say `BUILD property phase 5` to build this as written, or tell me what to change first.**
