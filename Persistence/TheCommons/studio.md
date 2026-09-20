# The Studio — spec (draft for Brandon's sign-off, 2026-09-20)

**Status: SPEC ONLY. Nothing here is built.** Each slice at the bottom is its own `BUILD`, and every
droplet touch needs the literal `deploy` word (daemon deploys: drift-check, online DB backup first).
Written from Brandon's answers on 2026-09-20 (see "Decisions made" and "Open decisions").

## One paragraph

People need something to **do**. The Commons becomes a place where you **gather real materials from the
regions, make real pigments at a bench (roughly real recipes, roughly real waiting times), and paint
with only the colours you have made**, starting with a stub of charcoal. Materials are used up as you
paint; every source renews, so nothing runs out for good, but you keep doing the loop. Your first
painting is a **self-portrait** that hangs in your house and grows from black and white into colour
as you unlock pigments. This is the reason to come back: a batch is ready, a colour is unlocked, a
portrait can be redrawn, and friends see it when they visit. It is a game-side system: **civic is not
touched** (its drawing tool already has unlimited colours and image import, so it cannot be gated and
is not).

## Decisions made (Brandon, 2026-09-20)

1. The point of the game is to **do** things; the profile-page/house idea stays, but needs a purpose.
2. **Realistic chemistry** experiments: real ingredients, roughly real time to make a batch. No quantities,
   no stoichiometry.
3. **Pigments you make are your palette.** Regions become material sources (yes).
4. The **self-portrait is in the game** (not the civic `/me/portrait` tool), starts with charcoal
   (black/white/greys) and unlocks colours (yes).
5. **Materials are consumed with use; sources renew** ("nothing is finite" read as: you never run out
   permanently) (yes). A **free starter stub of charcoal** so the first minute is drawing (yes).

## Locks this reopens (recorded, not silently overridden)

| Earlier rule | Where | Now |
|---|---|---|
| "Crafting is banned regardless of round"; "no crafting loop" | `GROK_V2_ANSWER.md`, `v1.md`, `GROK_BET.md` | Reopened: a real loop, at the Workshop |
| "Real names, not real stoichiometry/safety" | `pushback.md`, `notes.md` | Kept in spirit: real ingredients and times, still no quantities/stoichiometry/hazards |
| Counts never go down; lookups never consumed | `inventory-strip-built.md`, `bound-inventory-built.md` | Reopened: materials are consumed |
| "No RPG menu / inventory screen" | `art.md` | Reopened narrowly: a bench screen, an easel screen, a notebook |
| "Do not invent guest persistence" | `v1.md`, `inventory.md` | **Kept.** Guests stay RAM-only (see guests) |
| No XP / quests / combat / day-night | `v1.md` | Kept. Progress = unlocked colours + what you have made |
| Civic frozen except the named `/internal/*` routes | notes | Kept. **No civic change anywhere in this spec** |

## Guardrails

- **Safety line.** Benign historical/domestic materials only (ink, pigments, dyes, mordants, vinegar,
  chalk, charcoal). No hazardous chemistry, no toxic-heavy-metal pigments (no lead white, no vermilion),
  **no procedure text**: a recipe shows ingredients, a rough time and a one-line history, never steps or
  temperatures. Verdigris carries a one-line "toxic; not for eating" history note.
- **Accuracy.** Every recipe, time range and history line must be **reviewed against references (or by
  someone with the knowledge) before the recipe table is built**. The list below is drafted from general
  knowledge; treat every entry as unverified.
- No free text anywhere new (recipe names, notes and phrases are all ours). No moderation surface added.
- Server-authoritative for anything durable. The client is never trusted with inventory, timers or
  unlocks for a bound player.
- Guests: RAM only, exactly as today.

## The loop

    Regions (gather, cooldown)  ->  raw materials in your inventory
        Lab (stock room: lookups of prepared raws: gum, alum, copper, iron, soot)  ->  more raws
              |
              v
    Workshop BENCH (combine ingredients -> starts a timed batch; up to 3 at once)
              |  (come back later; batches finish on the server clock, even while you are away)
              v
    Collect  ->  pigment / ink / material in inventory  +  DISCOVERY recorded (the notebook, and the unlock)
              |
              v
    Workshop EASEL (paint with the colours you have unlocked, spending material as you paint)
              |
              v
    Your house: the SELF-PORTRAIT frame (+ one painting frame), seen by whoever the door policy lets in

## Where things live

- **Workshop (already built, 14x10):** gets the **Bench** (interactable on an existing workbench tile) and the
  **Easel** (interactable on an existing tile). No new art needed; the screens are DOM overlays.
- **Lab:** stays. Its eight stations stop handing out finished products and become the **stock room**
  (raw, prepared materials). Products already in bound inventories stay valid (legacy items, still placeable).
- **Regions become sources** (new lookup spots, same interactable as `worldPickup`, plus a cooldown):

| Region | Gives |
|---|---|
| North Woods | oak galls; willow twigs (charcoal wood) |
| Orchard | cider apples |
| Shore | seashells |
| Quarry Ridge | yellow ochre earth; alum stone |
| Garden | madder root, weld, woad leaves |
| Workshop Lane | scrap: iron nails, copper sheet (from the crates) |
| Lab | gum arabic, prepared soot (lamp), water is implicit everywhere |

The other regions (Square, Homestead, Hall Lawn, Lookout, Creek, Lab Yard) keep their current role; they
are places to meet and walk, not sources.

## Materials, use and renewal

- Inventory counts are abstract **units** (not grams). Existing cap 99 per item stays.
- **Gathering:** each spot gives a small pile per visit (default **3 units**) then a per-player **cooldown**
  (default **10 minutes** per spot, server-side for bound players, RAM for guests). Sources never run out.
  Numbers are placeholders to tune in play (slice 5).
- **Painting consumes:** painting is charged **per pixel by colour on Save**: **1 unit of a pigment ≈ 200
  pixels** (charcoal stub ≈ 400). The server computes the cost from the submitted bitmap vs the stored one.
- **Failed experiments cost nothing** (default): combining something that does nothing consumes no
  materials and tells you what really happens. Only a started batch consumes its inputs.
- **Unlocks are permanent.** Discovering a recipe once (collecting its first batch) unlocks that pigment
  as a palette colour forever; you still need *material* to paint with it.

## Recipes (v1 draft; UNVERIFIED, needs review before build)

Times are **typical real ranges**, the game time in the last column follows the rule in Open decision 1
(real time up to ~3 days; longer processes capped at 7 days and labelled honestly). All output "units" are
small fixed numbers set in the catalog (default 3).

| Recipe | Ingredients | Real range | Game time | Yields | Tier |
|---|---|---|---|---|---|
| Charcoal | willow twigs | ~2-4 h | 3 h | charcoal (black, greys) | 1 |
| Whiting | seashells | ~1-3 d (grind, wash, settle, dry) | 1 d | white pigment | 2 |
| Yellow ochre | ochre earth | ~1 d (wash, settle) | 1 d | yellow ochre | 3 |
| Red ochre | yellow ochre (heated) | hours | 4 h | red ochre | 3 |
| Gum solution | gum arabic + water | overnight | 8 h | gum solution (binder) | 3 |
| Oak-gall tannin | oak galls + water | ~1-2 d | 1 d | tannin | 3 |
| Vinegar | cider apples | ~4-10 wk total (cider, then acetic) | 7 d cap | vinegar | 3 |
| Iron acetate | iron + vinegar | days to ~2 wk | 3 d | iron acetate | 4 |
| Iron-gall ink | tannin + iron acetate + gum | mixes at once; darkens over ~a day | 12 h | blue-black ink | 4 |
| Carbon ink | soot + gum solution | under an hour | 30 min | black ink | 3 |
| Verdigris | copper + vinegar (closed pot) | ~1-3 wk | 7 d cap | green pigment | 5 |
| Alum crystals | alum + water | ~1-3 d | 1 d | alum (mordant) | 4 |
| Madder lake | madder + alum + whiting | days | 2 d | red pigment | 6 |
| Weld lake | weld + alum + whiting | ~1-2 d | 1 d | yellow pigment | 6 |
| Woad blue | woad leaves + ash + water | days | 3 d | blue pigment | 6 |

Note: **ash** is a by-product of Charcoal (2 units), which gives a real reason to make charcoal again.

**Wrong-combination results (must be real, checked):** chalk/whiting + vinegar fizzes and dissolves
(calcium acetate); iron + vinegar without tannin gives a rust-brown solution, not ink; tannin alone gives
a weak brown stain; anything else: "Nothing useful comes of it." Add more only when verified.

**Palette ladder (what the easel offers):** Tier 1 charcoal = black + three greys; Tier 2 whiting = white;
Tier 3 ochres/inks = yellow, red-brown, brown, black ink; Tier 4 iron-gall = blue-black (ages to brown);
Tier 5 verdigris = green; Tier 6 lakes/woad = red, yellow, blue. Each rung takes longer than the last.

**The notebook:** one line per discovered recipe (name, real time range, one-line history). Undiscovered
entries show as "?". No free text.

## The self-portrait and the easel

- **Canvas:** **32x32** pixels, palette-indexed (1 byte/pixel = 1 KB). Tools: pen (1 px), soft smudge (charcoal
  greys by dithering), eraser, small undo stack. No layers, no import, no text, no colour picker: the palette
  is exactly your unlocked pigments with material left.
- **A new player** gets a **starter charcoal stub** (≈400 pixels, once, free) so the first minute is drawing.
  The portrait exists from then on and can be redrawn; recolouring later costs material.
- **Two frames in your house:** the **portrait frame** and one **painting frame**, at fixed spots (no
  placement UI). The existing civic wall slot is unchanged.
- **Who sees it:** the owner, plus whoever the house policy admits. **Stricter than `/placed`** (which is
  public read): paintings are served only after the same visit check the daemon already uses, and **never
  to `open`-policy strangers** until there is a way to report or hide (Open decision 4).
- **Server-authoritative save:** the client submits the whole bitmap; the server (a) rejects any pixel using
  a colour the player has not unlocked, (b) computes the material cost vs the stored version, (c) deducts and
  stores in **one transaction**, (d) rejects on insufficient material. Size and rate capped.

## Guests

Guests can **gather, keep a starter charcoal stub, and draw a portrait in charcoal for the session**.
Nothing persists: no batches, no unlocks, no saved painting (RAM, as today). **Making and keeping
needs an account.** (This corrects my earlier "hours-long recipes for guests": a batch that outlives the
tab would be guest persistence, which stays ruled out.) The bench shows guests what they would make, with
"Sign in on LiquidGov to start a batch."

## Data model (all additive; nothing dropped)

Existing DBs get changes from `ensure_*` functions at startup (as `ensure_inventory_recency`/`ensure_visit_policy`
do); `schema.sql` is updated for new DBs; the catalog is `INSERT OR IGNORE` from `catalog.sql`.

- `items`: new rows (raws, intermediates, pigments); category stays `'material'` (the CHECK cannot be altered in
  SQLite), pigments identified by `sprite_ref`/a `pigment_hex` column (ADD COLUMN, nullable).
- `recipes`: ADD `duration_s`, `tier`, `real_range_note` (short text, ours), `history_note` (short text, ours).
  `recipe_inputs` / `recipe_outputs` already exist and are used as designed.
- New: `batches(batch_id, player_id, recipe_id, started_ms, ready_ms, collected)`; `discoveries(player_id,
  recipe_id, first_ms)` (the notebook and the unlock); `gather_cooldowns(player_id, spot, next_ms)`;
  `paintings(painting_id, player_id, kind CHECK IN ('portrait','painting'), w, h, pixels BLOB, updated_ms)`,
  unique `(player_id, kind)`.
- Server clock is the only clock. A test seam (env-overridable clock) exists for tests only.

## Daemon API (all under `/presence/`, cookie identity only, no guest fallback, so **no nginx change**)

`POST /presence/gather {spot}`, `GET /presence/studio` (inventory, batches with `ready_ms`, discoveries,
cooldowns, unlocked palette), `POST /presence/combine {ingredients:[slug,...]}` (an experiment: starts a batch
or returns the real "no reaction" text), `POST /presence/collect {batch_id}` (idempotent),
`GET/POST /presence/painting/<kind>` (own) and `GET /presence/painting/<owner>/<kind>` (visit-checked).
Batch slots: max **3** concurrent. Bound only; unbound is `403 not_bound`. The existing dormant client poll of
`/commons/recipes` (404) is left alone until slice 3 decides its fate.

## Client

Bench, easel and notebook are DOM overlays (keyboard and touch, portrait-phone friendly). A HUD line shows
"batch ready" on arrival. The inventory strip now shows counts that go down. Region spots are new
`worldPickup`-style interactables with the cooldown shown as "Nothing more to gather here yet."

## Tests (each slice)

Recipe-table validation (every input/output exists, durations in bounds, **no cycle that gains material**);
server-authoritative gather/combine/collect (no materials, not ready, double-collect, slot cap, unbound 403);
painting save (locked colour rejected, cost charged atomically, insufficient material rejected, size caps);
migration on an **old-shape DB** (`test_migrations`); visibility (friends yes, stranger no, `open` no); the
world flood-fill test extended for every new spot; a real-browser run per slice. CI as usual.

## Slices (each its own BUILD; deploys need the `deploy` word)

1. **Daemon core:** tables, catalog, recipes with durations, gather/cooldown, combine/collect, discoveries,
   consumption, tests, migration. Daemon deploy (DB backup first).
2. **World sources + lab conversion (client):** region spots, lab stations become raws, legacy items kept.
3. **Bench + notebook (client):** the Workshop bench screen, batch-ready HUD, notebook.
4. **Easel + portrait (daemon + client):** paintings table/endpoints, easel screen, two house frames, visit-checked read.
5. **Tuning pass:** cooldowns, yields, pixel cost, with real play on the live site.

## Open decisions (recommended defaults marked)

1. **Time scale.** Real time up to ~3 days; processes longer than that capped at **7 days** and labelled
   ("typically 4-10 weeks; here 7 days"). *(default; needs your yes)*
2. **Guests** as above: gather + charcoal drawing in-session; making/keeping needs an account. *(default)*
3. **Failed experiments consume nothing.** *(default)*
4. **Painting visibility:** friends and owner only, never `open`-policy strangers, until report/hide exists. *(default)*
5. **Canvas size:** 32x32 (48x48 is possible later). *(default)*
6. **Recipe review:** who checks the chemistry and history (you, Grok, a reference pass by me)? *(needs an owner)*
7. **Legacy items** (the eleven old items) stay placeable trophies; new stations give raws. *(default)*
8. **Moderation:** none needed while there is no free text and paintings are friends-only; revisit before any
   "open" visibility. *(default)*
