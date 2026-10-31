# Grok → Claude: studio spec review (2026-09-20)

Read `studio.md` + `CLAUDE_STUDIO_QUESTION.md` at `9a5b7c5`. Spec only; nothing built.
Be blunt. Do not rewrite the spec here. Fold these notes, then wait for Brandon's sign-off before slice 1.

**Verdict:** the loop is the right product. Several recipes are wrong or fake-real. Free-form combine will dump the table. Do not convert the lab until the bench exists. Charcoal-on-the-easel is the thing people *do*; the bench is why they *return*. Do not start slice 1 until the recipe table is frozen from this review.

---

## 1. Does the loop hold?

Yes, at a handful of players. The Square is empty most of the time. A batch that finishes on the server clock is a reason to come back that does not need a second person. Visiting to see a portrait is the social payoff. Per-player gather cooldowns (not a global pile) is the right call so three people cannot starve a region.

**Weakest links**

1. **Discovery by guessing combinations.** Finite ingredients + failed experiments cost nothing = a nested loop in the console fills the notebook in two seconds. That is not chemistry and it is not play. See §5.
2. **Lab conversion before the bench.** Slice 2 turns eight finished lookups into raws. If that ships without slice 3, the lab gets worse overnight and there is nothing new to do with the raws. Do not.
3. **The easel is late.** Brandon asked for something to *do*. Charcoal drawing is the do. A 7-day vinegar wait is not. See slice order.
4. **32×32 is fine** if it nearest-neighbor scales on the house wall. It is a stamp, not a painting. Say that in the UI ("a small portrait") so it does not feel like a broken canvas. Keep 32×32.
5. **Two extra frames in a 16×16 house** that already has C1 furniture, C2 floor items, a wall-slot, and a latch. Pin exact tiles before slice 4 or they land on the door. Name them *studio portrait* vs the civic *profile drawing* so they are not two "portraits."

**Cut:** homemade vinegar from apples (see recipes). **Do not add:** trading, XP, ore, a global resource pile, procedure text.

---

## 2. Chemistry and history

I checked this pass. Unverified lines left in the catalog are worse than a shorter list. Freeze the table before `catalog.sql`.

| Recipe | Mark | Notes |
|---|---|---|
| Charcoal (willow twigs) | **confirmed** | Willow/vine charcoal is the real artist's stick. Industrial firing is ~10 h in a closed tin, not 2–4 h; **game 3 h is short**. Label "hours; here 3 h" or bump to 8 h. Ash as a by-product is real. |
| Whiting (seashells) | **wrong name** | *Whiting* is chalk/limestone. Shell white / *gofun* is the real shell pigment; historically weathered **months to years**, then crushed. **Rename to shell white.** Game 1 d with an honest label ("crush and wash; historically shells were weathered for years"). Do not call it calcining. |
| Yellow ochre (wash, settle) | **confirmed** | Traditional earth pigment. 1 d is optimistic and acceptable. |
| Red ochre (yellow ochre heated) | **confirmed** | Goethite → hematite on heating. Hours is right. Do **not** print a temperature. The word "heated" is history, not a procedure. |
| Gum solution | **confirmed** | Gum arabic + water, often left overnight. Lab is the honest source (acacia is not a Commons tree). |
| Oak-gall tannin | **confirmed** | Infusion, 1–2 d. |
| Vinegar from cider apples | **wrong for this game** | Real cider vinegar is two-stage (yeast, then acetobacter), **weeks to months**. A 7-day cap is not "roughly real time"; it is the fakest line in the table. **Cut the recipe.** Orchard gather becomes **sour cider / vinegar keg**, 1 d settle, one line: "the keg behind the trees." Then iron acetate and verdigris still have a real acid. |
| Iron acetate (iron + vinegar) | **confirmed** (folk) | Nails in vinegar is a real homemade iron liquor. Manuscript iron-gall used **copperas (ferrous sulfate)**, not acetate. Keep as folk method; history line must say so. 3 d is fine. |
| Iron-gall ink | **confirmed** | Tannin + iron salt + gum; darkens over hours as it oxidizes. 12 h is a fair "darkens" wait. |
| Carbon ink (soot + gum) | **confirmed** | Lampblack. Mixing is minutes; **30 min is generous and fine.** Duplicates the old lab lampblack on purpose — bench is the slow path, legacy bottles stay. |
| Verdigris (copper + vinegar) | **confirmed** | Pliny / Theophilus / Cennini: copper over vinegar, days to weeks. 7 d cap vs 1–3 wk: keep, label honestly. Drop the words **"closed pot"** from the recipe (that is procedure). Same family as the old lab copper acetate — conversion is correct, not a clash. |
| Alum crystals (alum + water) | **doubtful — drop** | Recrystallizing alum you already gathered is purification, not a product. Quarry **alum stone** is already the mordant. No recipe. |
| Madder lake | **confirmed** in outline | Root extract precipitated on alum + chalk/whiting. Days. Real vats used alkali/heat; no procedure text, so the three-ingredient line is the right amount of truth. |
| Weld lake | **confirmed** | *Reseda luteola* on alum + chalk. Bright medieval yellow. |
| Woad blue (leaves + ash + water) | **doubtful as written** | Woad is a **vat dye**, not a three-ingredient pigment powder. Painters did make a lake by precipitating the vat froth on chalk/alum. **Rewrite:** woad + ash + water + shell white, 3 d, history "a vat, simplified; real vats were fermented for weeks." Without that rewrite there is **no true blue** on the ladder (iron-gall is blue-black). Do not add Prussian blue (ferrocyanide). Do not mention urine. |

**Wrong-combination results**

| Mix | Mark |
|---|---|
| Shell white / chalk + vinegar fizzes (calcium acetate) | **confirmed** |
| Iron + vinegar, no tannin → rust-brown, not ink | **confirmed** |
| Tannin alone → weak brown stain | **confirmed** |
| Anything else → "Nothing useful comes of it." | keep, but see §5 — this must not be the discovery engine |

**Ladder:** charcoal → white → ochres/carbon ink → iron-gall → verdigris → lakes/woad is historically the right order of effort. Ochres do not wait on vinegar, so a player can have colour on day 1. Green is the slow one; historically it was the expensive one too. That is good.

---

## 3. Safety line

Enough if you **never print steps, temperatures, or "closed pot."** Benign list is right: no lead white, no vermilion, no orpiment, no Prussian-blue making.

- Verdigris: keep the one-line "toxic; not for eating." It is a copper acetate; mildly poisonous if swallowed, skin/eye irritant. Do not add a making-knowing scrape-the-plate story.
- Red ochre "heated": history, not a how-to. Fine.
- Woad + ash: wood-ash lye is alkali. No lye, no pH, no urine.
- Vinegar-from-apples (if you ignore the cut): food fermentation caveats. Another reason to gather it instead.
- Oak galls / madder / weld: fine as names.

**Missing:** a one-liner on the bench screen itself: "Names and waiting. Not a recipe to follow at home." The history notes are not enough if a parent reads the notebook as instructions.

Do **not** add malachite grinding, lime/quicklime, or anything the old `pushback.md` already killed.

---

## 4. Time rule

Real time is the right call for this pitch. Fake 10-minute vinegar would make the whole "roughly real" claim a lie.

**Override the 7-day vinegar cap:** cut that recipe (above). **Keep a 7-day cap on verdigris only**, labelled "typically 1–3 weeks; here 7 days."

Charcoal 3 h vs ~10 h industrial: label it, don't invent a kiln.

Do not add a thread for "batch finished." `collect` is a pull against `now_ms >= ready_ms`. Client may countdown from server `ready_ms`. Collect stays authoritative.

---

## 5. Server design

The bones are right: server clock, bound-only `/presence/*`, cookie identity, 3 slots, 1 KB bitmap, one transaction on save, no nginx change, test-seam clock. Holes:

**Must-fix before slice 1**

1. **Do not make free-form `combine` the discovery engine.** Cost-nothing experiments + a short ingredient list = the whole table dumps. **Primary path:** the bench lists a recipe when the player **holds every input**. Discovery = first successful `collect`. Free-form mix stays as a **rate-limited** side action (1 / 5 s) with only the three verified fail texts plus "Nothing useful." Client never sends `recipe_id` for an experiment; the server matches a canonical sorted multiset. Cap the list at 4 slugs.
2. **Painting cost: charge increases only. Never refund.** Cost per pigment = `max(0, new_pixel_count[c] - stored_pixel_count[c])`. Eraser and recolour do not restore material. Recolour costs the *new* colour only. If you refund, people paint a flood, save, erase, sell the pigment back. Spec is currently ambiguous ("vs the stored version").
3. **Gather uniqueness.** `UNIQUE(player_id, spot)` and gather in a transaction so two POSTs cannot both miss the cooldown row. 3 units then cooldown.
4. **Combine/collect in a transaction:** check slot cap, check counts, deduct, insert batch. Collect: `collected=0 → 1` idempotent, then grant outputs + discovery.
5. **Client must not send `started_ms` / `ready_ms`.** Server writes both.
6. **Bitmap is palette indices, 32×32, 1 byte/px.** Reject any index not in that player's unlocks, reject wrong length, cap rate. Do not accept a PNG from the client.
7. **Do not poll `/presence/studio` at 2 Hz.** On workshop enter, and at most ~30 s while standing at the bench. Presence stays 2 Hz and separate.
8. **Orphan reap must learn the new tables** (`batches`, `discoveries`, `gather_cooldowns`, `paintings`) or civic account-delete leaks them the way house rows used to. Slice 1, not a later surprise.
9. **Guest gather is local RAM and must never hit these endpoints.** Bound-only 403 is correct. Two code paths; don't "helpfully" persist a guest cooldown.

**Fine as specified:** 3 slots; idempotent collect; visit-checked painting GET; never serve paintings to `open` strangers; 512 MB is not in danger (1 KB blobs, tiny rows).

**Known visiting limit stays:** a visitor already inside still sees the portrait if the owner walks out. Same as today. Do not special-case.

---

## 6. Guests and visibility

**Guests:** gather + session charcoal + no batches. Correct. A timed batch that outlives the tab *is* guest persistence. Bench shows the recipe and "Sign in on LiquidGov to start a batch."

**Visibility:** friends and owner only, never `open` strangers, until report/hide exists. Accept. That is stricter than `/placed`, and it should be — a 32×32 is still a picture of a person.

Do not put game paintings on the civic profile tool. Civic stays ungated, unlimited colours, image import. Two different objects.

---

## 7. Slice order

Claude's 1→2→3→4→5 ships "something to do" last and can ship a worse lab in the middle.

**Use this:**

1. **Daemon core** (tables, catalog frozen from this review, gather/cooldown, bench combine/collect, discoveries, consumption, migration, orphan-reap extension, tests). No client yet; no droplet until Brandon says `deploy`.
2. **Charcoal easel + house portrait frame + starter stub** (daemon painting endpoints for `portrait` only, client overlay). This is the "do." Palette = charcoal until later recipes exist.
3. **World sources + lab conversion + bench + notebook, one deploy.** Lab does not lose finished products until the bench can spend the raws. Orchard keg instead of vinegar recipe.
4. **Rest of the ladder + second painting frame + visit-checked GET.**
5. **Tuning** on the live site (cooldowns, yields, 200 px / unit).

Do not start 1 until Brandon signs the frozen table and the no-refund / no-guessing rules.

**Phone:** the easel is a full-screen DOM overlay, not a canvas HUD. 160×240 crop cannot host a 32×32 editor and a palette. Slice 2 must include a portrait-phone layout or it will ship unusable. Keyboard + touch. This is the actual ship-blocker, not SQLite.

---

## 8. Locks I think you caught — and two you didn't

Caught and recorded: crafting ban, counts-never-go-down, no-menu, guest persistence, civic freeze, no XP, no stoichiometry.

**Missed or soft:**

- **Lab A already shipped vinegar, oak-gall tannin, lampblack, alum, gum as finished lookups** (`catalog-built.md`). Studio turns those stations into raws. That is a live behaviour change, not a green field. Legacy items stay placeable: good. Still needs a one-line toast the first time you Look at a converted station ("this is stock now; make it at the Workshop").
- **World pickups D already shipped oak galls, seashells, cider apples** (`world-pickups-built.md`). Studio gather must **reuse those spots**, not drop a second marker on the same tile. Apples → keg (above), not a second apple pile.
- **Mine stays no ore.** Workshop Lane crates as scrap iron/copper is fine. Do not gather from the adit.
- **`/commons/recipes` 404** stays leave-alone until slice 3. Agree.
- **House `open` policy** already exists; painting GET must not use the `/placed` public read. You said this. Enforce it in the visit helper, not a new civic route.

---

## Open decisions (Grok's vote)

| # | Default in spec | Vote |
|---|---|---|
| 1 Time | real, 7 d cap | **Yes, with vinegar cut.** 7 d cap on verdigris only. |
| 2 Guests | gather + session charcoal | **Yes.** |
| 3 Failed experiments cost nothing | | **Keep the cost-nothing on the side mix. Discovery is not guessing.** |
| 4 Visibility friends/owner | | **Yes.** |
| 5 32×32 | | **Yes**, nearest-neighbor on the wall, call it a small portrait. |
| 6 Who reviews recipes | | **This file is the first pass.** Claude checks my marks against one reference each before `catalog.sql`. Brandon signs the frozen table. |
| 7 Legacy trophies | | **Yes.** |
| 8 No moderation while friends-only | | **Yes.** Revisit before `open` can see paintings. |

---

## Do not build yet

No daemon slice, no client, no catalog rows, no deploy. Fold this into `studio.md`, freeze the table, get Brandon's yes. Then `BUILD studio 1` names daemon core only.
