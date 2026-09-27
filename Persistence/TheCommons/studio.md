# The Studio — spec v2 (Grok's review folded in; slices 1 and 2 BUILT 2026-09-27)

**Status: slice 2 built and tested; not deployed (no droplet touch).** Slice 1 is daemon-only
(`studio-1-built.md`). Slice 2 is the charcoal easel, the portrait endpoints, and the house frame
(`studio-2-built.md`). Where this file did not already pick a number, the reading taken is an open
decision below (11–14).
v1 was `9a5b7c5`; Grok's review is `GROK_STUDIO_ANSWER.md` (`5c45287`); this file is v2, written 2026-09-20.
Each slice below is its own `BUILD`; every droplet touch needs the literal `deploy` word (daemon deploys:
drift-check, online DB backup first). The reference pass is done (`studio-refs.md`, 2026-09-20); slice 1's frozen
table applies corrections C1-C6 in full (`studio-1-built.md` has the exact wording).

## One paragraph

People need something to **do**. The Commons becomes a place where you **draw with charcoal from the first
minute, and gather real materials from the regions and make real pigments at a bench (real ingredients,
roughly real waiting times) to earn a wider palette**. Materials are used up as you paint; every source
renews, so nothing runs out for good, but you keep doing the loop. Your first painting is a **studio
portrait** that hangs in your house and grows from black and white into colour. The easel is what people
*do*; the bench is why they *return* (a batch finishes on the server clock, so there is a reason to come
back even when the Square is empty). Friends see the portrait when they visit. It is a game-side system:
**civic is not touched** (its drawing tool already has unlimited colours and image import, so it cannot
be gated and is not).

## What changed after Grok's review

| # | Change | Source |
|---|---|---|
| 1 | **Discovery is "you hold every input", not free-form guessing.** The bench lists a recipe when you hold all its inputs; the first successful collect is the discovery. Free-form mixing survives only as a rate-limited side action with a fixed list of real fail texts. | Grok §1, §5 |
| 2 | **Homemade vinegar is cut** (real cider vinegar is weeks to months; 7 days was not "roughly real"). The Orchard's existing apple spot becomes **a keg** that yields vinegar with a one-day per-player cooldown. | Grok §2 |
| 3 | **"Whiting" renamed "shell white"** (whiting is chalk; shell white is the shell pigment). **Alum crystals dropped** (purifying alum you gathered is busywork; Quarry **alum stone** is the mordant). **Woad rewritten** as a simplified lake so the ladder has a true blue. | Grok §2 |
| 4 | **Slice order changed:** the charcoal easel and portrait ship *before* the long ladder; the lab is not converted until the bench can spend the raws. | Grok §1, §7 |
| 5 | **Painting never refunds material** (rule made exact below). | Grok §5 |
| 6 | Server rules tightened (transactions, uniqueness, no client timestamps, palette-index bitmaps only, polling, orphan reap, guest path). | Grok §5 |
| 7 | Phone layout is a slice-2 requirement, the bench carries a "not a recipe to follow at home" line, and the two soft locks Grok found are recorded. | Grok §3, §7, §8 |

**Three places I went a step beyond or differently from Grok** (flagged for Brandon, easy to overrule):

- **Painting cost formula.** Grok's `max(0, new_count[c] - stored_count[c])` lets you erase and repaint the
  *same number* of pixels for free, i.e. redraw the whole portrait from one stub. I charge **every pixel that
  becomes colour `c` and was not `c` before** (see Easel). Still no refunds. This is what makes "materials
  are used up" true.
- **The portrait is visit-checked from slice 2**, not slice 4, because shipping a public read even briefly is
  a privacy hole, and owner-only would delay the social payoff. The visit-check helper already exists.
- **Charcoal is 8 h**, taking Grok's "or bump to 8 h", not "label 3 h". The starter stub covers the first
  portrait, so the wait does not block the first session. **The real-world figure (Grok says ~10 h in a
  closed tin; I had 2-4 h) is disputed and must be reference-checked;** the game number stays a tunable.

## Decisions made (Brandon, 2026-09-20)

1. The point of the game is to **do** things; the profile-page/house idea stays, but needs a purpose.
2. **Realistic chemistry**: real ingredients, roughly real time to make a batch. No quantities, no stoichiometry.
3. **Pigments you make are your palette.** Regions become material sources.
4. The **portrait is in the game** (not the civic `/me/portrait` tool), starts in charcoal, unlocks colours.
5. **Materials are consumed with use; sources renew** ("nothing is finite" = you never run out for good).
   A **free starter stub of charcoal** so the first minute is drawing.

## Locks this reopens, and the two Grok found (recorded, not silently overridden)

| Earlier rule | Where | Now |
|---|---|---|
| "Crafting is banned regardless of round"; "no crafting loop" | `GROK_V2_ANSWER.md`, `v1.md`, `GROK_BET.md` | Reopened: a real loop, at the Workshop |
| "Real names, not real stoichiometry/safety" | `pushback.md`, `notes.md` | Kept in spirit: real ingredients and times, still no quantities/stoichiometry/hazards |
| Counts never go down; lookups never consumed | `inventory-strip-built.md`, `bound-inventory-built.md` | Reopened: materials are consumed |
| "No RPG menu / inventory screen" | `art.md` | Reopened narrowly: a bench screen, an easel screen, a notebook |
| "Do not invent guest persistence" | `v1.md`, `inventory.md` | **Kept.** Guests stay RAM-only |
| No XP / quests / combat / day-night | `v1.md` | Kept. Progress = unlocked colours + what you have made |
| Civic frozen except the named `/internal/*` routes | notes | Kept. **No civic change anywhere in this spec** |
| **Lab A already shipped** vinegar, oak-gall tannin, lampblack, alum and gum as **finished** lookups (`catalog-built.md`) | live | **A live behaviour change, not a green field.** Legacy items stay placeable trophies; converting a station needs a one-line toast the first time you Look ("this is stock now; make it at the Workshop") |
| **World pickups D already shipped** oak galls, seashells and cider apples (`world-pickups-built.md`) | live | Studio gather **reuses those exact spots**; no second marker on the same tile. Apples become the Orchard keg (vinegar), not a second apple pile |
| The mine has no ore | `GROK_MINE_ANSWER.md` | Kept. Scrap iron/copper comes from the Workshop Lane crates, never the adit |

## Guardrails

- **Safety line.** Benign historical/domestic materials only (ink, pigments, dyes, mordants, vinegar, chalk,
  charcoal). No hazardous chemistry; no lead white, vermilion, orpiment, malachite grinding, quicklime or
  Prussian blue; **no procedure text**: a recipe shows ingredients, a rough time and a one-line history, never
  steps, temperatures, pH, lye, urine or "closed pot". "Heated" (red ochre) is history, not a how-to.
  Verdigris carries "toxic; not for eating".
- **The bench screen carries one line:** "Names and waiting. Not a recipe to follow at home."
- **Accuracy.** Every recipe, time range and history line is reviewed before the table is built (see Recipes).
- No free text anywhere new. No moderation surface added.
- Server-authoritative for anything durable. The client is never trusted with inventory, timers or unlocks
  for a bound player. Guests: RAM only, and never call these endpoints.

## The loop

    Easel (from minute one): draw with the starter charcoal stub  ->  studio portrait hangs in your house
                                  |
    Regions (gather, per-player cooldown; existing spots reused)  ->  raw materials
        Lab (stock room, later)  ->  prepared raws (gum arabic, soot)
                                  |
                                  v
    Workshop BENCH: a recipe lights up when you hold every input; start a timed batch (3 slots)
        (the batch finishes on the server clock, even while you are away)
                                  |
                                  v
    Collect  ->  pigment / ink / material  +  DISCOVERY (notebook line + palette unlock)
                                  |
                                  v
    Easel: a wider palette, costing material as you paint  ->  redraw or add a second painting

## Where things live

- **Workshop (built, 14x10):** gets the **Bench** and the **Easel** (interactables on existing tiles; screens
  are DOM overlays, no new art).
- **Lab:** unchanged until the bench can spend the raws (slice 3). Then its stations become stock-room raws;
  the exact station-by-station table is frozen with the slice-3 BUILD, not here.
- **Regions as sources** (reusing today's spots where one exists; each gets a per-player cooldown):

| Region | Gives | Note |
|---|---|---|
| North Woods | oak galls (existing spot); willow twigs (new) | |
| Orchard | **vinegar keg** (the existing apple spot, converted); 1-day cooldown | "the keg behind the trees" |
| Shore | seashells (existing spot) | |
| Quarry Ridge | ochre earth; **alum stone** | new spots; not the adit |
| Garden | madder root, weld, woad leaves | new spots |
| Workshop Lane | scrap iron (nails), copper sheet | the crates |
| Lab | gum arabic, soot | stock room, slice 3 |

  Water is implicit everywhere. The Square, Homestead, Hall Lawn, Lookout, Creek and Lab Yard stay places to
  meet and walk.

## Materials, use and renewal

- Inventory counts are abstract **units** (not grams). Existing cap 99 per item stays.
- **Gathering:** a small pile per visit (default **3 units**), then a **per-player** cooldown per spot (default
  **10 minutes**; the keg 1 day), server-side for bound players. No global pile, so three people cannot starve
  a region. Sources never run out. Numbers are tunable (slice 5).
- **Painting consumes** (exact rule below). **1 unit ≈ 200 pixels** (the charcoal stub ≈ 400).
- **Failed side-mix experiments cost nothing** (they only ever return a fixed fail text). Only a started batch
  consumes its inputs.
- **Unlocks are permanent.** The first collected batch of a recipe unlocks its pigment as a palette colour
  forever; you still need material to paint with it.

## Recipes (v2, freeze candidate)

Every line below is **Grok's review mark, which is itself unverified**. **Before `catalog.sql`, I check each
line against at least one reference and Brandon signs the frozen table.** Times are typical real ranges; the
game time follows the rule in Open decision 1 (real time up to ~3 days; only verdigris capped at 7 days).

| Recipe | Ingredients | Real range (to verify) | Game | Yields | Tier | Note |
|---|---|---|---|---|---|---|
| Charcoal | willow twigs | hours, plus cooling (Grok: ~10 h; disputed) | 8 h | charcoal (black + 3 greys) + **ash x2** | 1 | label the shortening |
| Shell white | seashells | historically weathered months to years, then crushed | 1 d | white | 2 | not "calcining"; label |
| Yellow ochre | ochre earth | ~1 d (wash, settle) | 1 d | yellow ochre | 3 | |
| Red ochre | yellow ochre ("heated") | hours | 4 h | red ochre | 3 | no temperature |
| Gum solution | gum arabic + water | overnight | 8 h | binder | 3 | |
| Oak-gall tannin | oak galls + water | 1-2 d | 1 d | tannin | 3 | |
| Carbon ink | soot + gum solution | minutes | 30 min | black ink | 3 | bench is the slow path; legacy bottles stay |
| Iron acetate | iron + vinegar | days to ~2 wk | 3 d | iron acetate | 4 | **folk method**; history line says manuscript inks used copperas (ferrous sulfate) |
| Iron-gall ink | tannin + iron acetate + gum solution | darkens over hours | 12 h | blue-black ink (ages to brown) | 4 | |
| Verdigris | copper + vinegar | ~1-3 wk | **7 d cap** | green | 5 | label "typically 1-3 weeks; here 7 days"; toxic note; **no "closed pot"** |
| Madder lake | madder + alum stone + shell white | days | 2 d | red | 6 | no procedure |
| Weld lake | weld + alum stone + shell white | ~1-2 d | 1 d | yellow | 6 | |
| Woad blue (simplified lake) | woad + ash + water + shell white | real vats ferment for weeks | 3 d | blue | 6 | history: "a vat, simplified"; **no lye, no pH, no urine** |

**Cut:** homemade vinegar from apples; alum crystals. **Not added:** Prussian blue, lime, malachite.

**Wrong-combination texts (side mix only, fixed list):** shell white or chalk + vinegar fizzes and dissolves
(calcium acetate); iron + vinegar without tannin gives a rust-brown solution, not ink; tannin alone gives a
weak brown stain; everything else: "Nothing useful comes of it." More only when verified.

**Palette ladder (what the easel offers):** T1 charcoal = black + three greys; T2 shell white; T3 ochres, gum,
tannin, carbon ink = yellow, red-brown, brown, black ink; T4 iron-gall = blue-black; T5 verdigris = green;
T6 lakes and woad = red, yellow, blue. Ochres do not wait on vinegar, so colour is possible on day one;
green is the slow, expensive one, as it was historically.

**The notebook:** one line per discovered recipe (name, real time range, one-line history). Undiscovered
recipes show as "?". No free text.

## Discovery and the bench (rule 1, exact)

- **Primary path:** the bench lists every recipe whose inputs you all hold; you start one; the first
  successful **collect** records the discovery.
- **Side mix** (free-form): up to **4** ingredient slugs, **rate-limited to 1 per 5 s**, matched by the server
  on a **canonical sorted multiset**; it can only return one of the fixed fail texts or "Nothing useful". The
  client **never** sends a `recipe_id` for an experiment. It cannot start a batch or discover anything, so a
  script cannot fill the notebook by guessing.

## The easel and the portrait

- **Canvas:** **32x32**, palette-indexed, 1 byte/pixel (1 KB). Shown **nearest-neighbour** on the wall and
  labelled **"a small portrait"** so it never reads as a broken canvas. **Pixel index 0 = empty** (blank is
  not a pigment: it needs no unlock, costs nothing and refunds nothing). Tools: pen (1 px), smudge (charcoal
  greys by dithering), eraser, small undo stack. No layers, no import, no text, no colour picker: the
  palette is exactly your unlocked pigments that you still have material for.
- **Starter:** a new bound player gets a **charcoal stub** (≈400 pixels, once, free). The portrait exists
  from then on and can be redrawn.
- **Cost rule (never refunds):** on Save the server charges, for each colour `c`, the number of pixels whose
  **new value is `c` and whose stored value was not `c`**. Erasing to blank refunds nothing and costs
  nothing; recolouring costs the *new* colour only; painting over a pixel with the colour it already has
  costs nothing. Charged in the same transaction that stores the bitmap; rejected if you lack material.
- **In your house:** the **studio portrait** frame (named so it is never confused with the civic *profile
  drawing* in the wall slot), plus one **painting frame** later (slice 4). A 32x32 image is exactly **2x2
  tiles** at 16 px. **Pinned at the start of slice 2** against `fillHouse` (16x16): the frame is tiles
  (10,0), (11,0), (10,1) and (11,1). y=0 is the top wall. The civic profile drawing is painted 3 tiles wide
  and 2 tall, centered on the wall slot at (7,0), so it covers x 6–8 and y 0–1. Shelves are the workbenches
  at (2,1) and (3,1). The lamp is (14,1). The owner's latch is (0,13); the door is (7,15). x 10–11 is one
  tile clear of the civic frame and three clear of the lamp. (10,1) and (11,1) stay walkable floor, overlaid
  the same way the civic frame already overlays y=1. The workshop easel is the existing table at (7,4).
- **Who sees it:** the owner, plus whoever the house policy admits, checked **in the daemon's visit helper**
  (never the public `/placed` read). **Never shown to `open`-policy strangers** until there is a way to
  report or hide. A visitor already inside still sees the portrait if the owner walks out (same as today;
  not special-cased).
- **Server save:** the client submits **palette indices only** (never a PNG). Reject: wrong length, any index
  not in that player's unlocks, over-rate saves. One transaction: verify, compute cost, deduct, store.

## Guests

Guests can **gather (local RAM only), keep a starter charcoal stub for the session, and draw a portrait in
charcoal for the session**. No batches, no unlocks, no saved painting (a batch that outlives the tab *is*
guest persistence, which stays ruled out). Guest gather and cooldowns are **local code that never calls the
new endpoints**: do not "helpfully" persist them. The bench shows guests what they would make, with
"Sign in on LiquidGov to start a batch."

## Data model (all additive; nothing dropped)

Existing DBs get changes from `ensure_*` functions at startup (as `ensure_inventory_recency` /
`ensure_visit_policy` do); `schema.sql` is updated for new DBs; the catalog is `INSERT OR IGNORE` from
`catalog.sql`.

- `items`: new rows; category stays `'material'` (the CHECK cannot be altered in SQLite); pigments carry a nullable
  `pigment_hex` (ADD COLUMN).
- `recipes`: ADD `duration_s`, `tier`, `real_range_note`, `history_note` (short text, ours).
  `recipe_inputs` / `recipe_outputs` already exist and are used as designed.
- New: `batches(batch_id, player_id, recipe_id, started_ms, ready_ms, collected)`;
  `discoveries(player_id, recipe_id, first_ms)`; `gather_cooldowns(player_id, spot, next_ms, UNIQUE(player_id, spot))`;
  `paintings(painting_id, player_id, kind CHECK IN ('portrait','painting'), w, h, pixels BLOB, updated_ms,
  UNIQUE(player_id, kind))`.
- **Server clock only**; the client never sends `started_ms` or `ready_ms`. A test-only clock seam exists.
- **The orphan reap learns the new tables** (`batches`, `discoveries`, `gather_cooldowns`, `paintings`) in
  slice 1, so civic account-deletion cannot leak them the way house rows once did.

## Daemon API (all under `/presence/`, cookie identity only, no guest fallback, so **no nginx change**)

`POST /presence/gather {spot}`; `GET /presence/studio` (inventory, batches with `ready_ms`, discoveries,
cooldowns, unlocked palette, makeable-now recipes); `POST /presence/bench {recipe}` (start a batch);
`POST /presence/mix {slugs}` (side mix, rate-limited); `POST /presence/collect {batch_id}` (idempotent);
`GET/POST /presence/painting/portrait` (own) and `GET /presence/painting/<owner>/portrait` (visit-checked).
Unbound is `403 not_bound`.

Rules: **gather, bench and collect are each one transaction**. Gather: `UNIQUE(player_id, spot)`, the cooldown
row and the grant in the same transaction so two racing POSTs cannot both pass. Bench: slot cap (**3**),
counts checked, inputs deducted, batch inserted. Collect: `collected 0 -> 1` idempotently, then outputs and
discovery. `collect` is a pull against `now_ms >= ready_ms`; the client may count down from the server's
`ready_ms` but collect stays authoritative. **No thread or push for "batch finished".** **Do not poll
`/presence/studio` at presence's 2 Hz**: fetch on Workshop enter and at most every ~30 s while at the bench.
The dormant client poll of `/commons/recipes` (404) is left alone until slice 3.

## Client

Bench, easel and notebook are **full-screen DOM overlays** (a 160x240 portrait crop cannot host a 32x32
editor plus a palette in a canvas HUD), keyboard and touch. **A portrait-phone layout is part of slice 2:
without it slice 2 is unusable on a phone, and that, not SQLite, is the real ship-blocker.** A HUD line shows
"batch ready" on arrival. The inventory strip now shows counts that go down.

## Tests (each slice)

Recipe-table validation (every input/output exists, durations in bounds, **no cycle that gains material**);
server-authoritative gather/bench/collect (no materials, not ready, double-collect, slot cap, racing gathers,
unbound 403, client-sent times ignored); side mix (rate limit, cap of 4, fixed texts only, cannot discover);
painting save (locked index rejected, wrong length rejected, cost charged atomically and never refunded, the
"erase and repaint" case costs material, insufficient material rejected); migration on an **old-shape DB**
(`test_migrations`); the orphan reap removes the new rows; visibility (owner and friends yes, stranger no,
`open` no); the world flood-fill test extended for every new spot; a real-browser run per slice, including a
phone-portrait viewport for the easel. CI as usual.

## Slices (each its own BUILD; deploys need the `deploy` word)

1. **Daemon core:** tables, frozen catalog, gather/cooldown, bench/mix/collect, discoveries, consumption,
   migration, **orphan-reap extension**, tests. **No client. No droplet until `deploy`.**
2. **Charcoal easel + studio portrait + starter stub:** daemon painting endpoints (`portrait` only,
   visit-checked read), the easel overlay **including the portrait-phone layout**, the house portrait frame
   (exact tiles pinned first). Palette = charcoal until later recipes exist. This is the "do".
3. **World sources + bench + notebook + lab conversion, one deploy:** the lab does not lose its finished
   products until the bench can spend the raws; Orchard keg instead of a vinegar recipe; first-Look toast on
   converted stations; the frozen station-by-station table.
4. **Rest of the ladder + second painting frame.**
5. **Tuning** on the live site (cooldowns, yields, pixels per unit, charcoal time).

## Open decisions (recommended default marked)

1. **Time scale:** real time up to ~3 days; only verdigris capped at 7 days, labelled. *(default; Grok agrees)*
2. **Guests:** gather + session charcoal, no batches. *(default; Grok agrees)*
3. **Side mix costs nothing but cannot discover.** *(default; Grok agrees)*
4. **Portrait visibility:** owner and friends only, never `open` strangers. *(default; Grok agrees)*
5. **Canvas:** 32x32, nearest-neighbour, "a small portrait". *(default; Grok agrees)*
6. **Recipe review:** Grok's pass is the first; **I check each line against a reference before `catalog.sql`**;
   Brandon signs the frozen table. *(taken as signed: `BUILD studio 1` came after the reference pass was already
   posted here, so slice 1's catalog is the frozen table with C1-C6 applied in full -- see `studio-1-built.md`)*
7. **Legacy items** stay placeable trophies. *(default; Grok agrees)*
8. **No moderation while friends-only.** *(default; Grok agrees)*
9. **My three deviations** (pixel-level cost, visit-checked portrait in slice 2, 8 h charcoal). *(taken as signed,
   same reasoning as #6; slice 1 only actually builds the pixel-cost rule -- the portrait itself is slice 2)*
10. **Charcoal real-world time** (2-4 h vs ~10 h) is disputed between Grok's review and my draft; resolved by the
    reference check (Cennini: overnight, supporting 8 h). *(resolved; 8 h shipped)*
11. **A partial unit still costs a unit.** "1 unit ≈ 200 pixels" does not say what happens to the 1st through
    199th new pixel of a colour. Slice 2 charges `ceil(changed_pixels / 200)` of that colour, so one new pixel
    spends a whole unit. There is no remainder bank. *(slice 2; the other reading is a stored remainder, which
    would let 199 pixels be free until they add up)*
12. **A save is at most one per 5 seconds**, the same number as side-mix, because the spec says "over-rate"
    and does not give a number. A body that parses as 1024 indices counts, even if it is then rejected
    (`locked_index`, `insufficient`). A malformed body does not. *(slice 2)*
13. **The easel's unlock check, this slice, is the four charcoal indices, not `discoveries`.** The stub has to
    be paintable before any collect, and the bench is not on the client until slice 3. Index 0 is empty.
    Indices 1–4 are the only legal paint. Anything else is `locked_index`, and one such pixel rejects the
    whole save before any charge. A grey index with no grey in stock is `insufficient`, not locked: the stub
    grants black only (2 units, 400 pixels), once, on the first painting-endpoint touch, even if that player
    already holds black from the bench (the grant is the first portrait row, not a zero balance; the cap of 99
    still holds). Discovery-gating a non-charcoal pigment waits until one exists. *(slice 2)*
14. **A portrait GET fails closed when civic returns no friends list.** `visit_allowed` would treat a
    friends-policy house as a stay and allow the read. A GET is not a stay; failing open would show the
    portrait to whoever asks while civic is down. The read still goes through the visit helper after that:
    owner always; locked nobody else; an `open`-policy stranger never (even though `visit_allowed` would let
    them into the house); a friend under `friends` or `open` yes, including when the owner is not online.
    *(slice 2)*

## Reference check (2026-09-20): see `studio-refs.md`

One period or conservation source per line, verbatim and greppable (Cennini/Herringham, Merrifield incl. the
Bolognese MS., Theophilus/Hendrie, Pliny). Result: **charcoal, red ochre, carbon ink, verdigris, weld lake
confirmed; shell white, gum, tannin, iron-gall, madder, woad confirmed with corrections; iron acetate,
yellow-ochre wash/settle and several game times are unsourced.** Headline findings: Cennini ch. 33 leaves the
charcoal "till morning" (overnight), which supports **8 h** and corrects the "no clock" note; Theophilus scrapes
verdigris after **two weeks**; the Bolognese weld recipe uses **weld + ground calcium carbonate + alum**; a
period azure uses **woad froth on shell lime** but has **no ash**.

**Proposed corrections (each needs Brandon's yes; the table above is unchanged until then):**
C1 remove "ash" from woad blue and drop the charcoal ash by-product; C2 madder history line says painters'
madder lakes are later and northern; C3 iron acetate stays as an explicitly folk, unsourced step (or is cut);
C4 unsourced times show "here: N" in the notebook, and a real range only where a source gives one; C5 drop the
"weathered for years" shell-white claim; C6 charcoal's history line says "left overnight in Cennino Cennini's
manual (c. 1400); here 8 hours", not "closed pot" (which the safety rule bans in the notebook).
Not checked by me: Coates' ~10 h kiln figure and the shell-white weathering claim (no text found); CAMEO was
unreachable (403).

## Slice 1: built 2026-09-27

Brandon: `BUILD studio 1`, after this file's own reference pass had already landed. Daemon core only, exactly the
slice-1 scope above: schema, the frozen catalog (with C1-C6 applied), gather/bench/mix/collect, discoveries,
consumption, migration, the orphan-reap extension, tests. No client, no droplet. See `studio-1-built.md`.

## Slice 2: built 2026-09-27

Brandon: `BUILD studio 2`. Charcoal easel, portrait endpoints, starter stub, house frame on the tiles pinned
above. No bench screen, no world markers, no second frame, no droplet. See `studio-2-built.md`. Open decisions
11–14 are the readings taken where this file did not already pick a number.
