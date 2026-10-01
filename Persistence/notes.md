# Notes — ideas, decisions, parked work

Last updated: 2026-09-28

Shared working memory for **ideas and product decisions** (Grok + Claude).
Repo path: `Persistence/notes.md` on `Bbottin92/liquidgov-us` `master`.

Not bug classes. Not “what is live on the droplet.”

**How this file is used**
- Session start (both): read this file.
- During a turn: if Brandon decides, rejects, or parks something, append a dated
  entry *this turn*. Do not wait for “task done.”
- One heading per topic. Newest dated bullet at the top of that heading.
- Durable facts only. No transcript dumps. If it can be grepped from git, skip it.

---

## Shared pad

- **2026-09-28 -- `BUILD wear a pigment` built, tested, and DEPLOYED (`cdea606`; also carried the
  friend-door wording fix, which is therefore live too; 37 verify checks, 0 failed; migration and route
  confirmed live by a read-only column check and a 403 probe).** Schema + daemon route + client, not
  client-only. `CLAUDE_WEAR_PIGMENT.md` as written: a nullable
  `players.shirt_pigment_index`, a `shirt_hex` field on the in-RAM `Occupant` broadcast to everyone nearby
  (not just a private preference), `POST /presence/wear`, and a "Wear"/"Default" pair on the easel's own
  actions row. `client_files_unit.js` grew from 116 to 122 checks; `test_studio.sh` grew from 121 to 129,
  including a fresh account driven through a real gather->bench->collect chain to actually earn a
  discovery-gated colour, and a second bound account confirming a nearby friend sees the change live, no
  rejoin needed. Two real things found while testing, not by inspection: `set_shirt` originally 404'd a
  brand-new account trying to wear charcoal (fixed to find-or-create, matching gather/bench's own
  precedent), and the test harness's frozen study-clock vs. presence's real-wall-clock idle-reap nearly
  wiped freshly-joined test occupants (worked around in the test, not a production bug). The actual sprite
  recolour could not be visually confirmed in the browser preview -- its cache kept serving a stale
  `main.js`/`net.js` through several reloads even though a direct fetch proved the served file was
  correct (same artifact as the door-wording fix) -- so this is tested end-to-end but not yet eyeballed.
  See `wear-a-pigment-built.md`.

- **2026-09-28 -- Wear-a-pigment proposed, not built.** Brandon: `BUILD wear a pigment`, the second pick
  off `CLAUDE_IDEAS.md`'s build order -- but that doc only named the idea, it didn't settle how it's
  picked, whether other players see it, or where it's stored, and this one touches the daemon (unlike the
  door wording fix), so it got its own doc first: `CLAUDE_WEAR_PIGMENT.md`. Recommended: reuse the fixed
  13-pigment table's own unlock rule (charcoal 1-4 always, 5-13 once discovered), bound-only (guests never
  have discoveries); a new nullable `players.shirt_pigment_index` column (same idempotent migration
  pattern as `visit_policy`) plus a `shirt_hex` field on the in-RAM `Occupant` struct so a nearby friend
  actually sees the colour, not just you; rendered as one small `fillRect` over the sprite's existing body
  rectangle, no new atlas; picked via a new "Wear" button in the easel's own actions row (reuses its
  already-unlocked palette list) rather than a new picker screen. Awaiting `BUILD wear a pigment` against
  this doc.

- **2026-09-28 -- Friend-door wording fix built, tested, and DEPLOYED (client only; shipped in `cdea606`
  alongside wear-a-pigment).** First pick off
  `CLAUDE_IDEAS.md`'s build order. The six Homestead friend-lot doors defaulted to prompt "Closed" and a
  "Closed." toast whether you were a guest, a bound player with no friend currently home, or (mechanically
  identical either way) -- all three read the same, none said why. Now: the on-screen prompt is "Away"
  (was "Closed") until `netPollLots()` finds a mutual friend actually online; the toast splits into "Log in
  on LiquidGov to visit friends." for a guest and "No one's home right now." for a bound player with an
  empty lot, mirroring the houseDoor branch three lines below it that already did this for guests.
  `client_files_unit.js` grew from 113 to 116 checks (all three new, each mutation-confirmed): the default
  prompt, and both toast strings via a direct `tryInteract()` call with a synthetic `nearestInteractable`.
  Full suite: 116 checks, 0 failed.

- **2026-09-28 -- Ideas survey proposed, not built.** Brandon: "pull the best aspects of other relevant
  popular games and implement them" -- too open to build from directly, so `CLAUDE_IDEAS.md` surveys what
  actually fits (Animal Crossing's placeable decorations and museum, Stardew's Community Center framed as
  the Commons's own shared collection, a pigment-recolour "wear what you made" in place of new-art avatar
  customization) against what the schema already allows (the `placed` table already has x/y/rot90, only
  ever used for one wall-slot row today) and this game's own hard rules (no currency, no new art files).
  Recommended order: friend-door wording fix, wear-a-pigment, multi-item house placement, a shared
  discoveries board. Explicitly rejected: shops/currency, combat, live seasonal events, a second
  fishing-style minigame (the Studio's gather spots already are that loop), full avatar art. Awaiting a
  pick or a `BUILD <name>` once one gets its own doc.

- **2026-09-28 -- `BUILD backpack` built, tested, and DEPLOYED (`9a0896b`, client only -- the daemon cycle
  ran but changed nothing, confirmed by its own drift check; 37 verify checks, 0 failed).**
  `CLAUDE_BACKPACK.md` as written. `ITEM_CATEGORY`/`ITEM_DESC` tables next to `PLACED_ITEM_INFO`; six new
  procedural glyph shapes in `sprites.js`'s `paintItemAtlas()`; a new `commons/js/backpack.js` (11th
  script) for the Bag button/`I` key/`#backpack` grid overlay and the shared `#itemTip` tooltip (hover +
  tap, click also selects like the strip already does); the bench (`studio.js`) grew section headers, a
  per-row icon, and a progress bar derived from a new client-side `RECIPE_DURATION_MS` table (the daemon's
  `/presence/studio` sends no `started_ms`, so it's `ready_ms - the recipe's known duration` -- checked
  against `main.cpp` directly, no route change). `client_files_unit.js` grew from 105 to 113 checks (three
  new coverage checks, each mutation-confirmed). Full doc: `backpack-built.md`.

- **2026-09-28 -- Backpack proposed, not built.** Brandon: a real backpack for all items (own icon each,
  hover/tap description bubble), plus a "currency" readout and a much more organized/appealing bench.
  Asked to clarify currency first: confirmed there is no currency system anywhere in this game and none
  should be built -- dropped entirely, not part of this pass. Handoff is `CLAUDE_BACKPACK.md`: a
  persistent "Bag" button + `I` key opens a full grid view of `inventory` (the strip stays as the
  quick-reference); icons come from a small set of procedural shape categories (twig/leaf/stone/jar/
  powder/swatch/ink/bundle), not 38 bespoke drawings; descriptions are a new client-side `ITEM_DESC`
  table (no schema change -- purely client flavour text); one shared `#itemTip` tooltip node handles both
  mouse hover and touch tap; the bench gets a structural/visual pass (section headers, per-recipe icons,
  a progress bar alongside the existing countdown text, the mix row visually distinct) with no route or
  behavior change. Client only. Awaiting `BUILD backpack`.

- **2026-09-27 -- `BUILD studio 5` built and tested; DEPLOYED 2026-09-28 (`c8ef0f1`, daemon-only
  constants, no schema, no client change).** The proposal in `CLAUDE_STUDIO_5.md` as written: bench slot
  cap 3->5, Orchard keg cooldown 1 day->12h. Charcoal's 8h and verdigris's 7-day cap left untouched -- both
  already resolved by slice 1's own reference check, not guessed. `test_studio.sh` grew from 88 to 105
  checks: the slot-cap test now opens 5 batches before `slots_full`; a new section pins the keg's 12h
  cooldown by number for the first time (it had never been asserted before, only that some cooldown
  existed); one pre-existing assertion had hardcoded the shared clock's value as a literal and had to be
  fixed to compute it instead once the new keg test shifted the timeline. Full suite: 26 files, 0 failed.
  Mutation-checked (both numbers reverted in turn) -- each failed exactly where expected, restored. Deploy
  verify: 37 checks, 0 failed. See `studio-5-built.md`.

- **2026-09-27 -- Studio slice 5 proposed, not built.** Brandon: "go build slice 5," then, asked how to
  handle a slice with no named target numbers, "I propose defaults, you approve." Handoff is
  `CLAUDE_STUDIO_5.md`. Two changes proposed: bench slot cap 3->5 (13 recipes spanning 30 min to 7 days
  can fill 3 slots with long batches alone and lock out short ones for most of a week); Orchard keg
  cooldown 1 day->12h (vinegar feeds two separate multi-day chains -- iron acetate, verdigris -- gating
  the shared input at a full day on top compounds an already-long wait for no reason tied to the
  ingredient). Charcoal's 8h and verdigris's 7-day cap are explicitly NOT touched -- both were already
  resolved by slice 1's own reference check, and shortening verdigris specifically would erase the one
  deliberately slow/expensive tier the spec wants. Awaiting `BUILD studio 5`.

- **2026-09-27 -- `commons-daemon/tools/scatter_author.js` built and tested; NOT deployed (client only).**
  Grok's spec, Brandon: "build it." The hand-written `SCATTER` array (~202 props) was padded past
  `world_scatter_unit.js`'s own `total >= 150` floor, which is why the Square/Hall Lawn/Orchard were
  dressed twice (dressRegions()'s own set piece, then SCATTER again). Replaced with a deterministic
  generator (mulberry32, seed 20260927): loads `world.js` with `SCATTER` forced to `[]` in a vm (same
  trick `world_scatter_unit.js` already used) to see the real, fully-paved ground, then shuffles and
  greedily places props per a fixed caps table (Orchard/Square/Hall Lawn get zero; everywhere else has an
  explicit min/max per prop type). `--check` fails if `world.js`'s array ever drifts from a fresh run --
  the intended way to move a prop is a rule or seed change, never a hand-patched coordinate. Straightened
  the Workshop and Lab branch paths (their old dogleg-around-a-crate is moot: the generator's own
  path-adjacency rule means a crate can never regenerate onto a lane again). Result this seed: 48 props
  (from ~202), each region within its own caps. Real bug the generator itself surfaced: Shore came back
  with zero trees on the first run because the "one line" row landed directly against the water band
  (water is solid, so the whole row failed the Chebyshev-from-solid rule) -- fixed by keeping the row
  draw at least 2 tiles clear of the water on every seed, not just this one. `world_scatter_unit.js`
  dropped the old `>= 150` floor for real per-region/tile cap assertions (incl. the zeros), nine lane
  sentinel coordinates that must stay paved, and a subprocess call to `--check`. 33 checks, 0 failed.
  4 mutation checks run (groundOk disabled -- no effect this seed, an honest negative; a hand-inserted
  coordinate; a recipe under its own spec minimum; a shortened lane) -- the real ones each failed exactly
  where expected, restored and reconfirmed. See `scatter-author-built.md`.

- **2026-09-27 -- Map still had no edge after the building branches.** `3121a46` put a lane on the Workshop, the Lab, the Hall and the mine, and took the gather glyphs off. Left in place. The grass checker is gone: one field colour, darker green only as the shoulder of a lane. Added lanes, around those branches, to the Lookout, the Orchard keg, the woods (beside the galls and the twigs), the east doors and the Garden, and from the mine lane south to the ochre and the alum. Hall's flowerbeds moved into the Garden; the Square and Hall corner-trees pulled off the region box. Workshop and Lab rugs point at the door. Same tiles. `world_scatter_unit` 31/0, `client_files_unit` 105/0. Not deployed. See `wayfinding-built.md`.

- **2026-09-27 -- Deploy-record correction: the iOS intro-autoplay fix IS live, the earlier "NOT deployed" bullet
  was wrong.** Fetched live 2026-09-27 ~16:31 UTC: `https://liquidgov.us/` serves the `aee9a3d` fix (`vid.play()`,
  `.catch(finish)`, the 1.5s `vid.paused` watchdog, literal `1500`) -- verified independently, not taken on say-so.
  Corrected the 2026-09-27 "iOS Low Power Mode intro-autoplay fix... NOT deployed" bullet above and
  `intro-autoplay-fix-built.md`'s own status line and Deploy section to match. Separately, `laws/1`'s
  control-disclosure toggle (`3efe548`) was independently re-verified live too (`<details><summary>Disclaimer</summary>`
  wraps "Who holds control" in both nav and footer) -- the existing "BUILD control disclosure... DEPLOYED" bullet
  was already correct and needs no change. `deploy-daemon.sh`'s "CIVIC CHANGED since e028ec4" warning compares git
  history to the *daemon's own* marker, not to what is actually live on the box -- civic through `aee9a3d` is
  already deployed; that warning does not mean civic needs redeploying, and a later Studio deploy should pass
  `--civic-done` rather than re-syncing civic on the strength of it. Studio slices 1-4 (master `009571e`) stay
  built and undeployed; not touched by this correction.

- **2026-09-27 -- `BUILD studio 4` built and tested; NOT deployed (daemon + client, no schema, no
  droplet).** The rest of the palette ladder and the second painting frame, per `CLAUDE_STUDIO_4.md`.
  Indices 1-4 stay exactly the old charcoal table; 5-13 are the other nine pigments in tier order,
  discovery-gated (locked_index with no discovery, insufficient once discovered but out of stock).
  `save_portrait`'s cost/lock loop became one shared `save_canvas(kind)` function -- `save_painting` is
  the other one-line wrapper over it. The second canvas (`paintings.kind='painting'`, already allowed
  since slice 1) gets no stub and no ensure step: own GET never inserts (1024 zeros for "no row yet"),
  own POST's first ACCEPTED save is what creates the row, a rejected one creates nothing. Same visit rule
  as the portrait, same shared 5-second save gap (a portrait save blocks a painting save and the
  reverse). Second frame pinned at (4,0), owner's house only, no tile change. Found and fixed along the
  way: an unknown-index render fallback that drew black instead of paper, and the smudge tool silently
  half-erasing any pigment index 5+ instead of stamping solid (neither asked for by the spec directly,
  both implied by its own wording). Two of my own test assertions assumed alice's charcoal_black was 0
  when it wasn't (her earlier portrait tests had already spent/restocked it) -- fixed to check against
  her actual prior balance. The frozen-civic "fails closed" check first passed for the wrong reason (bob's
  friends-list answer was already hot in the 15s cache from checks moments earlier) -- fixed by using a
  freshly created account whose first-ever visit check is the frozen one. Full suite: 26 files, 0 failed.
  See `studio-4-built.md`.

- **2026-09-27 -- Studio slice 4 named, not built.** Handoff is `CLAUDE_STUDIO_4.md`. Palette indices stay fixed: charcoal remains 1–4 so stored portraits still mean the same colours; the other nine paintable pigments are 5–13 in tier order and discovery-gated. Cost rule unchanged (`ceil` of pixels that newly become that index, no refund). Second frame pinned at (4,0)–(5,1). It is flush against the shelf and the civic frame; this 16×16 house has no one-tile gap left for another 2×2, and the house is not growing. The second canvas reuses `/presence/painting/<kind>`, gets no stub, and uses the portrait's visit rule (owner and friends, fails closed). The workshop easel stays the portrait; the new frame opens the same overlay on the other bitmap. No deploy.

- **2026-09-27 -- `BUILD studio 3` built and tested; NOT deployed (client + one daemon field, no schema, no
  droplet).** Map/lab wiring, the bench overlay, the notebook. The daemon's `GET /presence/studio` now sends
  a discovery's own `display_name`/`real_range_note`/`history_note` and a fixed `recipe_count` (13), so the
  notebook needs no second route. `WORLD_PICKUPS` retired outright: its three spots (oak galls, seashells,
  the orchard tile) are `studioGather` markers now, same tiles, no second marker; the orchard stops granting
  `cider_apple` and gives `vinegar` on a 1-day cooldown (people who already hold a cider apple keep it).
  Lab table: copper/iron/chalk unchanged; vinegar/tannin/alum grant nothing and toast once per session;
  lampblack/gum became `lab_soot`/`lab_gum` gather spots. Six of the frozen map table's own literal
  coordinates landed inside `world_scatter_unit.js`'s 1-tile clearance around a pre-existing scatter prop
  (three of them ARE scatter flowerbed tiles) -- nudged onto clear ground per the spec's own fallback rule,
  table in `studio-3-built.md`. Also found and fixed: `world_scatter_unit.js`'s workshop check had been
  silently wrong since slice 2 added the easel (still asserted "exactly one interactable, a sign"); and the
  new marker code first broke `test_world_scatter.sh`'s own "`scatterRegions()` is the last thing
  `dressRegions()` does" wiring check, fixed by giving it its own `addStudioGatherMarkers()` function called
  right after, not folded into that function. Full suite: 26 files, 0 failed, 344s. See `studio-3-built.md`.

- **2026-09-27 -- BUILD studio 3 named, not built.** Brandon: "lets do map sources and bench."
  Handoff is `CLAUDE_STUDIO_3.md`. Client work plus one field on `GET /presence/studio`
  (discovery notes + a recipe count, no undiscovered names). Lab freeze: copper/iron/chalk
  stay trophies; vinegar, tannin, and alum toast and stop granting; lampblack and gum
  become soot and raw gum. The orchard apple tile becomes the keg. No second markers.
  No deploy. Slices 1–3 ship together when he says deploy.

- **2026-09-27 -- `BUILD studio 1` (the Studio's daemon core) built and tested; NOT deployed.** Trigger came right
  after the reference pass posted here, taken as the sign-off `studio.md` itself asks for (C1-C6 applied in full;
  the three flagged deviations recorded as taken, though only the pixel-cost one is actually built yet -- the
  easel is slice 2). Schema: `items.pigment_hex`, `recipes.duration_s/tier/real_range_note/history_note`, four new
  tables (`batches`, `discoveries`, `gather_cooldowns`, `paintings`); fresh and existing DBs converge
  (`ensure_studio_columns`/`ensure_studio_tables`). Catalog: 27 new items, 13 new bench recipes, every Lab-A item
  left untouched as a legacy trophy. Five new/extended endpoints under `/presence/` (gather/bench/mix/collect/GET
  studio), all bound-only, transactional, server-clock-only. A new test-only clock seam
  (`COMMONS_TEST_CLOCK_FILE`) had to be built (`studio.md` assumed one already existed). 88 + 10 new checks, two
  real bugs found and fixed by the tests themselves: a multi-input bench must roll back EVERY deduction, not just
  stop partway (mutation-confirmed); the orphan reap must run Studio's cleanup BEFORE house's (house's own reap
  deletes the `players` row Studio's still needs to resolve by username -- the wrong order doesn't silently leak,
  `PRAGMA foreign_keys` throws and rolls back, but the account is then stuck retrying forever). Also fixed two
  pre-existing tests that hardcoded catalog counts (`test_migrations`, `test_client_files`'s exact-equality check,
  which slice 1 breaks on purpose by being server-only). Full suite: 25 files, 0 failed. No client, no droplet.
  See `studio-1-built.md`.

- **2026-09-27 -- iOS Low Power Mode intro-autoplay fix, built, tested and DEPLOYED (civic only, verified live; corrected
  from "NOT deployed" the same day -- see the correction bullet above).** Brandon: "finish the iOS fix
  and commit it," after it was found sitting uncommitted and undocumented (stashed, no test trail). Real live trace (iPhone
  Safari, 2026-09-21): a blocked `autoplay` on the Genesis intro video silently sat on a black frame for the full 12s
  fallback; a rarer shape resolves the play promise then silently stalls. Fix (3 lines, `intro_overlay_html()`): an explicit
  `vid.play()` promise, `.catch(finish)` for the fast reject path, a 1.5s `vid.paused` watchdog for the silent-stall case.
  Verified against the REAL compiled binary: extracted the actual served intro script (found and fixed a wrong-script
  extraction bug first -- the homepage has 4 self-invoking scripts) and ran it in a fake-clock harness, 13 checks, normal
  playback unaffected, both failure shapes now end in <=1.5s instead of 12s, idempotent. Mutation-checked: the same harness
  against the pre-fix script correctly fails. See `intro-autoplay-fix-built.md`.

- **2026-09-27 -- `BUILD control disclosure` built, tested and DEPLOYED (civic only, verified live).** Grok relay (laws/1): the chapter-audio button
  was already live, done differently (real narration, not speechSynthesis) -- nothing to do there. "Who holds control" came
  out of the shared footer and the guest nav (both universal via page_wrap/nav()) and now sits behind a closed
  `<details><summary>Disclaimer</summary>...</details>`, same pattern as the file's existing "Delete..." disclosures.
  /control unchanged and still reachable. The /laws overview page's own differently-worded mention ("...of this site") was
  left alone on purpose. Verified against the real compiled binary locally (throwaway store): exactly one occurrence each in
  nav and footer, both closed, the audio button and /control untouched, a signed-in page too. civic has no CI; this is the
  repo's usual local-build-then-live-check pattern. **Found and set aside, not part of this build:** an uncommitted,
  undocumented iOS Low Power Mode autoplay fix on http_server.cpp (real bug trace, iPhone Safari 2026-09-21) -- stashed, not
  shipped; needs its own decision. See `control-disclosure-built.md`.

- **2026-09-20 — Studio v2 deviations accepted, table still unsigned.** `GROK_STUDIO_V2.md`.
  Pixel-transition cost (not histogram delta), visit-checked portrait GET in slice 2, charcoal
  8 h as a game number. Reference check next; Brandon signs; then `BUILD studio 1`.

- **2026-09-20 — Studio spec reviewed, nothing built.** `GROK_STUDIO_ANSWER.md`.
  Loop holds at a handful of players (batch-on-the-clock is the return). Free-form
  combine would dump the table — discovery is "you hold the inputs." Cut homemade
  vinegar; gather a keg. Rename whiting → shell white. Drop alum-from-alum.
  Rewrite woad as a lake (needs shell white). Charge painting increases only, never
  refund. Charcoal easel before lab conversion. Frozen table + Brandon sign-off
  before `BUILD studio 1`.

- **2026-09-19 — site sense built, not deployed.** `site-sense-built.md`.
  Guest nav + footer + apps tile for The Commons. /privacy /terms.
  Hall confirm is honest for guests. Communities keeps next=. Intro has Skip.
- **2026-09-19 — share card built, not deployed.** `og-card-built.md`.
- **2026-09-19 — share card built, not deployed.** `og-card-built.md`.
  Five stars + Myspace/ballot-box/gameboy quote on og-image-v2.jpg.
- **2026-09-19 — polish built, not deployed.** `polish-built.md`.
  Back-to-site link both sides of the door. No duplicate grass [Look]. Desktop hides Enter.
- **2026-09-19 — door built, not deployed.** `door-built.md`.
  Civic: Quad CTAs and GET /world now send people to /commons/. Guests walk in.
- **2026-09-19 — fill map built, not deployed.** `fill-map-built.md`.
  Creek/Shore banks + Shore pier. Scatter 168→202. Atlas ribbon, not P0 flood.
- **2026-09-19 — tree canopies built, not deployed.** `canopy-built.md`.
  Second-storey foliage on the tile north of each tree trunk.
- **2026-09-19 — behind trees built, not deployed.** `behind-trees-built.md`.
  Tall tiles Y-sort with actors. Also threads cam into depth-sort (strict-mode bug).
- **2026-09-19 — pickup markers built, not deployed.** `pickup-markers-built.md`.
  Glyphs on the three outdoor lookups; indoor leftover prompt removed.
- **2026-09-19 — depth sort built, not deployed.** `depth-sort-built.md`.
  Actors sort by Y; 10×3 foot shadow. Client only.
- **2026-09-19 — lit friend door + item glyphs live.** Brandon deployed `cc0498b`
  (`--label lit-door`). Verify 33/0. `GROK_LIT_DOOR_LIVE.md`. Also ships `f57d13d` item glyphs.
- **2026-09-19 — lit friend door built, not deployed.** `lit-door-built.md`.
  Online friend lots stamp T_DOOR_HOUSE instead of a gold square.
- **2026-09-19 — item glyphs built, not deployed.** `item-glyphs-built.md`.
  11 lab items, 16×16 atlas, floor/wall/chips. Client only.
- **2026-09-19 — resume name live.** Brandon deployed `7955df0`
  (`--label resume-name`) after recloning a corrupt local git store.
  Verify 33/0. `GROK_RESUME_NAME_LIVE.md`. Occupant lerp shipped in the same deploy (`f7c8ffe`).
- **2026-09-19 — resume name built, not deployed.** `resume-name-built.md`.
  Same-tab refresh skips the welcome card. Indoor button says Leave.
- **2026-09-19 — occupant lerp built, not deployed.** `occupant-lerp-built.md`.
  2Hz snapshots walk instead of teleport. Client only.
- **2026-09-19 — place banner live.** Brandon deployed `cfd1fde`
  (`--label place-banner`). Verify 33/0. `GROK_PLACE_BANNER_LIVE.md`.
- **2026-09-19 — place banner built, not deployed.** `place-banner-built.md`.
  Region names off the grass; FireRed-style banner on enter. Client only.
- **2026-09-19 — chrome polish live.** Brandon deployed `12a78ff`
  (`--label chrome-polish`). Verify 33/0. `GROK_CHROME_POLISH_LIVE.md`.
- **2026-09-19 — chrome polish built, not deployed.** `chrome-polish-built.md`.
  Title, welcome copy, hide desktop d-pad, DOM toast, paper nameplates.
- **2026-09-19 — inventory strip live.** Brandon deployed `a9a998d`
  (`--label inv-strip`). Verify 33/0, items 11 unchanged. `GROK_INV_STRIP_LIVE.md`.
- **2026-09-19 — inventory strip built, not deployed.** `inventory-strip-built.md`.
  Chips of what you hold, tap to change the item in hand. Client only.
- **2026-09-19 — D+C1+C2 live.** Brandon deployed `ccdf06f`
  (`--label objects-d-c`). Verify 33/0, items 8→11. `GROK_OBJECTS_LIVE.md`.
- **2026-09-19 — C1+C2 built, not deployed.** `house-objects-built.md`.
  Furniture in every house; floor-place last pickup, cap 8, owner only.
  Ships with D in one deploy (`--label objects-d-c`).
- **2026-09-19 — D built, not deployed.** `world-pickups-built.md`.
  Oak galls (North Woods), seashells (Shore), cider apples (Orchard).
  No limestone. Catalog 8→11 items, still 8 recipes. Client only those
  three Look markers. C1 waits.
- **2026-09-19 — lab A live.** Brandon deployed `f8d111f` with
  `--label catalog-a`. Verify 33/0, items 3→8. `GROK_CATALOG_LIVE.md`.
  Hard-refresh the Square for the second row of lab benches.
- **2026-09-19 — deploy A requested, Grok cannot SSH.**
  `CLAUDE_DEPLOY_CATALOG.md`. `9dba50b` CI green. This sandbox has
  no `liquidgov_deploy_key` (publickey denied). Command:
  `./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label catalog-a`
- **2026-09-19 — lab A built, not deployed.** `catalog-built.md`. Five
  stations, no lime: vinegar, oak-gall tannin, lampblack, alum, gum.
  `ensure_catalog(catalog.sql)` every start. Client second row. CI green.
  D and C wait.


- **2026-09-19 — scatter B accepted, already live.** `GROK_SCATTER_REVIEW.md`.
  `d2b6859` is on `/commons/js/world.js` (30926 bytes, `scatterRegions`).
  `forceRedraw` lives in `sprites.js`. Do not re-sync. Density is the
  flood-fill + 9 mutations, not a new tile type.
- **2026-09-19 — connection badge live.** `GROK_STATUS_LIVE.md`. Served
  page has `#connStatus`, 8 scripts, civic 200, internals 404. Machine
  accepted: 4s / 6 *move* failures (polls not counted — a tick has 1–4
  polls and would flash on a 1s restart). CI on `bd50119` green. No
  daemon restart. No next BUILD.
- **2026-09-19 — connection status: A.** `GROK_STATUS_ANSWER.md`. DOM
  badge outside the canvas, one line "Reconnecting…", 4s / 6 failures,
  hide on the next success. Observe only. No extra clear-delay. Machine
  in `js/net.js`. Client only after review. Not built.
- **2026-09-19 — SSG crib, no generator.** `ssgs.md`: Hugo / Eleventy /
  Astro / Vite would be a compile step. The Commons client is already
  static files on nginx; civic is request-time C++. Do not add `_site/`.
  No code change.
- **2026-09-19 — split client live, Grok independent check.** Claude's
  two-phase deploy accepted. From here, no SSH: `/commons/` is the 41-line
  shell (0 inline, 8 classic `js/*.js` tags + `style.css`), all eight
  scripts 200 `application/javascript` no-cache, stylesheet `text/css`,
  `tiles.png`/`sprites.png` 200. Civic `/` 200. Public `/internal/friends`
  and `/internal/session-user` still 404. `/commons/presence/1,1` is `[]`
  JSON. Daemon was not restarted (Claude: up since 17:23:28 UTC,
  NRestarts=0). Rollback still the old one-file page if a visitor somehow
  can't load a script. `/commons/recipes` 404 stays leave-alone.
- **2026-09-19 — notes.md restored.** Grok commit `909044f` accidentally
  overwrote this file with a placeholder while adding `makefile.md`.
  Restored byte-for-byte from `da2f8d0` (the split review), then these
  two bullets. Nothing else in the pad was meant to change.
- **2026-09-19 — Makefile crib, no Makefile.** `makefile.md`: CMake stays
  the daemon build. Do not add a Makefile that lists `.cpp` files. A
  five-line wrapper that only calls cmake is the most that would ever
  be allowed. Client is still static files. No code change.
- **2026-09-17 — two-rooms-at-once live check, no bug, no code.**
  `GROK_V3_ANSWER.md`: presence vein is spent (indoors-vanishing was the
  hole, closed), most-recent-placeable stays as shipped, cookie bind is
  the only real remaining increment. Asked for exactly one thing before
  holding: five minutes of live verification that two shared interior
  instances occupied at once don't need a correctness pass, not an
  invented "multi-instance" subsystem. Ran it against the live site
  (`https://liquidgov.us/commons/`, not local dev): tab A joined
  `instance:demo`, tab B joined `instance:lab` -- both held independent
  occupant lists on the daemon's own feed. Tab A walked back outside:
  `instance:demo` went empty (no ghost), `instance:lab` still showed
  tab B, completely unaffected. Tab A's `netRegion` correctly resolved
  back to an outdoor region, and outdoor camera-neighbor polling (two
  simultaneous region GETs near a seam) still fired correctly
  afterward. No console errors either tab. No bug found -- confirms
  `PresenceStore`'s existing "map of opaque region keys" design already
  handles this with zero special-casing, exactly as Grok expected.
  Nothing to patch, nothing committed except this bullet. **Holding
  until Brandon says BUILD and names cookie resolver** -- the first
  `http_server.cpp` edit this entire project would ever make.
- **2026-09-17 — self-nameplate + lab placeables deployed live.**
  Brandon said "deploy it live" -- unlike the shared-interiors "copy"
  round, this one changes the daemon (new `placed.hpp/.cpp`, changed
  `PresenceStore::join`/`rekey` signatures), so it needed the full
  sequence, not a file copy. Found one more real gap before applying
  anything: the live nginx config only proxies `/commons/presence/` --
  the new `/placed/<instance>` daemon route had **no matching location
  at all**, so a request to `/commons/placed/...` would have hit the
  `/commons/` *static alias* and tried to serve a file that doesn't
  exist. Added `location /commons/placed/` to `nginx-commons.conf`
  (same trailing-slash shape as presence, same reasoning) before
  touching the live box. Sequence: synced+rebuilt the daemon
  (`cmake --build -j1` on the real box), restarted the service,
  verified the new routes on loopback first, synced the client, backed
  up the live nginx config again, added the one new location, tested
  with `nginx -t` on disk (no effect on the running process) before
  reloading. Civic stayed 200 throughout, confirmed before, during
  (mid-config-swap), and after. Verified live at
  `https://liquidgov.us/commons/` in a real browser: self-nameplate
  renders over the local player for the first time in production;
  picked up chalk at the lab, placed it in the demo house wall-slot,
  confirmed on the daemon's own `/commons/placed/instance:demo`
  response and visually in a screenshot with the player moved clear of
  it. No console errors. `deploy-plan.md` updated to reflect the new
  proxied route. Left the test placement in place afterward (harmless,
  same as any real guest's) rather than manufacturing a cleanup step
  for low-stakes cosmetic state.
- **2026-09-17 — V2 stretch: self-nameplate + lab placeables (repo
  commit, not yet copied live).** Both remaining "if there's room" items
  from `GROK_V2_ANSWER.md`.
  **Self-nameplate**: `GROK_NAME_REVIEW.md`'s exact ask -- `join`/`rekey`
  now echo the *accepted* `display_name` (post-disambiguation) instead
  of the client trusting its own raw typed input. Daemon:
  `PresenceStore::join`/`rekey` take an optional `std::string*
  out_display_name`, populated from the same `disambiguate_name_locked`
  call that was already computing it internally -- no new logic, just
  exposing what was already being thrown away. Client: `netJoin`/
  `netRekey` capture it into `acceptedDisplayName`, drawn over the local
  player's own sprite the same way other occupants' nameplates already
  render. No nameplate draws until the first join response actually
  lands (never shows the raw typed name as a stand-in).
  **Lab placeables**: one placed item per shared interior instance, RAM
  only (new tiny `PlacedStore`, same shape as `GuestRegistry` --
  mutex-guarded map, matching this codebase's existing convention even
  though the single-threaded server doesn't strictly need the lock).
  `GET`/`POST /placed/<instance>`; POST requires a valid guest credential
  (griefing bar, not a security boundary) and validates the item slug is
  a real row in the `items` catalog table (a client that doesn't
  recognize an unknown slug can't happen, and a typo doesn't wedge the
  one shared slot with something nothing can render) -- explicitly not
  the same thing as `items.wall_slot_ok`, which is reserved for the
  real, later civic profile-canvas wall slot; this is "not a civic PNG"
  per the spec, any catalog item is eligible here. **Found and fixed a
  real bug while wiring this, not by inspection**: `LAB_RECIPES.item`
  held display-style strings ("copper acetate bottle", spaces) instead
  of the real catalog slugs ("copper_acetate_bottle", underscores) --
  harmless as long as it was only ever used as a local JS object key,
  but every `POST /placed/` call would have 400'd against the real
  `items.slug` column. Fixed at the source since nothing displays
  `.item` as text (confirmed by grep before changing it) -- one
  representation, not a translation table between two schemes. Demo
  house's wall-slot tile (previously pure decoration, explicitly "stays
  empty until glue exists" per its own P2-era lock) is now interactable
  -- placing there sends whatever item was most recently picked up at a
  lab station (no inventory-picker UI exists, so "most recent" is the
  only unambiguous choice); this reuses the exact tile position for a
  different, simpler, explicitly-authorized purpose than the real
  civic wall-slot feature (`GROK_V2_ANSWER.md`'s own "not a civic PNG"
  distinction), which stays exactly as parked as before.
  Verified live across two independent browser tabs: self-nameplate
  renders correctly for both the local player and (via the existing
  other-occupant path) a second guest; picked up chalk at the lab
  station with the real slug recorded; placed it in the demo house
  wall-slot; a second guest's tab, joining fresh, saw the exact same
  placed item rendered (flat color + label) with no code shared beyond
  the poll; placement persisted across an exit-and-re-enter (proving
  it's real daemon-side RAM state, not client-local); restart boundary
  re-verified for both `PresenceStore` and the new `PlacedStore`
  (RAM clears, SQLite catalogs survive); full regression suite (missing
  field 400, wrong-secret 403) still holds. Not yet copied to
  `/opt/commons/index.html` or rebuilt on the droplet -- repo commit
  only, same as shared interiors was before "copy." Civic untouched, no
  deploy, no nginx.
- **2026-09-17 — shared interiors copied live.** Brandon said **copy**
  after `GROK_V2_REVIEW.md` accepted `6ce42cf` with two checks named
  before doing it: passerby must not draw/walk indoors, and the
  colon-containing instance path (`/commons/presence/instance:demo`)
  must resolve through the production proxy as JSON, not a civic HTML
  404. Verified both live (screenshot inside the demo house shows only
  the local player; the colon path returns `[]`/occupants correctly)
  before and after the copy, not just trusted from the local round's
  own testing. Ran `commons-daemon/deploy/sync-client-to-droplet.sh`
  (client only, `/opt/commons/index.html`) -- no daemon rebuild, no
  nginx change, exactly as scoped. Fixed ownership after rsync (root
  by default, `chown liquidgov:liquidgov`). Verified end-to-end in a
  real browser at `https://liquidgov.us/commons/`: entered the demo
  house, `netRegion` resolved to `"instance:demo"`, and the daemon's
  own feed showed the exact same occupant. Daemon (`commons-daemon`,
  still active) and civic (still 200) both unaffected throughout.
  Self-nameplate, lab placeables, Mine/Workshop/Hall, cookie bind, and
  wall-slot all still parked, exactly where they were.
- **2026-09-17 — V2: shared interiors (BUILD).** `GROK_V2_ANSWER.md`:
  V1's actual hole wasn't a 13th region, it was that "indoors is a
  single-player cutscene" — the client called `netLeave()` on every
  house/lab entry, so real presence was wasted the instant two guests
  would have been in the same room. Mine/Workshop/Hall stay shut (each
  needs a different *kind* of feature, not "one more 16x16") — this
  round only networks the two interiors that already exist, as one
  **shared** instance each (`instance:demo`, `instance:lab`), not
  per-player houses. No daemon changes at all: `PresenceStore` already
  treats region as an opaque string key, so an instance key is just
  another region to it, same 2Hz/secret/24-cap.
  Client refactor: generalized `regionKeyFor`/`worldToRegionLocal`/
  `regionLocalToWorld` to accept either an outdoor `REGIONS` entry or a
  plain instance-key string (interior local coords need no supermap
  AABB offset — the instance's own tile space IS the whole region);
  unified `netJoinOutdoor`/`netRekeyOutdoor`/`netMoveOutdoor` into
  plain `netJoin`/`netRekey`/`netMove` that work for both; added
  `netPollCurrentRegion` (single-region poll, no camera-neighbor union
  needed since there's only ever one instance to be in); frame()'s
  network tick now runs the same join-on-null/rekey-on-mismatch pattern
  for whichever `mode` is active, indoors or out, so `enterInterior()`/
  `exitToOutdoor()` need **zero** explicit network calls — the very
  next tick detects the region mismatch and rekeys automatically, the
  same mechanism that already handled outdoor-to-outdoor crossings.
  `netHandleAuthFailure`'s remint path simplified the same way (resets
  `netRegion` to null, the natural tick picks up wherever the player
  actually is). Rendering generalized to fire for either mode.
  Verified live: entering the demo house rekeys clean (no ghost in the
  outdoor region, correct region-local coords in the daemon's feed); a
  second tab entering the same house sees the first guest as a real
  occupant (the actual hole, closed); exiting rekeys back outdoors with
  no ghost in the instance; the lab's walk-in door trigger (not just
  the demo house's Enter-to-exit) also rekeys correctly; outdoor
  camera-neighbor polling re-verified unaffected by the refactor; no
  console errors either tab. Not deployed to the live droplet yet —
  this is a repo commit only, separate explicit go-ahead needed before
  touching `/opt/commons/index.html` again. Civic untouched, no deploy,
  no nginx.
- **2026-09-17 — decided: world-reachable guest mint is the product, not
  a hole.** Independently confirmed by Grok (`GROK_LIVE_CHECK.md`, no
  SSH) after the deploy: civic still 200, `/commons/` shows the real
  client, `GET /commons/presence/1,1` returns `[]` (JSON, proving the
  slash rewrite holds in front of a second independent check, not just
  Claude's own). Explicit product decision, not an open question:
  anyone reaching `POST /commons/presence/guest` and joining presence
  (bounded by the existing 24/region, 96/global caps) is the intended
  anonymous-visiting design, same as walking into the Square in person
  — do **not** "fix" this later by gating it behind a real LiquidGov
  account or opening LGP1 from the daemon. Cookie bind (a *promotion*
  once it exists, per `presence.md`/`cookie-bind.md`) stays the next
  queued item, not triggered yet. `/opt/liquidgov-us` (the leftover
  clone found during deploy) stays as-is — leave it unless deliberately
  deleted on purpose. No further deploy this hour.
- **2026-09-17 — commons-daemon is live on the droplet.** Brandon said
  "go" after `c024c27`'s deploy-preflight fixes. Ran the full
  `Persistence/TheCommons/deploy-plan.md` sequence for real against
  `143.198.159.58` for the first time in this project's history:
  synced daemon source (`schema.sql` landed as a real dereferenced
  file, confirmed, not a dangling symlink) and the client separately,
  built with `cmake --build -j1` on the actual 512MB box (`FetchContent`
  reached sqlite.org fine, no tarball fallback needed), installed
  `commons-daemon.service` (`active (running)`, ~1-2MB RSS, well under
  the 64M cap), verified the daemon on loopback *before* touching
  nginx, backed up the live nginx config, edited in the three
  `/commons` locations, tested on disk with `nginx -t` (which does not
  touch the running process) before reloading, then reloaded. Civic
  stayed `200` the entire time, never interrupted. Verified end-to-end
  including **a real browser loading `https://liquidgov.us/commons/`
  for the first time ever** — minted a real credential, joined
  presence, and the daemon's own feed showed the exact same occupant.
  Full join/move/leave/wrong-secret-403 lifecycle re-verified over real
  HTTPS through the proxy afterward. `deploy-plan.md` updated in place
  to record what actually happened rather than staying a hypothetical.
  **New, unexpected, non-blocking finding from actually being on the
  box:** `/opt/liquidgov-us` exists — a full git clone of this repo
  (`origin` remote, at `4d38c92`), separate from `/opt/liquidgov` (the
  real running app, confirmed via the systemd unit and the live
  process's own cwd). Not touched, not part of this deploy, flagged for
  Brandon since it was genuinely unknown before today (possibly a
  staging/migration experiment from another session) — worth him
  knowing what's actually sitting on his own droplet.
- **2026-09-17 — commons: deploy preflight, two real holes closed, still
  no SSH.** `GROK_DEPLOY_REVIEW.md`/`CLAUDE_DEPLOY_PREFLIGHT.md` on
  `fb889c0`: accepted the slash fix, found two more real problems before
  anything could actually go live. Both fixed as files only:
  1. **`deploy-plan.md`'s step 4 named a path that doesn't exist** --
     `cp /opt/commons-daemon/commons/index.html` assumed `commons/` was
     inside `commons-daemon/`; it's a sibling directory at the repo
     root. Added `commons-daemon/deploy/sync-client-to-droplet.sh` (new
     script, rsyncs the actual `commons/` dir to `/opt/commons/`,
     separate from the daemon's own sync script so the nginx static
     alias directory never risks getting the daemon's source/build tree
     dumped into it) and fixed `deploy-plan.md`'s sequence to call it.
  2. **The shipped client would have talked to the *visitor's* loopback,
     not the droplet.** `DAEMON` was unconditionally
     `http://127.0.0.1:8765` (or a loopback-only `?daemon=` override) --
     curl on the droplet could prove nginx worked, but no real browser
     ever could have. Fixed: off loopback, `DAEMON` is now
     `location.origin + "/commons"`; every existing call site already
     builds URLs as `DAEMON + "/presence/..."`, so this composes
     correctly with the nginx proxy with zero other client changes
     (`https://liquidgov.us/commons` + `/presence/join` is exactly what
     `location /commons/presence/` matches). Loopback behavior
     (`?daemon=` allowlist, local dev default) is untouched. Verified
     live: default loopback join still works exactly as before; the
     production string composition checked directly
     (`https://liquidgov.us/commons/presence/join`,
     `.../presence/1,1`) since there's no live non-loopback deploy to
     load the page from yet.
  Also did the two optional cleanups `CLAUDE_DEPLOY_PREFLIGHT.md` named:
  a `location = /commons { return 301 /commons/; }` redirect in
  `nginx-commons.conf` (bare no-slash URL used to fall through to
  civic's own `location /` and 404 there — confusing to debug blind),
  and patched `nginx.md` itself so its own presence-location example
  no longer advertises the double-slash 404 shape or the `8091`
  placeholder port (this daemon's actual coded default is `8765`) to
  whoever pastes from it next. Still no SSH, no droplet contact, no
  `nginx -t` against anything real — waiting on Brandon's "go."
- **2026-09-17 — commons: sibling deploy staged, nothing applied.**
  "Let's move forward" → confirmed the direction was sibling deploy →
  confirmed the mode was "stage first, apply after review" before
  touching anything, given this is the first round in the whole project
  to go anywhere near the actual droplet. Staged, not run:
  `commons-daemon/deploy/commons-daemon.service` (systemd unit --
  `User=liquidgov`, loopback-only via `IPAddressAllow`/`Deny`,
  `MemoryMax=64M`, `Restart=on-failure`, no `After=`/`Requires=`
  coupling to civic's own unit either direction), `nginx-commons.conf`
  (the two `location` blocks per `nginx.md`'s shape), and
  `deploy/sync-to-droplet.sh` (rsync, modeled on civic's own script,
  targeting the sibling path `/opt/commons-daemon`, `-L` to dereference
  the `schema.sql` symlink so it doesn't arrive on the droplet as a
  dangling path string -- exactly the caveat `GROK_DAEMON_REVIEW.md`
  already flagged). Full sequence, pre-flight checks (droplet outbound
  internet for `FetchContent`, disk/RAM for a `-j1` build), and rollback
  written up in `Persistence/TheCommons/deploy-plan.md`.
  **Real bug found while deriving the nginx config, not copied blind**:
  `nginx.md`'s own example (`location /commons/presence` without a
  trailing slash + `proxy_pass .../` with one) sends
  `/commons/presence/join` upstream as `//join`, not `/presence/join` --
  traced through nginx's documented rewrite rule, not guessed. Every
  presence call through that shape would 404 against this daemon's
  exact-match routes, never caught because no nginx config for this
  project has ever actually been installed anywhere. Fixed by matching
  trailing slashes on both sides (`location /commons/presence/` +
  `proxy_pass .../presence/`), nginx's own documented safe pattern.
  Flagged explicitly in both the config file's header and
  `deploy-plan.md` rather than silently deviating from the accepted
  spec. Explicitly NOT done this round (listed in `deploy-plan.md`):
  wiring `commons/index.html` itself to call the same-origin path
  (still loopback/`?daemon=` only), cookie bind (still blocked on a
  civic-side change per `cookie-bind.md`), and proxying `/health`,
  `/items`, `/recipes`, `/maps` (the client never calls them over the
  network today). No SSH, no droplet contact, no `nginx -t` run against
  anything real. Civic files untouched.
- **2026-09-17 — commons: cookie bind, design-only (`cookie-bind.md`), no
  code.** "Should-fix cookie bind next" was flagged before doing anything
  else this round: unlike every prior should-fix, this one is
  structurally blocked by *two* of the project's hardest standing rules
  at once, not just deferred by choice. `lg_session` is `HttpOnly` +
  host-only with no `Domain=` (`cookie.md`) so the browser will never
  send it to the daemon's current, different origin without the
  same-origin nginx round; and resolving the token to an account needs
  `account_from_req()`'s chain, which lives entirely inside
  `http_server.cpp` and isn't callable from outside it without a civic
  change (`GROK_GLUE_REVIEW.md` already parks exactly this as "not this
  week"). Asked which slice was actually wanted rather than guess; got
  design/document only. Wrote `Persistence/TheCommons/cookie-bind.md`:
  the presence-specific contract that `glue.md`/`cookie.md` don't cover
  -- a bound occupant's id is `"u:" + lg_username` (already the exact
  scheme `presence.hpp`'s own `Occupant::id` comment named, written
  before this doc existed), authenticated per-request by a *resolved
  cookie*, never a client-supplied `id`, and never mixed with this
  session's guest `(id, secret)` scheme for the same request; promotion
  is meant to relabel an active guest's row in place per `presence.md`,
  but no `PresenceStore` primitive does that today -- flagged as an open
  design question (a new `rebind()` vs. an unobservably-fast leave+join)
  rather than picked. No code changed. Civic untouched, no deploy, no
  nginx.
- **2026-09-17 — commons-daemon: CORS echoes a specific Origin, no more
  wildcard (should-fix, closed).** `cors-preflight.md`'s "What `*`
  forbids later": a credentialed request (`credentials: include`, a
  forwarded `lg_session` cookie) can never use `Allow-Origin: *` --
  browsers hide the response outright. Built that target shape now,
  ahead of glue/cookie-forward actually existing, since nothing about it
  is unsafe to ship early: this daemon has no cookie of its own and the
  client never sets `credentials: include` today, so
  `Allow-Credentials: true` is inert until something actually needs it,
  not a new hole. `http.cpp`: `cors_header_for(origin_of(head))` computed
  once per request --
  - Trusted (loopback) `Origin` -> that exact value echoed back plus
    `Access-Control-Allow-Credentials: true`. Never `*` + Credentials
    together (invalid, browsers reject it) -- this is why the header
    block only exists in the "echo one exact origin" shape, no wildcard
    path left in the code at all.
  - Untrusted `Origin` (anything not loopback, including literal `null`
    from a `file://` page) -> no CORS header at all. Fails closed;
    `Origin` is never reflected blindly.
  - No `Origin` header (curl, same-origin, non-browser callers) -> no
    CORS header either -- harmless, browsers only consult it for an
    actual cross-origin fetch.
  Deliberately narrower than the client's own `?daemon=` allowlist
  (`4e9bfbd`, which also trusts RFC1918 LAN boxes): that param decides
  which *backend* the client will talk to, this decides which *other
  pages* get to read this daemon's responses, so it stays loopback-only
  per the review's literal "loopback list now; `https://liquidgov.us`
  later" -- the production origin gets added at the actual deploy round,
  not now. Verified live: curl with `Origin: http://localhost:8090` and
  `http://127.0.0.1:8090` both get the header echoed + Credentials;
  `https://evil.example`, literal `null`, and no-Origin-at-all all get
  no CORS header, checked on the normal response path, the `OPTIONS`
  preflight, and the 413 path; the real client in a real browser (not
  just curl) still joins/polls normally against the new header shape.
  Full regression (catalogs, missing-field 400, wrong-secret 403) holds.
  Civic untouched, no deploy.
- **2026-09-17 — commons: allowlist `?daemon=` before this file can ever
  be public (should-fix, closed).** `GROK_DAEMON_URL_REVIEW.md` flagged
  the override added in `92daa22`: a shared link with
  `?daemon=https://evil.example` would make the page mint a guest secret
  and POST it straight to the attacker, harmless only because the page
  is loopback-only today. Gated it *now*, before any deploy, per the
  review's exact spec:
  - `?daemon=` is honored at all only while the page itself is being
    viewed from loopback (`127.0.0.1`/`localhost`/`::1`) -- on a real
    deploy the param is ignored outright, not just untrusted *values* of
    it, since the eventual fix there is the relative proxy, not this
    param.
  - Even then, the value must parse as a `URL` with `protocol === "http:"`
    and a host that's loopback or RFC1918 private (`10.*`,
    `172.16-31.*`, `192.168.*`) -- no `https:`, no `javascript:`, no
    arbitrary DNS name.
  - Anything that fails either check falls back to the same
    `http://127.0.0.1:8765` default as before; nothing untrusted is ever
    used.
  Verified live: default unaffected (real join/poll still works exactly
  as before); a trusted loopback override (`127.0.0.1:9999`) is honored;
  `https://evil.example`, `http://evil.example` (untrusted DNS, even
  over plain http), and `javascript:alert(1)` are all rejected and fall
  back to the default; the RFC1918 boundary itself checked directly
  (`172.15.*` rejected, `172.16.*`/`172.31.*` accepted, `192.169.*`
  rejected, `192.168.*` accepted). Civic untouched, no deploy. The
  daemon's own `Access-Control-Allow-Origin: *` stays parked and
  untouched per `cors-preflight.md` -- that's legal today only because
  the guest secret travels in the JSON body, not a cookie, and becomes
  the actual should-fix once glue/cookie-forward exists.
- **2026-09-17 — commons: `?daemon=` override for the hardcoded loopback
  host (should-fix, closed as far as it can go without nginx).**
  `GROK_PRESENCE_REVIEW.md`: "`NET` host is loopback `:8765`. Grok
  preview and `liquidgov.us` will not see WSL. Relative `/commons/`
  proxy is the nginx round." That's true regardless of how the constant
  is spelled -- no client-side change makes a remote preview sandbox or
  a real visitor's browser able to reach a developer's own WSL loopback.
  The actual fix (same-origin relative path, nginx proxying it to the
  daemon) stays explicitly out of scope: no nginx, no deploy, this round.
  What's cheap and real: `DAEMON` now reads an optional `?daemon=` query
  param before falling back to the same `http://127.0.0.1:8765` default
  as before, so pointing the client at a different reachable daemon (a
  LAN IP, a tunnel, another dev box) is a URL, not a source edit.
  Verified live: default (no param) still joins/polls the real daemon
  exactly as before; `?daemon=http://example.invalid:9999` correctly
  overrides `DAEMON` and degrades to the local-only passerby simulation
  with no thrown error (same graceful-offline path as an unreachable
  default daemon). Also folded in Grok's correction from the
  `display_name` round (`GROK_NAME_REVIEW.md`, `f2132ad`): there is no
  self-nameplate today (checked `fillText` call sites directly -- only
  the passerby and other-occupant labels exist), so nothing to fix there
  yet; noted for whenever one lands, `join`/`rekey` should echo back the
  *accepted* `display_name` so a self-label draws the disambiguated
  string, not the raw typed input. CORS `*` still parked. No deploy.
- **2026-09-17 — commons: region-local display_name disambiguation
  (should-fix, closed).** `GROK_PRESENCE_REVIEW.md`: "Row hijack is
  closed; name spoof is not. Fine until cookie bind." Guests have no
  bound identity yet (no `lg_username` until glue lands), so this doesn't
  try to reserve a name globally or across time -- `display_name` is
  still whatever a guest types, and reusing an old name after everyone's
  gone is fine, by design ("nothing is saved"). What it does guarantee
  now: no two *simultaneously present* occupants in the same region share
  a `display_name`. `PresenceStore::disambiguate_name_locked` (daemon
  only, `presence.hpp`/`.cpp`) checks the destination region's current
  occupants before every insert in `join_locked` (covers both `join` and
  `rekey`, its only caller) and appends " (2)", " (3)", etc. until unique,
  excluding the caller's own row so a returning guest never collides with
  itself. `move` never touches `display_name`, so this only needed to
  land in one place. No client change needed: the client always resends
  its original chosen name on join/rekey, and the daemon re-disambiguates
  fresh against whichever region it's actually entering each time.
  Verified live: two guests both joining as "Ash" in the same region get
  "Ash" and "Ash (2)"; a third guest naming themselves "Ash" in a
  *different* region is untouched (no false global collision); rekeying
  into a region that already has "Ash" and "Ash (2)" correctly lands on
  "Ash (3)"; `move` leaves an already-disambiguated name alone. Full
  regression (catalogs, 413 cap, missing-field 400, wrong-secret 403,
  restart boundary) still holds. Civic untouched, no deploy. Found and
  fixed an unrelated test-hygiene issue while verifying this: a browser
  tab left open from an earlier round kept running its game loop in the
  background against the live daemon across a `preview_stop` (which only
  tears down the static file server, not an already-loaded page's JS) --
  closing stray tabs matters, not just stopping the dev server, when
  actually verifying a "fresh restart."
- **2026-09-17 — commons: camera-neighbor region polling (should-fix, closed).**
  `70bc5b8` shipped presence networking as a stated V1 simplification:
  only the player's own joined region was polled/rendered, not every
  region whose AABB intersects the camera (`presence.md`'s literal
  tick-rate spec, and the exact FireRed seam failure visiting is meant to
  beat). Closed it: `regionsIntersectingCamera(camX, camY)` walks all 12
  `REGIONS` and keeps whichever ones' outdoor tile AABB overlaps the
  current viewport (typically 1-2 on this phone-sized 160x240 viewport,
  presence.md's own worked example says 1-4); `netPollRegions()` fires a
  `GET /presence/<region>` per intersecting region in parallel (one
  region's fetch failing doesn't drop every other region's occupants) and
  merges the results, tagging each occupant with which region it came
  from so `regionLocalToWorld` converts correctly at render time.
  `netRegion` still means exactly one thing -- which region the player's
  own row is joined/rekeyed into -- polling breadth is a separate, wider
  concern layered on top, not a redefinition. Also hoisted the camera
  (camX/camY) computation to the top of `frame()` so the networking tick
  and the draw call share one computation instead of two. Verified live:
  moved a tab near the Square/Lookout seam and confirmed two simultaneous
  `GET /presence/1,0` + `GET /presence/1,1` polls fire per tick; placed a
  real occupant in the neighboring region via curl and watched it render
  correctly past the seam in the browser (screenshot-confirmed, not just
  a network-log check); moving back to the region center correctly drops
  it from `otherOccupants` again. Civic untouched, no SSH/systemd/nginx,
  no deploy. Remaining should-fix items from `GROK_PRESENCE_REVIEW.md`
  (`display_name` spoofing, CORS `*`, hardcoded loopback host) still
  parked.
- **2026-09-17 — commons: guest credentials survive an interior stay.**
  Grok caught a real must-fix in `70bc5b8`'s presence networking
  (`GROK_PRESENCE_REVIEW.md`/`CLAUDE_PRESENCE_PATCH.md`): `GuestRegistry`
  reaped credentials on the same 30s clock as `PresenceStore`'s plaza-slot
  reap, but a guest in a house/lab stops heartbeating presence entirely
  (`netLeave()` on entry, nothing calls `verify()` again until they walk
  back out). Any interior stay over 30s meant the credential died while
  the guest was still very much active, and exiting got a silent 403 with
  no way back in. Fixed both ways the patch asked for:
  1. **Daemon**: `GuestRegistry::kIdleMs` raised from 30s to 30 minutes
     (`guest.hpp`) -- `PresenceStore`'s own 30s plaza-slot reap is
     untouched, so a crashed/closed tab still frees its region slot fast;
     only the credential's own lifetime changed.
  2. **Client**: `join`/`move`/`rekey` now check for a 403 response and
     call a new `netHandleAuthFailure()` -- mints a fresh credential and
     rejoins from the player's current outdoor position, guarded by
     `netReauthInFlight` so one bad response can't fire overlapping
     remints. This also covers a daemon restart mid-session, which the
     30-minute window alone can't (RAM state is gone either way).
  Verified live: entered the demo house, waited 32s (past the *old* 30s
  window), walked out -- same guest id, clean `200` on rejoin (not a
  remint), daemon confirms the occupant reappeared. Separately killed and
  restarted the daemon while a tab sat outdoors (wipes `GuestRegistry`
  even with the 30-minute window) -- the next throttled move/poll hit a
  403, `netId` changed (real remint), rejoin succeeded, no thrown error,
  two-tab presence still worked afterward. Full regression set (catalogs,
  413 cap, missing-field 400, wrong-secret 403) still holds. Should-fix
  items explicitly left for later per the review: camera-neighbor
  polling, `display_name` spoofing, CORS `*` (fine until `lg_session` is
  ever forwarded), hardcoded loopback host. Civic untouched, no SSH/
  systemd/nginx, no deploy.
- **2026-09-16 — commons real presence networking + server-issued guest ids.**
  Brandon asked for a client-side round; found first that `commons/index.html`
  had zero daemon wiring at all (no `fetch` calls anywhere -- presence was a
  pure local simulation, `otherRealOccupantsInRegion` was a hardcoded 0).
  Rescoped with Brandon to full presence wiring, id-first, rather than
  building an inert guest-id endpoint nothing would call. Landed both sides:
  - **Daemon**: new `GuestRegistry` (`src/guest.hpp/.cpp`) mints a fresh,
    unguessable `(id, secret)` pair via `POST /presence/guest` --
    `id` is the public occupant label already broadcast by every
    `/presence/<region>` GET; `secret` is a private bearer credential
    returned only once and never echoed back. `join/move/rekey/leave` now
    all require a matching `secret`, constant-time compared, closing a real
    (not just hypothetical) hijack: any client in a region can already see
    every occupant's `id` from the GET feed, so before this a client could
    replay someone else's `id` in a `join` call and steal/rename their row
    outright -- confirmed live (wrong secret -> 403, occupant row
    untouched; unknown id -> 403; missing secret -> 400 same as any other
    missing field). Unclaimed credentials expire after 30s
    (`GuestRegistry`'s own idle window, same shape as `PresenceStore`'s).
    Also added CORS (`Access-Control-Allow-Origin: *` + an `OPTIONS`
    preflight handler in `http.cpp`) -- needed the moment any browser page
    calls this loopback daemon cross-origin, which nothing did before now.
  - **Client**: `commons/index.html` now really talks to the daemon.
    `enter()` mints a credential; the outdoor frame loop joins on first
    tick, rekeys on crossing an outdoor AABB (no raw leave+join, matching
    presence.md's "no ghost in the old list"), and throttles move+poll to
    ~2Hz (presence.md's tick rate) rather than every render frame.
    `otherRealOccupantsInRegion` now reflects a real poll instead of a
    hardcoded 0, so the passerby's hide-rule reacts to actual other
    players for the first time. Entering a house/lab calls a clean
    `netLeave()` (interiors use `instance:<id>` keys presence.md says V1
    doesn't wire for guests); exiting rejoins automatically via the same
    join-on-null-region path used at login, no separate code path needed.
    `x,y` sent as fractional region-local tile coordinates (not integers,
    not world-absolute) -- satisfies presence.md's literal "tile
    coordinates local to that region" while keeping sub-tile smoothness.
    **Known V1 simplification, stated not hidden**: only the player's own
    current region is polled/rendered, not every region whose AABB
    intersects the camera (presence.md's actual tick-rate spec) -- proves
    real multiplayer works, but a neighbor just past the camera edge won't
    show yet. Remote occupants render at a static standing frame (no
    animFrame in the occupant feed); smoothing between polls is a later
    nice-to-have.
  - **Bug found and fixed during live testing, not by inspection**:
    `netLeave()` first used `navigator.sendBeacon` with a JSON `Blob` body.
    That's cross-origin-preflighted, and the actual POST failed
    (`net::ERR_FAILED`) against this daemon even though the `OPTIONS`
    preflight itself succeeded -- and `sendBeacon()` still returned `true`
    regardless (that return value means "queued," not "delivered"), so the
    `if (!sent) fetch(...)` fallback never fired and a house/lab entry
    silently never told the daemon to leave. Switched to plain
    `fetch(..., {keepalive:true})`, already proven working for every other
    call; interior-entry leave now verified reaching the daemon (200 OK).
    A genuinely undelivered leave (e.g. an abruptly closed tab, tested via
    closing the tab outright) still isn't guaranteed -- no browser
    guarantees that -- but `PresenceStore`'s existing 30s idle reap is the
    correct backstop and was reconfirmed cleaning up that exact case live.
  Verified end-to-end in two real browser tabs against the live daemon
  (not just curl): two guests join, see each other by name (not the
  passerby), region-crossing rekeys with no ghost left in the old region,
  interior entry/exit leave-and-rejoin correctly, an unreachable daemon
  degrades to the pre-existing local-only simulation with no thrown
  errors, and the full prior regression set (catalogs, 413 cap, missing-
  field 400, restart boundary) still holds. Added `commons-client` to the
  shared (global, not project-local) `.claude/launch.json` alongside the
  existing `liquidgov-http` entry -- a static `python3 -m http.server` for
  `commons/`, port 8090, `liquidgov-http`'s own entry untouched.
- **2026-09-16 — commons-daemon: should-fix round (reap, move timestamp,
  schema dedup, sqlite via FetchContent).** Brandon scoped this round to
  "should-fix daemon items" only (not client, not sibling/deploy) via
  explicit confirmation. Four of `GROK_DAEMON_REVIEW.md`'s five should-fix
  items landed:
  1. `move` now stamps `last_update_ms` on every call — occupant JSON no
     longer looks frozen between joins.
  2. Idle reap: `PresenceStore::reap_locked` runs under `mu_` at the top
     of every public method (join/move/rekey/leave/occupants/global_count)
     and drops any occupant whose `last_update_ms` is more than 30s
     (`kIdleMs`) stale. A crashed/closed tab no longer holds a cap slot
     until process restart. Single-threaded, lazy-on-access — no timer
     thread, matching the daemon's existing synchronous style.
  3. `schema.sql` dedup: `commons-daemon/schema.sql` is now a symlink to
     `Persistence/TheCommons/schema.sql` (verified content was already
     byte-identical apart from a comment) — one source of truth, no more
     copy-drift risk.
  4. Vendored `sqlite3.c`/`.h` (9.5MB) dropped from git; CMake now
     `FetchContent`s the same 3.53.4 amalgamation from sqlite.org, pinned
     by SHA256 (downloaded once with Brandon's explicit permission to
     compute the hash, content verified byte-identical to the prior
     vendored copy before switching). Real tradeoff accepted: the build
     now needs network access at configure time; it was previously fully
     offline. `db.hpp`'s include path updated to match (`"sqlite3.h"` via
     the target's public include dir, not a relative `third_party/` path).
  **Deliberately not done:** server-issued guest ids (should-fix #3,
  impersonation resistance). This can't be meaningfully half-done
  daemon-side — the server minting an id nobody's client asks for or
  stores achieves nothing, and `commons/index.html` still generates its
  own guest id today. Stays parked for an explicit client-side round
  before nginx is ever in front of this daemon. Verified live: full
  regression set, `move` timestamp advances on move, an untouched
  occupant is gone after >30s while one refreshed via `move` survives,
  clean build from a bare `rm -rf build` (proving FetchContent actually
  works, not just "worked once"), and the restart boundary (SQLite
  catalogs survive, RAM presence + reap state do not). Civic untouched,
  no SSH/systemd/nginx, no deploy.
- **2026-09-16 — commons-daemon: body-cap nit, truncate to `Content-Length`
  after the split.** Grok caught a gap in the 413 patch (`d2b5bf4`): the
  1 MiB header guard bounds `raw`, not this request's declared body — so
  even when `Content-Length` is legal (<=64KiB), the initial `head`/`body`
  split in `serve_one` could already hand back more bytes than `need`
  (whatever trailed the header terminator in the same reads), and nothing
  trimmed it back down. Not the 5GB hole (413 still fires correctly for an
  illegal length), but trailing/pipelined-looking bytes could silently ride
  along as part of this request's body. Fixed: after the read loop,
  `body.resize(need)` whenever `body.size() > need`. Verified live: a
  legal small `Content-Length` with garbage glued on in the same packet
  still joins clean (occupant reflects only the real JSON), the 413
  boundary and normal regression set are unaffected, and a restart still
  shows the RAM/disk boundary holding.
- **2026-09-16 — decided.** GitHub shares `Persistence/notes.md` (+ README) only.
  `context.md` and `state.md` exist in the *old* Grok sandbox and were never
  pushed. Do not invent fakes. Do not have Claude create them. A fresh Grok
  hard-gate that requires those two files will fail until we drop that read or
  Claude commits the *real* files from that sandbox. “Have Claude git pull”
  updates notes/README only.

- **2026-09-16 — commons-daemon: body-cap must-fix patched (`http.cpp`), presence
  bugs already fixed upstream.** Grok's review (`GROK_DAEMON_REVIEW.md`,
  `CLAUDE_DAEMON_PATCH.md`) flagged three must-fixes: (1) duplicate `join`
  double-counting `total_`, (2) `rekey` always decrementing `total_` even
  when `from_region` never held the id, (3) `Content-Length` uncapped past
  the 1 MiB header guard. Before patching, `git pull` surfaced that (1) and
  (2) were already fixed upstream by a newer id-index redesign of
  `presence.cpp`/`presence.hpp` (`92e379d`, `63792ea`: an id-lookup replaces
  the old scan-and-decrement pattern, `rekey`'s `from_region` is now
  advisory only since `join_locked` de-dupes globally regardless) — pulled
  that in rather than reapplying an older, now-superseded fix and clobbering
  it. Only (3) still needed work: `serve_one` now rejects `need < 0` or
  `need > 65536` with a 413 and closes the connection before the body-read
  loop, so a client-declared multi-GB `Content-Length` can never reach
  `body.append` on the 512 MB box. Verified live: duplicate join (same
  region and cross-region) collapses to one occupant; rekey from a region
  the id was never in leaves `global_count()` correct; a 5 GB, a negative,
  and a 65537-byte declared length each get a 413 and the daemon stays up
  and keeps serving normal requests; a full process restart still shows
  SQLite catalogs surviving and RAM presence clearing. No should-fix items
  touched (idle reap, server-issued ids, FetchContent, schema dedup, cookie,
  deploy) — those stay parked. Civic untouched, no SSH/systemd/nginx, no
  deploy.
- **2026-09-16 — Sibling: daemon written and tested locally, not deployed.**
  Brandon confirmed the scope explicitly before any droplet contact:
  write the daemon locally first, zero SSH/systemd/nginx this round.
  New `commons-daemon/` (C++, own CMake build, separate from
  `liquidgov-platform/`): a real SQLite ledger (vendored `sqlite3`
  amalgamation — WSL has the runtime `.so` but no `-dev` headers and no
  sudo to install them; downloaded the official amalgamation from
  sqlite.org with Brandon's explicit permission first, per the file-
  download rule) running `schema.sql`/`seed.sql` (the locked V1 catalog:
  3 lab recipes chalk-only, 12 outdoor regions, house/lab interior
  templates), RAM-only presence (`presence.md`'s shape — keyed by region,
  join/move/rekey/leave, 24/region and 96 global caps), and a minimal
  single-threaded synchronous HTTP server deliberately shaped like
  civic's own rather than a framework. Friends/cookie-forwarding and
  wall-slot fetch are documented (`glue.md`, `cookie.md`,
  `canvas-frame.md`) but **not wired** — no civic route exists to call
  and adding one means touching `http_server.cpp`, out of scope. Tile
  collision/movement stays client-side JS (`wasm.md`) — not duplicated
  here. Verified end-to-end, not just "it compiles": full presence
  lifecycle (join/move/rekey confirmed to leave no ghost/leave), per-
  region and global cap enforcement, a missing-field 400, and — the
  actual point of an integration test, not just a happy path — a real
  process restart: SQLite catalogs survived, RAM presence did not,
  proving the RAM/disk boundary `presence.md` describes rather than
  asserting it. `liquidgov-platform/`/`http_server.cpp`/civic untouched,
  no SSH, no systemd unit, no nginx change, nothing on the droplet.
- **2026-09-16 — Correction: `lg_session` does get `Secure`, conditionally.**
  My own first pass on the cookie (same session, bullet below) grepped
  only the literal `Set-Cookie:` string in `set_cookie()` and reported
  "no `Secure`" as flat fact — missed the very next line,
  `if (g_https) o += "; Secure";` (line 1112), and `clear_cookie()`'s
  matching one. `g_https` is set per-request (line 6824) from
  `X-Forwarded-Proto`, so on the real site behind nginx it's true for
  real traffic and the cookie the browser actually gets **does** carry
  `Secure`. Caught by Grok independently citing
  `deploy/ux-on-droplet.patch` and writing `cookie.md` concurrently — a
  real overlap this time, not just a doc-only commit alongside — merged
  by rechecking the source directly rather than taking either version on
  trust, confirming Grok's finding first-hand before writing it into
  `glue.md`. Lesson for next time reading this file cold: a `+= "..."` or
  a conditional two lines past the literal string you grepped for is easy
  to miss with a narrow grep; read the whole function, not just the match
  line. `glue.md`'s host-only/no-`Domain=` finding was independently
  correct and unaffected.
- **2026-09-16 — Glue: cookie and wall-slot are now verified facts.**
  Brandon delegated the choice; picked glue over sibling (order of
  operations — sibling needs glue's cookie name, glue needed nothing
  sibling has). Read the real `http_server.cpp` (WSL copy) and filled in
  `glue.md`'s TBDs with actual findings, no implementation:
  session cookie is `lg_session`, `HttpOnly; Path=/; SameSite=Lax;
  Max-Age=2592000`, no `Secure`, **no `Domain=`** — meaning it's host-only
  today, which resolves the "path vs subdomain" question as a fact rather
  than a preference: path needs zero civic change, subdomain needs one.
  Traced the real cookie→session→account→username chain
  (`account_from_req()`) that any future friends-query endpoint would
  reuse, not invent. Bigger find: the wall-slot's "profile-canvas bitmap"
  doesn't exist as such — the profile page CSS-animates through *frames*,
  and the real per-frame route, `GET /canvas-frame/<username>/<i>`, is
  already live and public today. Only real gap: no existing way to ask
  "how many frames / which is current" from outside — flagged as a small
  addition (e.g. a `/latest` alias), not a blocker. Checksummed the WSL
  copy against the live droplet before writing any of this down
  (`03fd46eb...`, matched exactly) rather than assume it was still
  current — read-only, no deploy, no edit to `http_server.cpp`. Sibling
  still waits on Brandon; nothing in `commons/index.html` changed this
  round.
- **2026-09-16 — Sprites patch: idle-skip sees facing now.** Fixed the
  must-fix from `GROK_SPRITES_REVIEW.md`: `updateFacingAnim` could change
  `player.dir` on a fully blocked step (hold a new direction into a
  wall — `movePlayer` returns `false`), but `frame()`'s idle-skip only
  checked position/toast, so the sprite kept showing the old facing cell
  until the next real step. Added `prevPlayerDir`/`prevPlayerAnim` and the
  same pair for passerby, tracked alongside position exactly like the
  existing prev-frame state, and included both diffs in `changed`. Idle
  skip itself is untouched — still skips the blit when truly nothing
  changed, per canvas.md; it just now correctly counts "started facing a
  different direction" as a change. Verified in-browser: walked the
  player against the solid Homestead wall, tapped a different direction
  into it (movement fully blocked, position unchanged), and confirmed
  both `player.dir` and `prevPlayerDir` updated together — proving the
  redraw branch actually ran, not just that the state variable changed.
  Full regression set still holds (A/D, water, spawn free, dual crop at
  390×844, no console errors). Glue and sibling still wait on Brandon.
- **2026-09-16 — Sprites shipped.** Brandon picked sprites first, lowest
  blast radius per Grok's own suggested order (glue needs a real cookie
  decision, sibling needs glue's cookie name, sprites needs neither).
  `commons/index.html`: player and passerby are no longer black squares.
  One procedurally-baked 16×24 sprite atlas (4 dirs × 2 walk frames, art.md)
  built once at init — same bake-then-blit shape as the tile bake, one
  `drawImage` per actor per frame (canvas.md: no per-frame `fillRect`, no
  save/restore, no `<img>` per direction). Player and passerby share the
  exact same atlas (locked: "online others: same sprite"). Facing follows
  input direction (updates even on a blocked step, so bumping a wall still
  turns you to face it); the walk-cycle leg-swap only advances on an
  actual successful move, resetting to a standing pose when idle or
  blocked. Renamed `passerby.dir` (its patrol ping-pong ±1) to `wpDir` to
  free up `.dir` for sprite facing — same field name, unrelated meaning,
  would have silently clobbered each other otherwise. Sprite atlas context
  is deliberately `{alpha:true}`, the one exception to canvas.md's
  `alpha:false` rule: that rule is for the opaque full-frame bake/display
  layers, and a sprite composited over the world needs real transparency
  or it blits as an opaque box. Verified in-browser: atlas cells visually
  distinct per direction (eyes down, none up, single dot left/right), walk
  frame toggles while moving and resets when stopped/blocked, facing
  updates correctly for all 4 directions, nameplate repositioned above the
  taller sprite's head, full regression set still holds (A/D, water/
  bridge, boarded-door toast, Hall confirm/open, spawn free, dual crop at
  390×844, no console errors). Still no glue, no sibling, no droplet
  contact. `liquidgov-platform/` untouched.
- **2026-09-16 — WebGPU textures crib.** `webgpu-textures.md`: textures
  cannot mapAsync; readback is copyTextureToBuffer. Pixel art uses
  nearest / textureLoad. Commons keeps Canvas 2D drawImage.
- **2026-09-16 — WebGPU mapAsync crib.** `webgpu-mapping.md`: staging
  copy, 8-byte align, buffer cannot be STORAGE and mapped. Why GPU
  collision loses to `solid[i]`. Still no shaders in commons/.
- **2026-09-16 — WGSL crib only.** `Persistence/TheCommons/wgsl.md` is
  syntax, not a renderer. `webgpu.md` still stands: no shaders in
  commons/. Next visible work still sprites / glue / sibling.
- **2026-09-16 — WebGPU is not this game.** `Persistence/TheCommons/webgpu.md`:
  240×160 blit does not want a compute shader. Canvas 2D stays. Sprites /
  glue / sibling still wait on Brandon.
- **2026-09-16 — WASM is not V1.** `Persistence/TheCommons/wasm.md`: no
  compile step in commons/. WASM only earns a slot if a C++ sibling
  daemon shares collision; blit/input stay JS. Sprites/glue/sibling still
  wait on Brandon.
- **2026-09-16 — nginx reverse proxy notes.** `Persistence/TheCommons/nginx.md`:
  static `alias /opt/commons/`; presence `proxy_pass` loopback; forward
  Cookie; buffering off; do not expose the game port or rewrite civic
  locations. Not a BUILD. Sibling still not this week.
- **2026-09-16 — two parked P3 nits closed.** Brandon said "P4" meaning
  "whatever's next," not literally invoking `sibling.md`'s deploy work --
  that doc itself says the sibling is "a new BUILD, not a P4," and "not
  this week," so held off on it rather than guess into a production
  change. Instead closed the two items already parked after P3 review, in
  `commons/index.html`: (1) the interact button now shows
  `nearestInteractable.prompt` (Try/Read/Look/Hall/Enter) instead of
  always saying "Enter" — falls back to "Enter" when nothing's in range
  (the interior-exit case). (2) Lookout's railing moved from y=10 to
  y=16 (plateau y=10-15): the landscape crop is only 10 tiles tall, so a
  camera centered on a player *at* the railing was topping out at y=15 and
  never actually reaching the Square (y=20) — confirmed by computing the
  visible tile range directly (was 9-19, now 11.5-21.5) and by screenshot,
  the Square's notice-post is now visibly in frame from the railing.
  Verified in-browser: both fixes work as intended, full regression set
  still holds (A/D, water, spawn free, no console errors). Still waiting
  on Brandon for a specific glue / sprites / sibling direction before
  starting any of those.
- **2026-09-16 — sibling mechanics.** `Persistence/TheCommons/sibling.md`:
  game daemon + nginx static `/commons/` next to `liquidgov_http` on the
  512 MB box. Presence RAM, SQLite WAL not rsync'd, friends via localhost
  cookie, civic crash-isolated. Path vs subdomain waits on a real
  Set-Cookie read. Not a BUILD. P3 still closed.
- **2026-09-16 — P3 closed.** c996a3d accepted: six quiet regions dressed
  per atlas (woods/lookout/orchard/workshop/garden/quarry). Shutter and
  quarry sign toast only. No new interiors. Civic untouched. Outdoor V1
  DoD met. Parked: interact-button caption; Lookout railing may sit too
  far north to see the Square in the landscape crop.
- **2026-09-16 — IndexedDB is a blob cache.** `Persistence/TheCommons/indexeddb.md`:
  later, tileset/house-snapshot/photo-staging. Not V1 inventory, not
  trades, not friends, not the 5.6 MB bake. Do not open IDB in commons/
  during P0–P3. P2 still closed. P3 waits.
- **2026-09-16 — P3 shipped.** `commons/index.html`: all 12 regions now
  dressed, matching `v1.md`'s full outdoor definition of done. North Woods
  (tree line on the world's north edge, scattered solitary trees, one
  cluster + sit-log), Lookout (a walkable plateau, a solid railing along
  its south edge facing the Square, a bench, one tree off the plateau),
  Orchard (six trees in loose rows), Workshop Lane (closed-shutter facade
  + bench + two crates — shutter interact toasts "Closed for now.", no
  interior, ever, without Brandon reopening that pin), Garden (flower-bed
  cluster + a low sitting wall), Quarry Ridge (boarded adit + sit-rock +
  a sign — only the **sign** is interactable, "Read" → "Closed.", the adit
  itself stays decorative, no ore, no interior, matches the locked pin).
  Same tileset throughout — new tile ids (tree, railing, plateau,
  flower-bed, low-wall, crate, adit) reused across regions per art.md,
  not one-off per-region art. No new doors, no new interiors, no state
  changes beyond two toasts. Verified in-browser: all six regions' set
  pieces render and collide correctly (adit/flowerbed/lowwall/crate/tree
  all solid, plateau walkable, railing solid), shutter and sign toast
  correctly, the adit itself confirmed *not* interactable, and the full
  P0-P2 regression set still holds (A/D, water/bridge, Homestead's
  boarded-door toast, Hall confirm/open, spawn still free, dual crop at
  390×844, no console errors). `liquidgov-platform/` untouched. This
  closes the outdoor world; P2's demo house/lab interiors are unaffected.
- **2026-09-16 — inventory is RAM.** `Persistence/TheCommons/inventory.md`:
  P2 `inventory = {}` vanishing on refresh is the spec, not a bug. Do not
  sessionStorage/localStorage/SQLite in this preview. Write-only object
  may toast a count later; placing items is glue. P2 still closed.
- **2026-09-16 — P2 closed.** e67d88e accepted: Lab Yard walk-in door,
  real 16×16 demo house (empty wall-slot on the wall), 20×16 lab with
  three lookups (chalk, no lime), session RAM inventory, `activeSpace`
  collision. Station interact wins over Enter-to-exit. Civic untouched.
  P3 (six quiet outdoor regions) waits on Brandon. Parked: interact
  button caption stays "Enter" until P3.
- **2026-09-16 — P2 shipped.** `commons/index.html` extended per
  `BUILD_ORDER.md`'s P2 list. Lab Yard dressed outdoors: a wall-tile
  building, a sign, and a Lab door that is a genuine **walk-in trigger**
  (per atlas.md, deliberately not the Enter-to-confirm family the other
  doors use — not solid, not in `interactables`). Demo house's P1 black
  stub replaced with a real 16×16 interior: table, two chairs, rug, and
  an empty wall-slot frame embedded in the wall itself (still empty —
  no glue, per `glue.md`). New 20×16 Lab interior with a workbench visual
  and three lookup stations (copper acetate, iron-gall ink, chalk-only —
  pushback.md's fix, no lime), each showing the real recipe name in a
  toast and recording one pickup in a plain in-memory `inventory` object
  (v1.md: session RAM, vanishes on refresh, no SQLite in this preview).
  Collision/movement/rendering generalized behind a single `activeSpace`
  (outdoor / demoHouse / lab) rather than copy-pasting the outdoor logic
  twice — same 12×12-box/4-corner/axis-split rules from collision.md
  apply unchanged inside either interior, only which tile/solid array and
  pixel bounds count as "the world" changes. Demo house keeps its P1
  Enter-to-enter/Enter-to-exit pattern exactly as shipped; the Lab is
  walk-in both ways, with Enter also working as a fallback exit. Real bug
  caught by testing, not review: `tryInteract()` originally checked
  `mode === "lab"` before checking `nearestInteractable`, so standing
  right next to a lab station and pressing Enter always exited the lab
  instead of triggering the station — no toast ever fired, at any
  distance. Fixed by checking the interactable first; the interior-exit
  fallback only applies when nothing is in range. Also fixed on sight
  (not asked, but the same class of bug as the P0 fountain trap): the
  wall-slot frame was placed one tile off the actual wall, floating on
  the open floor instead of embedded in it. Verified in-browser: Lab door
  auto-warps in and back out (both via walking onto the interior door and
  via the Enter fallback, landing at the intended spot outside each time,
  no oscillation), demo house enter/exit round-trips to the exact saved
  outdoor position, both entry tiles confirmed unblocked
  (`boxBlockedAt` false), all three lab stations produce the correct
  recipe text and inventory entry, and the full P0/P1 regression set
  still holds (A/D not inverted, water solid off the path, bridge still
  crosses it, boarded-door toast, Hall's `confirm()`/`window.open()`
  stubbed and verified, dual crop switches at 390×844, no console
  errors). North Woods, Lookout, Orchard, Workshop Lane, Garden, Quarry
  Ridge remain untouched — P3, not this round. `liquidgov-platform/`
  untouched.
- **2026-09-16 — dt clamp notes.** `Persistence/TheCommons/dt.md`: keep
  1/20; that is what stops tab-return tunneling through 1-tile doors.
  Do not add CCD to satisfy a dt=1 test. P1 still closed. P2 waits.
- **2026-09-16 — P1 closed.** Patch 9048aac accepted: notice-post not
  fountain, passerby visible in the Square (hide seam returns 0), toast
  off-transition redraws, doors solid, benches on the ribbon, waypoints
  on path/bridge, spawn assertion. Civic untouched. P2 waits on Brandon.
- **2026-09-16 — P1 patch shipped.** All items from
  `CLAUDE_P1_PATCH.md`/`GROK_P1_REVIEW.md`/`collision.md` fixed in
  `commons/index.html`: (1) `T_FOUNTAIN` removed entirely, replaced with
  `T_NOTICE_POST` (dark ink-brown, distinct from Hall's brass), still
  offset off the spawn tile. (2) Passerby visibility rewritten: hide is no
  longer "shares a region with the local player" (that hid it at spawn,
  the exact waiting-room look it exists to prevent) but "a second
  non-ambient occupant is in its region" via a named
  `otherRealOccupantsInRegion()` seam that always returns 0 today (no
  multiplayer transport in this preview) — passerby now stays visible at
  spawn, nameplate intact, confirmed visually with player and passerby
  both on-screen in the Square together. (3) Toast off-transition fixed:
  `prevToastActive` tracked alongside the other prev-frame state, so the
  exact frame where `toastActive` flips false forces a redraw instead of
  leaving "Closed." on the bitmap until something else happens to move.
  Should-fix: boarded/demo/Hall doors now all solid (interact still fires
  from an adjacent tile, Chebyshev ≤ 1, doors don't auto-warp on overlap);
  Creek/Shore benches moved off open water onto tiles adjacent to the
  bridge; passerby waypoints recomputed to sit exactly on the carved
  path/bridge tiles (the old last waypoint was plain Shore water, off the
  ribbon). Added a startup assertion (`assertSpawnIsFree`) that
  console.errors if a future prop ever lands back on the spawn tile,
  per `collision.md`'s explicit invariant. Verified in-browser: spawn is
  provably unblocked (`boxBlockedAt` false) and movable in all four
  directions, all three door types solid under realistic per-frame `dt`
  (confirmed tunneling only happens with an unrealistic `dt=1.0` test
  value, never with the game's real `1/20` clamp), water still solid off
  the path, bridge still crosses it, both benches have a walkable adjacent
  tile, dual crop still switches at 390×844, no console errors. No P2.
  `liquidgov-platform/` untouched.
- **2026-09-16 — collision notes.** `Persistence/TheCommons/collision.md`:
  keep 12×12 AABB + axis split; doors/notice-post solid; interact from
  adjacent; never spawn overlapping solid; passerby does not collide, so
  waypoints stay on the ribbon. For the open P1 patch, not a new phase.
- **2026-09-16 — Grok P1 review.** Spine accepted (Hall, 7 doors, bake
  labels, idle skip, hidden watchdog). Must-fix: notice-post not fountain;
  passerby must stay visible in the Square (hide only for a second real
  occupant); toast off-transition must redraw. Should-fix: boarded doors
  solid, benches on the path, passerby stays on the ribbon. Paste
  `CLAUDE_P1_PATCH.md`. P2 not started. Civic frozen.
- **2026-09-16 — P1 shipped.** `commons/index.html` extended per
  `BUILD_ORDER.md`'s spine list: the Square (fountain + 2 benches),
  Homestead Row (facade with 7 doors: door 0 = demo, doors 1-6 = boarded —
  no cookie glue, per `glue.md`), Creek/Shore (bench + recolored
  path-over-water as a "bridge" tile, cosmetic only), Hall Lawn (door +
  2 hedges, opens `liquidgov.us/proposals` in a new tab after a
  `confirm()`). North Woods/Lookout/Orchard/Workshop Lane/Lab Yard/Garden/
  Quarry Ridge untouched — P2/P3, not this round. Passerby implemented per
  the round-2 lock: nameplate exactly `passerby`, `isAmbient` flag
  internal, patrols Square→Homestead→Creek→Shore and back, hides whenever
  its current region (computed from AABB, never stored per-tile) matches
  the player's. One prompt at doors (`Enter`/tap a new on-screen button):
  Hall confirms+opens, the demo door stubs to a black placeholder interior
  ("[Enter] to step back outside"), a boarded door shows a transient
  "Closed." toast. `canvas.md`'s three P1 asks done: region labels are now
  baked into the offscreen canvas once (no more per-frame `fillText`),
  frame() skips the actual blit when nothing changed (player/passerby
  didn't move, crop unchanged, no toast) though rAF keeps ticking, and the
  P0-parked nit is fixed — the watchdog no-ops entirely while
  `document.hidden`, tearing down an already-running fallback too.
  Real bug caught by testing, not review: the fountain's placement formula
  put it on the exact same tile as the player's spawn point, so a fresh
  arrival's own bounding box already overlapped a "solid" tile and every
  direction of movement was silently rejected. Fixed by offsetting the
  fountain 3 tiles south of center. Verified in-browser: A/D still not
  inverted, water still solid off the path, the path/bridge still crosses
  it, dual crop still switches at 390×844, `document.hidden` watchdog
  guard confirmed both for "don't start while hidden" and "tear down an
  active fallback when hidden", demo door enter/exit round-trips, boarded
  door toasts without changing mode, Hall's `confirm()`/`window.open()`
  call verified with both accept and decline (stubbed, so production
  liquidgov.us was never actually opened during the test). No console
  errors. `liquidgov-platform/` untouched.
- **2026-09-16 — canvas 2D notes.** `Persistence/TheCommons/canvas.md`:
  P0 blit is the big win. P1: bake static labels, skip idle frames, one
  sprite atlas, no fillText-per-frame, watchdog respects document.hidden.
  Not a patch to commons/ yet. P1 still waits on Brandon.
- **2026-09-16 — P0 closed.** Patch 0957725 accepted: name field types
  "Brandon", rAF is the loop, interval only as watchdog, alpha:false,
  d-pad tabindex=-1, input.blur() on enter. Civic untouched. P1 not
  started. Parked (not a new patch): watchdog should no-op while
  `document.hidden` so a background tab doesn't start the 60Hz fallback.
  Next work waits on Brandon saying P1 / BUILD.
- **2026-09-16 — P0 patch shipped.** All three items from
  `CLAUDE_P0_PATCH.md`/`GROK_P0_REVIEW.md` fixed in `commons/index.html`:
  (1) keydown/keyup now ignore entirely (no `preventDefault`, no `keys[]`
  update) while `#nameOverlay` is visible or `document.activeElement`/
  `e.target` is an INPUT/TEXTAREA — typed "Brandon" via real keyboard
  input end-to-end and confirmed `nameInput.value === "Brandon"`, then
  confirmed movement worked immediately after submit (added `input.blur()`
  in `enter()` so the now-hidden input doesn't keep eating keys via stale
  focus). (2) `requestAnimationFrame` is the primary loop again
  (`rafLoop`); a `watchdogTick` every 300ms only starts a `setInterval`
  fallback if `lastTickAt` has gone stale, and tears it down if rAF
  resumes — confirmed rAF firing normally in this session's browser pane
  this time (72 callbacks/500ms measured directly) with `usingFallback`
  correctly false throughout. (3) `{alpha:false}` added on both the bake
  and display 2D contexts, `imageSmoothingEnabled=false` added on the bake
  context too (was only on display before), `tabindex="-1"` added to all
  four d-pad buttons. Re-verified after patching: A/D still not inverted,
  dual crop still switches at 390×844, water collision still blocks
  off-path movement, no console errors. Did not touch checker grass,
  Creek/Shore flood-fill, or start P1, per explicit instruction.
  `liquidgov-platform/` untouched.
- **2026-09-16 — Grok P0 review.** Skeleton accepted (bake/blit, dual crop,
  A/D, path ribbon, labels, civic untouched). Must-fix: name input vs WASD
  `preventDefault` ("Brandon" untypeable). Should-fix: rAF as the product
  loop, setInterval only as rAF-fallback; `alpha:false`. P1 not started.
  Paste `CLAUDE_P0_PATCH.md`. See `GROK_P0_REVIEW.md`.
- **2026-09-16 — blit pipeline.** `tilemap.md` expanded: display canvas
  is the logical crop only (not the 1152×1280 bake in the DOM). Floor
  camera, smoothing off, alpha:false, ignore DPR, no worker. Giant-canvas
  + CSS-translate is the fail.
- **2026-09-16 — P0 shipped.** `commons/index.html`: vanilla HTML/canvas,
  one file, no build step. Uint8Array(72×80) tile+collision, baked once to
  an offscreen canvas per `tilemap.md`, blitted per tick — no per-frame
  72×80 loop. Dual crop (240×160 @3× landscape / 160×240 @2× portrait,
  picked by `width >= height`) confirmed switching correctly at a real
  390×844 phone size. A/D and arrows confirmed not inverted (moved player
  in the correct direction, tested via dispatched key events, not just
  read from code). On-screen d-pad confirmed via real pointer events on
  the button elements. Water (Creek/Shore) confirmed solid; the
  Square→Homestead→Creek→Shore path ribbon confirmed walkable straight
  through that same water band. World edge confirmed blocking. All 12
  region names confirmed present and in the correct grid order as small
  world-space ink labels — no HUD chrome. No fountain, no other set piece,
  no doors, no passerby, no SQLite, no cookie, per P0 scope. One
  implementation deviation from `tilemap.md`'s default suggestion: the
  tick loop uses `setInterval`, not `requestAnimationFrame` — a bare rAF
  counter measured 0 callbacks over 500ms in the tool's own browser pane
  even fully visible/focused, so rAF isn't reliable in every environment
  this needs to run in; `setInterval` has no such dependency and the
  movement math was already delta-time based, not frame-count based, so
  nothing else changes. `liquidgov-platform/`, `liquidgov-core/`, LGP1
  untouched. Grok reviews next.
- **2026-09-16 — tilemap notes for P0.** `Persistence/TheCommons/tilemap.md`:
  one Uint8Array 72×80, bake supermap once, blit camera crop. No chunks,
  no WebGL, no 12 canvases. Claude should follow this in `commons/`.
- **2026-09-16 — P0 BUILD assigned to Claude.** Brandon: Grok guides /
  criticizes; Claude implements to save Grok tokens. Paste `CLAUDE_P0.md`.
  Sibling folder `commons/` (vanilla HTML/canvas), not `liquidgov_http`,
  not React. P0 only. Grok reviews after the commit. Civic frozen.
- **2026-09-16 — first BUILD = P0 only.** Brandon chose P0, not P0+P1.
  Empty walkable 72×80, dual crop, d-pad, A/D = left/right, throwaway name,
  spawn Square, region labels, no set pieces, no doors, no passerby.
  P1+ wait for a later BUILD. Still no code until Brandon says BUILD.
- **2026-09-16 — Grok build order.** Answer in
  `Persistence/TheCommons/BUILD_ORDER.md`. Greater map = empty walkable
  72×80 first (one array, not 12 maps). Chunk = a **system**, then a
  **spine** of adjacent regions, not one region. Dual crop + A/D + d-pad
  de-risked in P0. Passerby in P1 after the spine walks, before interiors.
  No V1 SQLite/P2P/glue. Recommended first BUILD: **P0+P1**. P0-only is
  the smaller phone-check slice. Spec not reopened. Still waiting on
  Brandon for BUILD.
- **2026-09-16 — Question posed to Grok: build order.** Spec pack fully
  locked, no open bets. Brandon wants the actual build scoped top-down:
  greater map first, then chunks, then smaller shippable pieces, so BUILD
  doesn't turn into one big-bang implementation. Asked Grok directly (he's
  the one building the client) in
  `Persistence/TheCommons/BUILD_ORDER_QUESTION.md` — what order he'd
  actually build in, what a "chunk" means in his build, where presence/
  `passerby` slots in, and what (if anything) he'd de-risk first. Not a new
  pushback round; spec isn't reopened. Still waiting on Brandon for BUILD.
- **2026-09-16 — Grok accepts passerby.** Ambient walker is locked as a
  **V1-only crutch**: nameplate exactly `passerby` (no unlabeled variant),
  `is_ambient` internally, hide while a real guest is in that region,
  Square → Homestead → Creek → Shore, trivial to delete later. Claude's
  round-2 rewrite stands. No remaining open bets. No game code. Civic
  frozen. BUILD still waits for Brandon.
- **2026-09-16 — Claude pushback round 2, ambient walker.** Grok's four spec
  patches (Square/Lab Yard neighbor fix, camera-overlap presence subscription,
  cookie-scoped friends query, `players` last-position columns) are correct,
  adopted, no further pushback. On Grok's new bet (an ambient walker so a
  solo preview isn't an empty Square): **rewrite, not accept-as-is** — ship
  only the nameplate-reading-"passerby" variant, reject the no-nameplate
  variant, because a real occupant always carries a name label
  (`art.md`) and an unlabeled look-alike would be indistinguishable from an
  actual second visitor — the opposite of this project's honesty pattern
  (boarded mine, closed shutter). Written up in `pushback.md` (Round 2) and
  `v1.md` (Pending, not yet locked). Not locked — awaiting Brandon/Grok to
  close it out. No game code. Civic frozen. BUILD still waits for Brandon.
- **2026-09-16 — Grok accepts Claude’s three overrides.** Now locked as
  implementation: regions **24×20** (supermap 72×80); camera **dual crop**
  240×160 @3× landscape / 160×240 @2× portrait (pick by viewport aspect);
  lab recipe #3 **chalk only, no lime**. Grok also patched (see
  `Persistence/TheCommons/GROK_ANSWER.md`): Square south = Homestead Row
  (atlas typo said Lab Yard); presence clients subscribe to
  camera-overlapping regions; friends query is cookie-scoped (no client
  username param); `players` last-position columns. Ambient walker on
  Square→Shore is a Grok *bet*, not locked. No game code. Civic frozen.
  BUILD still waits for Brandon.
- **2026-09-16 — Claude spec pack landed.** Wrote all seven files in
  `Persistence/TheCommons/`: `atlas.md`, `schema.sql`, `presence.md`,
  `art.md`, `glue.md`, `v1.md`, `pushback.md`. No lock changed — everything
  pinned in this heading (title, region names/order, 3×4 layout, Homestead
  gate, Hall/mine V1 scope) is unchanged. Two of Grok's *unlocked*
  implementation bets in `GROK_BET.md` are overridden in `pushback.md`, not
  silently: region grid 32×32→**24×20** (Grok's own bet flagged this risk
  and offered this exact fallback), and camera flat 240×160→**dual
  landscape/portrait crop** (240×160 desktop, 160×240 phone) so the phone
  case isn't letterboxed/non-integer-scaled. Lab recipe #3 also rewritten
  (drop "lime," keep only "chalk," per the safety-boundary reasoning in
  `pushback.md`) — the other two lookups, the palette, the Homestead cap,
  the Hall/mine V1 scope, and A/D=left/right are all accepted as Grok bet
  them. Ready for Grok to read `pushback.md` and answer.
- **2026-09-16 — Claude spec pack (no game code, civic frozen).** Paste
  `CLAUDE_THE_COMMONS.md` into Claude Code (not `CLAUDE_CODE_CONTINUE.md`).
  He fills `Persistence/TheCommons/` (atlas, schema, presence, art, glue, v1,
  pushback) from Grok bets in `GROK_BET.md`. V1 client stays the Grok preview
  after Brandon says BUILD. Droplet sibling later. Do not touch
  `liquidgov_http` / LGP1.
- **2026-09-16 — locked.** A + continuous outdoor + warp interiors + homestead
  gate. Title **The Commons**. Plaza region **the Square** (not “Commons”).
  Art: GBA / FireRed tiles. No code until Brandon says BUILD.
  - Outdoor 3×4, north up:
    North Woods | Lookout | Orchard
    Workshop Lane | **the Square** | Hall Lawn
    Lab Yard | Homestead Row | Garden
    Quarry Ridge | Creek | Shore
  - Interiors extra: per-player house, lab, hall (thin), mine (boarded / not present).
  - Homestead Row = gate (online / friends doors). Houses are instances, not 12 lots.
  - V1: all 12 lightly dressed; mine boarded; no workshop, crafting, farming,
    swimming, or XP. Lab output is placeable / wall-able, not “salt in a bag.”
  - **V1 runs in the Grok preview app.** Droplet sibling + SQLite later.
    `liquidgov_http` is never the game.
  - Presence: per-region at ~2 Hz even on one canvas. Droplet is 512 MB / 1 vCPU;
    do not flood.
  - Guests walk the 12; house binds only after LiquidGov cookie; no second
    password. Preview may use a throwaway name until glue exists.
  - Profile canvas from the site = one **wall slot**, not the floor.
  - Friends = civic **query**, short RAM TTL, never a second graph.
    Visit: open / friends / locked.
  - Photo → civic blob + feed only. Game does not grow posts, comments, or follows.
  - Touch: on-screen d-pad; A/D = left/right on screen.
- **2026-09-16 — parked, atlas not locked.** (superseded by lock above)
  Working title The Commons (not confirmed). Art lean GBA / FireRed cousin
  (not confirmed). Nothing built. Do not scaffold, sprite, or write game code
  until Brandon says BUILD. Do not start the game in the civic repo.
- **V1 proposed, not locked:** (superseded) walkable public ground, your house,
  lab, Hall door → live `liquidgov.us/proposals`, mine boarded. Guest play
  until cookie glue exists.
- **Atlas not locked.** (superseded) Brandon wants one greater map of about 12
  minor maps or instances. Options A/B and warp vs continuous were the choice;
  lock is A + continuous.
- **2026-09-17 — cookie resolver built, local-tested, not deployed.** Brandon
  said "build" in reply to the offer naming this exact increment; treated as
  BUILD + cookie resolver per `cookie-bind.md`'s standing gate. First-ever
  edit to `http_server.cpp`: one new same-box-only route,
  `GET /internal/session-user`, ~17 lines, reuses `account_from_req()`
  as-is, refuses (404) anything that arrives via nginx (X-Forwarded-For
  present) or from a non-loopback peer. Daemon side: new `civic.cpp`
  resolver (skips the network call unless the raw Cookie header actually
  contains `lg_session=`, 1s timeout, every failure path falls back to
  guest) plus one `effective_id()` helper wired into all five routes that
  used to check only a guest secret (`join`/`move`/`rekey`/`leave`/
  `placed` POST). A resolved cookie now derives `"u:"+username` server-side
  and the client's own id/secret/display_name are not consulted for
  identity at all. Full local test (scratch civic store + scratch daemon
  DB): a real civic session cookie forwarded with intentionally garbage
  client-submitted id/secret bound correctly as `u:boundtest` on join,
  moved, and left; guest register/join/move regression unaffected. See
  `Persistence/TheCommons/cookie-resolver-built.md` for the full
  before/after and what's explicitly still not built (deploy; mid-session
  promotion/rebind, `cookie-bind.md` left that decision open; the
  restart-snapshot hook for bound players). No nginx, no systemd, no
  droplet touched this round.
- **2026-09-17 — cookie resolver must-fix: mid-session login ghost,
  fixed, still not deployed.** Grok review (`GROK_COOKIE_RESOLVER_REVIEW.md`)
  caught that the prior round's `effective_id()` was cookie-wins on
  every route, which meant a guest who logs in without leaving got a
  `move` rejected (bound id not in the region yet) and, worse, a
  `rekey` that inserted the bound row into the new region without ever
  erasing the old `g_...` row -- a ghost sprite until the 30s idle
  reap. Fixed daemon-only: `PresenceStore` gained `forget(id)` (global
  erase by id) and `contains(region, id)`; `main.cpp`'s `effective_id()`
  split into `bind_or_promote()` for join/rekey (retires the old guest
  row when a verified one exists, before inserting the bound row) and
  `effective_id_in_region()` for move/leave (only switches to the bound
  id once it's actually landed in that region, otherwise keeps using
  the still-live guest row so an ordinary walk never 403s). Verified
  locally: guest walks, logs in mid-session, keeps moving under the
  guest id in the same room, then rekeys and the old room ends up
  empty with no ghost while the new room shows exactly one bound row;
  both prior regressions (already-bound-from-start, no-cookie-bad-secret
  still 403s) still pass. See `cookie-resolver-built.md`'s own
  "Must-fix" section. Still holding on deploy for Brandon's own
  **deploy** word.
- **2026-09-17 — cookie resolver deployed, live.** Brandon said
  "deploy." Rebuilt and restarted both civic (`/opt/liquidgov`) and the
  daemon (`/opt/commons-daemon`, reconfigured for the new `civic.cpp`
  source) on the droplet; no nginx change. Drift-checked both source
  trees against the droplet first (sha256 + diff) -- clean, only this
  project's own commits were live there. Verified in production: public
  `curl https://liquidgov.us/internal/session-user` -> `404` plain text
  (blocked exactly like `/logs`'s own non-operator refusal); a real
  throwaway account, logged in via a real browser, walked into the
  Square with a decoy name typed into the guest-name modal and both the
  occupant feed and the on-screen self-nameplate showed the real
  account name instead. Both services' logs clean post-restart. See
  `cookie-resolver-built.md`'s own "Deployed" section.
- **2026-09-17 — cookie bind arc closed out.** Grok independently
  confirmed the live deploy with no SSH access (`GROK_BIND_LIVE.md`):
  public `/internal/session-user` 404s, `/commons/` still the Square,
  `GET /commons/presence/1,1` empty (the verification occupant had
  already idle-reaped), civic still serving `/proposals`. "No further
  deploy this hour." The one real production side-effect from
  verification -- a throwaway account, `claudeverify17`, created to
  prove the nameplate shows a real account name -- was removed the
  right way: logged in as it, used the site's own `/settings/delete`
  self-service flow (typed the username to confirm), then confirmed a
  login attempt afterward fails with "Invalid username or password."
  No filesystem or LGP1 touch. Cookie bind's whole arc (reachability,
  resolution, the mid-session ghost fix, deploy, independent
  confirmation) is now fully closed on every side. Known remaining
  gaps, not proposed as work: in-place `rebind()` vs. leave+join
  (`cookie-bind.md` never decided), snapshot-on-leave for bound
  players, and a possible `lg_session` -> username cache if the 2 Hz
  loopback call ever actually hitches the daemon's accept loop under
  real load (not a concern at Square-scale today).
- **2026-09-17 — "your house" built, local-tested, not deployed.**
  Brandon said "build" for `GROK_V4_ANSWER.md`'s named increment: bound
  players were real but still lived in the shared demo house, the
  actual next "presence wasted" hole. Safe thread, civic untouched.
  Daemon: `/presence/join`/`/rekey` now echo `"bound":true/false`
  alongside `display_name`. Client: one new Homestead door
  (`T_DOOR_HOUSE`, physically separate from the six still-boarded
  friend-lot doors), a new `mode: "house"` reusing `demoHouseSpace`'s
  geometry under a per-username instance key
  (`instance:house:<username>`), gated on the new `bound` flag -- a
  guest gets a toast, a bound player gets their own isolated room with
  a working wall-slot. Verified backend via curl (bound flag correct
  both ways, two different bound users' houses don't cross-contaminate)
  and client logic via a local static server pointed at a scratch
  daemon (door tile distinct and reachable, guest/bound branches both
  correct, network tick rekeys into the right instance, exit and
  cross-instance placed-item isolation both clean). Snapshot-on-leave
  turned out to be a real separate unit of work once scoped (no
  `players`/`map_instances` rows exist anywhere yet) -- deferred, not
  built this round. See `Persistence/TheCommons/your-house-built.md`.
  Holding on deploy.
- **2026-09-17 — your-house must-fix: house key used the wrong field,
  fixed, still not deployed.** Grok review (`GROK_HOUSE_REVIEW.md`)
  caught that the house key was built from `display_name`, which is
  the *disambiguated Square nameplate* -- a guest who types "brandon"
  before the real `brandon` binds turns the bound player's own
  display_name into "brandon (2)", a space-and-parens string that
  would key a wrong, different house room. Fixed daemon-only for the
  field, plus one client-side hardening pass: `main.cpp`'s join/rekey
  now also echo a raw `"username"` field (only when bound, never
  touched by disambiguation); the client builds
  `INTERIOR_INSTANCE_KEY.house` from that, never `display_name`, and
  `isBound` is now set from every response instead of latching true
  once. Also added `encodeURIComponent` on the four client fetches that
  put a region/instance key in a URL path -- which surfaced a real
  latent bug while implementing it: `commons-daemon`'s `http.cpp` never
  url-decoded `req.path` at all, so encoding client-side without a
  matching daemon-side decode would have broken every outdoor poll
  (`1,1` -> `1%2C1`, no longer matching `PresenceStore`'s literal
  keys). Fixed with a one-line decode in `http.cpp`, symmetric with the
  new client encoding. Verified locally reproducing the exact
  collision scenario (guest claims "boundname" first, real account
  `boundname` binds after) -- `username` stays clean, the house key is
  correct, the wrong suffixed key is confirmed empty; separately
  verified through the real client that outdoor polling still resolves
  real occupant data through the new encode/decode round trip. See
  `your-house-built.md`'s own "Must-fix" section. Holding on deploy.
- **2026-09-17 — your house deployed, live.** Brandon said "deploy"
  (client + daemon only). Drift-checked `main.cpp`/`http.cpp` against
  the droplet first -- clean, only this round's own changes. Synced,
  rebuilt, restarted `commons-daemon` only; `liquidgov.service` never
  touched (uptime confirmed unchanged). Verified in production: the
  exact disambiguation collision reproduced for real (guest claims a
  name first, the real bound account joins after) -- `username` stays
  clean, the house key is correct, the wrong suffixed key is empty. A
  real browser with a real logged-in account walked through the new
  door with a decoy name typed into the guest modal; the network log
  showed the client's own `encodeURIComponent`'d request
  (`instance%3Ahouse%3A...`) returning `200`, proving the encode/decode
  round trip works through the whole real stack, not just locally. The
  self-nameplate showed the real account name, the wall-slot worked
  inside the house too. Both services' logs clean. Cleaned up the one
  throwaway test account via its own `/settings/delete`. **Process
  lesson, caught after the fact by `GROK_HOUSE_KEY_REVIEW.md`:** synced
  client before finishing the daemon rebuild+restart, a narrow window
  where the new client's `encodeURIComponent`'d requests would have hit
  the still-old, non-decoding daemon and gone empty. No evidence any
  visitor actually hit it, but the correct order (verified by the
  review, not by me beforehand) is daemon rebuild+restart fully first,
  client sync second, whenever a change pairs client encoding with
  server decoding. See `your-house-built.md`'s own "Deployed" section.
- **2026-09-17 — your house arc closed out.** Grok independently
  confirmed the live deploy with no SSH access (`GROK_HOUSE_LIVE.md`):
  `/commons/` still the Square, `GET /commons/presence/1,1` empty JSON
  (proving the decode is a correct no-op on a literal comma, not just
  harmless on encoded input), civic still serving `/proposals`. Could
  not independently bind a throwaway account itself (house door is
  logged-in-only, Grok has no browser/SSH), so it took the production
  collision result (nameplate suffixed, house key clean) as reported.
  Accepted the sequencing self-correction from the deploy note as the
  standing rule for next time. "No further deploy this hour." Cookie
  bind, the mid-session ghost fix, and your house (including its own
  must-fix) are now all live and independently confirmed end to end.
- **2026-09-17 — persist the bound house built, local-tested, not
  deployed.** Brandon said "build" for `GROK_V5_ANSWER.md`'s named
  increment: the house door and bound identity were real, but
  `PresenceStore`/`PlacedStore` are RAM, so a daemon restart emptied
  the house and dropped the chalk. Safe thread, civic untouched. New
  `house.hpp`/`house.cpp` (`HouseStore`) is the first code anywhere in
  the project to actually write `schema.sql`'s `players`,
  `map_instances`, and `placed` tables -- find-or-create a `players`
  row and a `map_instances` house row on first bound entry, snapshot
  last position onto that row on `/presence/leave`, and a real
  `placed` row (not the RAM `PlacedStore`) for the wall slot, scoped
  by that instance only. Join/rekey into a house now also runs an
  ownership check -- a guest or a *different* bound player is refused
  outright (`403 not_your_house`) -- since there's real durable data to
  protect now, not just an ephemeral RAM room. Demo and lab are
  completely unchanged, still shared RAM. Client repositions on
  `resume_x/y/facing` when the daemon sends them, guarded on still
  being in `mode: "house"` when the (async) response lands. **Tested
  by actually killing and restarting the daemon mid-test** -- the
  literal bug this build exists to fix -- and confirming the wall-slot
  item survived while presence correctly did not; re-entering resumed
  at the exact last-left position; ownership check holds against both
  a second bound account and a plain guest. See
  `persist-house-built.md`. Only the leave half of "leave / ~30s"
  snapshot-on-leave is wired -- the periodic half needs an actual timer
  loop, flagged as its own separate addition, not attempted. Holding
  on deploy.
- **2026-09-17 — persist-house must-fix: walking out the door never
  snapshotted, fixed, still not deployed.** Grok review
  (`GROK_PERSIST_REVIEW.md`) caught that the prior round's test proved
  the mechanism (a direct `/presence/leave` call) but not the real
  path: `exitToOutdoor()` never calls `netLeave()`, walking out the
  house door is an ordinary rekey, so snapshot-on-leave alone would
  only ever fire on an actual tab close, never on normal walk-out-and-
  come-back-later play. Fixed daemon-only: `/presence/rekey` now also
  snapshots when `from_region` is the caller's own house, captured
  before the rekey erases that occupant row -- same mechanism `leave`
  already used, now on the path that actually matters. Also wrapped
  `house_instance_for`'s two-row create (`map_instances` INSERT +
  `players` UPDATE) in `BEGIN`/`COMMIT` per `schema.sql`'s own stated
  rule, closing a crash-window that could have orphaned a house
  instance and stranded its wall-slot item on a second, unreachable
  copy. Verified locally reproducing the real pattern: entered, placed
  an item, walked out via rekey (no leave call at all), killed and
  restarted the daemon -- resumed at the exact walk-out tile, item
  intact; `leave`'s own snapshot path re-confirmed unaffected. See
  `persist-house-built.md`'s own "Must-fix" section. Holding on
  deploy.
- **2026-09-17 — persist house deployed, live.** Brandon said "deploy"
  (daemon first -- new `house.cpp` -- then client, no civic, no
  nginx). Drift-checked first, clean. Verified with a real throwaway
  account through the real client, not curl: entered its house, placed
  an item, walked to a distinctive tile, waited for the real move-sync
  tick, then triggered the door's actual fallback exit and let the
  client's own automatic tick fire the rekey -- no manual daemon call.
  House emptied correctly, item untouched. Re-entering through the
  real door resumed at the exact walk-out tile and facing, over a real
  cross-origin cookie round trip. Did not force a second production
  restart just to re-prove survival -- the identical binary already
  proved that locally; a real live occupant (`u:Bbottin`) was on the
  box during this deploy and a gratuitous second restart would only
  have interrupted them for no new information. Cleaned up the
  throwaway account via its own `/settings/delete` -- its game-side
  rows are harmless orphans in the daemon's own DB, untouched by civic
  account deletion by design. See `persist-house-built.md`'s own
  "Deployed" section.
- **2026-09-18 — persist-house arc closed out.** Grok independently
  confirmed the live deploy with no SSH (`GROK_PERSIST_LIVE.md`): Square
  up, `GET /commons/presence/1,1` empty JSON, nginx still serving
  `/commons/`. Walk-out -> rekey -> resume is only provable logged in, so
  it took the client-driven production sequence as reported. Also
  settled: leftover `players`/`placed` rows after a throwaway civic
  delete are orphans by design, not civic coupling -- never open LGP1 to
  clean them; a later round can reap unused house rows if they ever
  matter. Environment note: WSL briefly failed on the host (`HCS
  0x8007005a`, insufficient system resources) between the deploy and
  this closeout; cleared on retry, no repo or droplet state affected.
  Cookie bind, the ghost fix, your house, and persist-house are now all
  live and independently confirmed.
- **2026-09-18 — friends query built, local-tested, not deployed.**
  Brandon said "BUILD friends query" (`GROK_V6_ANSWER.md`). Second civic
  edit: one same-box-only, read-only route `GET /internal/friends` (same
  gate as `/internal/session-user`; identity is the session cookie only,
  no username parameter; mutual accepts minus blocks either way).
  Daemon: shared loopback fetch in `civic.cpp`, a 15s RAM-only
  `FriendsCache` (`friends.cpp`), `PresenceStore::bound_usernames()`, and
  `GET /presence/lots` -- mutual friends who are in the game, sorted, six
  max, for Homestead lots 2-7. It lives under `/presence/` so the existing
  nginx location covers it: no nginx change. Client: the six boarded
  doors become `friendDoor`s; a bound player near the facade polls every
  5s and a door with a friend draws lit with their name. **Visiting is
  deliberately not built** (waits on this per Grok, and no visit-policy
  data exists): a named door says visiting isn't open yet, and the
  daemon's `not_your_house` check is untouched. Tested with four real
  civic accounts (accepted, never-accepted, blocked), the public-404
  gate, a `?username=` probe, and a real browser. The browser check
  caught a load-time crash (`FRIEND_LOT_DOORS` declared after the world
  is built) that reading the diff would not have. Unlike the last three
  rounds, deploying this touches civic (rebuild + restart `liquidgov`).
  See `friends-query-built.md`. Holding on deploy.
- **2026-09-18 — friends query deployed, live.** Brandon said "deploy"
  (civic, then daemon, then client; no nginx). Drift-checked first --
  civic was 25 added lines, zero removed. Civic rebuilt and restarted
  first: public `/internal/friends` 404s, same-box no-cookie is `{}`,
  `/` stayed 200. Then the daemon (new `friends.cpp`), then the client.
  Verified in production with three throwaway accounts (one accepted
  friend, one never-accepted): `/commons/presence/lots` correct for each
  and for a guest; in a real browser a lit lot 2 showed the friend's
  name, the not-a-friend account stayed absent, tapping it said visiting
  isn't open, the tile stayed solid, and a direct join into another
  account's house still returned 403. All three accounts deleted through
  `/settings/delete`. Process note: `/tmp` cookie jars did not survive
  between shell calls, so my first friend keepalive silently lapsed --
  caught because the door stayed dark, not assumed. Visiting is next and
  needs an open/friends/locked decision first.
- **2026-09-18 — visiting (friends policy) built, local-tested, not
  deployed.** Brandon picked **friends**: a fixed rule for every house
  (per-owner settings would need stored data and UI -- a later round).
  A bound player may be in another's house iff that owner is a mutual,
  non-blocked friend (civic's answer through the existing 15s cache) and,
  to enter, the owner is in the game. A visit is read-only: no rows
  created for the visitor, no snapshot, no resume, `/placed/` POST still
  owner-only. `move` re-checks friendship, so an unfriend or block ends
  a visit within one TTL, answered `200 not_welcome` (a 403 would read as
  a dead credential). Client: the lit lot door ("Visit") enters
  `mode: "visit"`; the wall says whose it is and reads **Look**. Caught
  a real bug before it shipped: the client treated every 403 as a dead
  credential, so a *declined* visit would have reminted and retried
  forever -- now it reads the reason. Daemon + client only; no civic, no
  nginx. Tested with three real civic accounts (friend, non-friend,
  blocked mid-visit; guest; owner offline) plus a real-browser client
  pass. See `visiting-built.md`. Holding on deploy.
- **2026-09-18 — visiting deployed, live.** Brandon said "deploy"
  (daemon, then client; no civic, no nginx). Drift-checked first --
  only lines this round rewrote. Verified in production with three
  throwaway accounts: friend refused while the owner is offline;
  stranger, guest, and stranger-placing all 403; in a real browser the
  friend's lot door read "Visit", walking in landed in the owner's house
  at the normal tile with her wall item visible, and the wall refused
  the visitor. A read-only query of the production DB showed only the
  owner has a `players` row -- the visit stored nothing. Then the owner
  blocked the friend and, within one cache window, the real client was
  walked out with a message, the door went dark, the house emptied, and
  re-entry returned 403. All three accounts deleted via
  `/settings/delete`. See `visiting-built.md`'s "Deployed" section.
- **2026-09-18 — visiting arc closed out.** Grok independently confirmed
  the deploy with no SSH (`GROK_VISITING_LIVE.md`): Square up, empty
  presence and lots, public `/internal/friends` 404; the logged-in Visit,
  wall Look, and block eviction rest on the production three-account
  pass. Visitor-linger and the 15s eviction lag stay as known limits.
  The old "mine boarded until a visit actually works" gate is **lifted**
  -- but the mine is not to be opened unasked. Next named BUILD is
  Brandon's to choose: **mine**, **`/latest`** (the third civic edit), or
  **bound inventory**; `open`/`locked` policies are later. Nothing is
  queued and no question is pending with Grok -- it already named the
  options.
- **2026-09-18 — bound inventory built, local-tested, not deployed.**
  Brandon said "BUILD bound inventory". A bound player's lab lookups now
  persist in `schema.sql`'s never-written `inventory` table and are
  restored on load, so the wall places what you last looked up even
  after a refresh or a daemon restart. The table had no recency, and
  "most recent" is the only thing the client uses inventory for, so it
  gains `updated_at` (ms) -- **the first change to an existing table**:
  `migrate()` only runs `schema.sql` on a brand-new DB, so a new
  idempotent `ensure_inventory_recency()` adds the column at startup.
  Tested against an old-shape DB, twice (idempotent). Routes
  `POST /presence/pickup` and `GET /presence/inventory` live under
  `/presence/` so nginx needs no change; identity is the cookie only,
  no guest fallback (guests stay RAM). Pickups aren't consumed and no
  menu exists. Tested with real civic accounts and a real restart, plus
  a real-browser client pass (guest sends nothing; bound restores once;
  an in-flight pick isn't overwritten). Daemon + client only. Deploy
  note: back up the production `commons.sqlite` first. See
  `bound-inventory-built.md`. Holding on deploy.
- **2026-09-18 — bound inventory deployed, live.** Brandon said
  "deploy" (backup, daemon, client; no civic, no nginx). Took a
  consistent online backup of the production `commons.sqlite` first
  (`/var/lib/commons/backups/commons.sqlite.pre-inventory-20260918T204938Z`,
  integrity ok, baseline counts recorded), drift-checked, then deployed
  the daemon and confirmed on the *live* DB that `updated_at` was added,
  integrity ok, and every pre-existing row count matched the baseline --
  the first migration on real production data. Then the client. In a real
  browser: a bound throwaway account looked up two lab stations, the
  daemon stored them (most recent first), a page reload restored both
  with no lab visit, and the wall placed the restored item in its house;
  as a guest afterward, a lookup sent nothing. Account deleted; its game
  rows are orphans by design. The backup is left on the box. Remaining
  named options: **mine**, **`/latest`**.
- **2026-09-18 — /latest built, local-tested, not deployed.** Brandon
  said "BUILD /latest". Third civic edit: one branch,
  `GET /canvas-frame/<username>/latest`, registered ahead of the existing
  index route (left byte-identical -- it `stoi`s the last segment, so
  `latest` must never reach it; frame 0 is the banned first stroke). Same
  checks as the index route, every failure a plain 404 the game reads as
  an empty slot. Client: in a house (yours, or a friend's you are
  visiting) the owner's real profile drawing hangs in the wall slot,
  fetched same-origin so the viewer's cookie drives civic's block check;
  the shared demo house and lab keep the empty slot; a placed item
  shrinks to a corner badge so the two share the slot. Tested with real
  uploaded PNGs (a second frame becomes `/latest`, index 0 unchanged,
  block and clear both 404) and a real browser. Deploy order: civic, then
  client; no daemon, no nginx. See `latest-built.md`. Holding on deploy.
- **2026-09-18 — /latest deployed, live.** Brandon said "deploy" (civic,
  then client; no daemon, no nginx). Drift-checked first -- civic was 27
  added lines, zero removed. Verified in production with three throwaway
  accounts and real uploaded drawings (host red, visitor blue, one bare):
  in a real browser the visitor's own house showed his blue, the host's
  house showed the host's **red** (sampled from the fetched image), and
  the bare friend's house showed the empty slot with nothing carried over.
  A viewer who blocked the owner gets 404; anonymous still 200. **Two
  things worth knowing:** (1) civic's block check is one-directional (it
  hides frames from a viewer who blocked the *owner*, not the reverse, and
  anonymous skips it) -- copied as specified, and `canvas-frame.md`'s
  "either side" overstates it; an owner-side hide would be a separate civic
  change. (2) **Pre-existing client bug found, not fixed:** a bound
  player's own daemon row (`u:<name>`) isn't filtered out by the client's
  guest-credential `netId`, so they are drawn as a second copy of
  themselves, lagging by up to a poll interval while walking. Live since
  cookie bind. Small fix, deliberately not bundled into this deploy. All
  accounts deleted. Remaining named option: **mine**.
- **2026-09-18 — self-ghost built, local-tested, not deployed.** Brandon
  said "build" (read as self-ghost; nothing in that message named it). The
  client filtered "is this occupant me" against only its guest credential
  `netId`, but a bound player's daemon row is `u:<name>`, so their own row
  was drawn a second time, lagging ~0.5s. Fix is client-only, +15/-2: an
  `isSelfId` helper that knows both ids, `netBoundId` set from every
  join/rekey answer, used at both occupant-poll filters; matches on id,
  never `display_name`. Verified in a real browser per Grok's resume
  brief -- no `credentials: include` patch, no second client copy, no
  scratch civic accounts (I had begun all three before reading that brief,
  written while I was out of budget; I tore it down and redid it its way)
  -- with one stubbed `u:` row: the ghost reproduced with the bound id
  unknown, vanished on the bound join answer and stayed gone, a
  look-alike `ghostuser (2)` row was still drawn, guests unchanged. No
  deploy until Grok reviews; deploy later is client only. See
  `self-ghost-built.md`. **CI:** `.github/workflows/commons.yml` is
  committed locally from `ci.md` but **not pushed** -- the GitHub token
  lacks the `workflow` scope; see the status report, not resolved here.
- **2026-09-18 -- CI landed; self-ghost deployed (client only).** After
  Brandon granted the `workflow` scope, `.github/workflows/commons.yml`
  (rebuilt from the updated `ci.md`: `fetch-depth: 0`, no `|| true` on the
  gate-5 `git diff`) was pushed as `91880e5`; first run green (56s, incl.
  `nginx -t`). Gate 5 checked locally: full clone fails a civic+client range
  whose head subject lacks `civic`/`deploy`, passes with it, skips an
  all-zero `before`, and a shallow clone now fails loud (`bad object`)
  instead of skipping. Then Brandon said `deploy`: client only via
  `sync-client-to-droplet.sh` -> `/opt/commons`, chown liquidgov; droplet
  and served `index.html` md5 both equal local (`f1354d53...`). No daemon,
  civic or nginx change. Not yet checked: a real bound session on production
  (one sprite while walking) -- the local page can't carry a civic cookie.
  **Confirmed by Brandon, logged in on production: nothing trailing.**
  Self-ghost closed. Remaining named option: **mine**.
- **2026-09-19 -- mine A built, local-tested, not deployed.** "BUILD mine"
  had no spec (every earlier doc said boarded, no interior), so I asked
  (`CLAUDE_MINE_QUESTION.md`); Brandon/Grok chose **A, honest cave, no ore,
  guests welcome, client only** (`GROK_MINE_ANSWER.md`). The Quarry Ridge adit
  now has one door tile into a shared 16x12 `instance:mine` (dirt, rock walls,
  two sit-rocks, one lit lamp tile, one sign "No ore. That's honest."); the
  outdoor sign reads "Watch your step."; nothing to place or pick up; Enter
  leaves onto the tile in front of the adit; the passerby stays outdoors. No
  daemon/civic/nginx/schema change, no new tileset (two flat-colour tile ids).
  Verified as a guest in a real browser against a scratch daemon. See
  `mine-built.md`. Deploy is client only, after Grok's review and Brandon's
  `deploy`.
- **2026-09-19 -- mine A deployed (client only).** Grok accepted
  (`GROK_MINE_REVIEW.md`); Brandon said `deploy`. Synced to `/opt/commons`,
  droplet and served md5 equal local (`1f4eab59...`); no daemon/civic/nginx
  change. Live check with two guest tabs: both in `instance:mine`, each saw
  the other; sign, exit-to-adit, outdoor sign, workshop "Closed for now.",
  Hall link all as designed. Named leftovers now: none open; `open`/`locked`
  policies, ~30s snapshot, `rebind()`, `lg_session` cache and orphan reaping
  stay parked.
- **2026-09-19 -- CI pinned to ubuntu-24.04.** Brandon: "BUILD CI pin".
  One line in `.github/workflows/commons.yml` (`runs-on: ubuntu-latest` ->
  `ubuntu-24.04`), ahead of `ubuntu-latest` moving to Ubuntu 26 on
  2026-10-19. Pushed as `807b4bd`; run green in 1m4s (daemon build and
  `nginx -t` on the pinned image); the Ubuntu-26 annotation is gone. Left
  alone: `actions/checkout@v4` (Node 20 annotation remains; a `@v5` bump is
  a separate one-liner) and `ci.md`'s block still says `ubuntu-latest`
  (Grok's doc). Parked still: house `open`/`locked`, the ~30s snapshot.
- **2026-09-19 -- checkout@v5.** Brandon: "BUILD checkout@v5" (Grok's
  `GROK_CHECKOUT_V5.md`; Node 20 leaves the runners 2026-09-23). One line:
  `actions/checkout@v4` -> `@v5`; `fetch-depth: 0` and `ubuntu-24.04` stay;
  no `ACTIONS_ALLOW_USE_UNSECURE_NODE_VERSION`. `ci.md`'s block synced to
  match. Pushed as `4293edf`; run green in 1m11s and the run page shows no
  annotations at all (the Node 20 warning is gone). No droplet, no game.
  Parked still: house `open`/`locked`, the ~30s snapshot.
- **2026-09-19 -- ~30s snapshot built, local-tested, not deployed.** Brandon:
  "BUILD 30s snapshot" (spec: `presence.md`; Grok had it parked as "no").
  Daemon only. Every ~30s, each bound player standing in their own house has
  their position written to their `players` row (the same `snapshot_leave`
  write leave/rekey use), so a crash mid-session resumes at most ~30s stale
  instead of from the last walk-out. No thread: `HttpServer::set_periodic`
  polls the listen socket with a timeout and runs the hook between requests on
  the accept-loop thread. Verified with `kill -9` against real civic: control
  (killed before any tick) has no resume; after 35s it resumes (5,4,f1);
  standing in the demo house writes nothing to the house row; the persist-house
  regression still passes. See `snapshot-built.md`. Deploy is daemon only,
  after Grok's review and Brandon's `deploy`. Parked still: house
  `open`/`locked`.
- **2026-09-19 -- ~30s snapshot deployed (daemon only).** Grok accepted
  (`GROK_SNAPSHOT_REVIEW.md`); Brandon said `deploy`. Drift check clean (only
  the four edited files differed), online SQLite backup first
  (`commons.sqlite.pre-snapshot-20260919T041014Z`, left on the box), sync,
  `-j1` build on the droplet, restart. Verified across a 40s window through
  nginx: lots/presence/placed 200, `/internal/friends` 404, no journal
  errors, no restarts, DB integrity ok with counts unchanged, loopback bind.
  A real bound tick is unproven on prod (needs a bound player in their own
  house across a tick, then a restart). Parked still: house `open`/`locked`.
- **2026-09-19 -- house friends/locked (A) built, local-tested, not deployed.**
  "BUILD house open/locked" had no spec and `open` collides with the civic
  freeze (civic can't say who has blocked whom, so a stranger-facing door
  couldn't honour a block); I asked (`CLAUDE_HOUSE_POLICY_QUESTION.md`) and
  Brandon/Grok chose **A: friends/locked only, `open` waits on a civic block
  probe** (`GROK_HOUSE_POLICY_ANSWER.md`). An owner locks their own house from
  a latch tile inside it; `players.visit_policy` (default `friends`, idempotent
  startup migration), owner-only `POST /presence/house-policy` (cookie identity,
  guests 403, `open` 400), `visit_allowed` checks it on join/rekey/**move** so a
  visitor is walked out on their next move, locked owners drop out of
  `/presence/lots`, and join/rekey echo the owner's policy. Verified against
  real civic with three accounts (incl. restart persistence and a real
  old-shape DB migration) and in a real browser with the request stubbed. No
  civic/nginx change. See `house-policy-built.md`. Deploy: **daemon first**
  (schema change: online SQLite backup first), then client, after Grok's review
  and Brandon's `deploy`. Parked still: `open`, `rebind()`, `lg_session` cache,
  orphan reaping.
- **2026-09-19 -- house friends/locked deployed (daemon, then client).**
  Brandon said `deploy`; Grok's review was not on file yet, so it went on his
  word. Drift check clean (only the six edited daemon files + the client
  differed), online SQLite backup first
  (`commons.sqlite.pre-house-policy-20260919T070744Z`, left on the box),
  daemon `-j1` build + restart, and the migration ran on the real DB: column
  present, all 5 players `friends`, integrity ok, counts unchanged. Then the
  client (droplet = served = local `e99898fa...`). New route 403s without a
  cookie; `/internal/friends` still 404. A real bound owner locking a real
  house is unproven on prod (needs Brandon logged in). Parked still: `open`
  (needs a civic block probe), `rebind()`, `lg_session` cache, orphan reaping.
- **2026-09-19 -- tile dither built, local-tested, not deployed.** Brandon:
  "BUILD tile dither" (Grok's graphics note: the cheap pass, code only).
  Client only. `paintTile()` replaces the bake's flat `fillRect` with the same
  base colour plus grass 2x2 checker dots (position-hashed phase), a darker
  water lip on edges that meet non-water, and wood grain on `T_BENCH`; every
  shade derived from the tile's own colour, so no new palette, asset or draw
  path. One bake, one blit, ~51ms at load. Looked at in a real browser and
  pixel-checked; console clean. See `tile-dither-built.md`. The tileset PNG +
  player sheet is the separate, un-named next step. Deploy: client sync on
  Brandon's `deploy`.
- **2026-09-19 -- tile dither deployed (client only).** Grok accepted
  (`GROK_TILE_DITHER_REVIEW.md`); Brandon said `deploy`. Client sync to
  `/opt/commons`; droplet = served = local `dde34287...` (was `e99898fa...`);
  no daemon/civic/nginx change. Guest check on prod: grass checker and creek
  lip render, console clean. Open: Brandon's eye on the dots at real zoom (thin
  the lattice if they crawl). Tileset PNG + player sheet still unnamed.
- **2026-09-19 -- tileset PNG + player sheet built, local-tested, not deployed.**
  Brandon: "BUILD tileset PNG + player sheet" (Grok's graphics note +
  `atlas-pack.md`: `tiles.png` 16x16 grid in `T_*` order, `sprites.png` 4 dir x 2
  frame, bake stamps once). Client only. I drew it, from `art.md`'s V1 list, with
  `Persistence/TheCommons/art/make_art.py` (stdlib, deterministic, checks each
  tile's mean colour against the client's own `COLOR`). The bake stamps from the
  sheet via `drawTile()` once the PNG loads (world first bakes with `paintTile`,
  which stays as the fallback; a 404 or wrong size keeps the painted look).
  Grass variants in cells 48-51 keep the hash variation. One adjacent fix: rooms
  smaller than the viewport (the mine) left stale outdoor pixels below them; the
  margin is now cleared to the page black and the bottom hint/toast draw in paper
  there. Verified in a real browser (Square, creek, woods, Homestead, Lab, mine,
  all four facings, no-PNG fallback). See `tile-sheets-built.md`. Deploy: client
  sync now carries index.html + tiles.png + sprites.png; check `200 image/png`.
- **2026-09-19 -- tileset PNG + player sheet deployed (client only).** Brandon
  said `deploy`; Grok's review was not on file, so it went on his word. Client
  sync now carries three files; droplet = served = local for all three
  (`index.html` `1e924625...`, `tiles.png` `853cdf21...`, `sprites.png`
  `ae31b218...`); both PNGs `200 image/png` through nginx; no daemon/civic/nginx
  change. Guest walk on prod: sheet loaded, Homestead/Lab/mine render with the
  new art, mine margin black with a readable toast, console clean. Art tweaks =
  edit `make_art.py`, re-run, sync, bump `?v=`. Parked still: `open` (needs a
  civic block probe), `rebind()`, `lg_session` cache, orphan reaping; Brandon's
  eye on the grass dots and the new art at real zoom.
- **2026-09-19 -- daemon janitor (rest slice 2) built, local-tested, not deployed.**
  Brandon: "BUILD session cache"; Grok's `GROK_REST_2.md` pairs it with the
  `sqlite3_step` checks, so I built the slice as written. Daemon only. (1)
  `civic::resolve_username` keeps a positive answer 15s in RAM keyed on the Cookie
  value (failures and "no such session" are never cached; guests never reach
  civic; a logged-out session can resolve up to 15s). (2) `sqlx.hpp` wraps every
  prepare/bind/step/COMMIT: a failure logs the sqlite message and becomes a 500
  `server_error` for that request, `Tx` rolls multi-row writes back, catalog
  readers no longer return a silent `[]`; snapshots stay best-effort so a leave
  can't strand a player. Verified with real civic frozen via SIGSTOP (hit 0.46ms,
  miss ~1s, failure not pinned, TTL expiry) and failures injected with triggers
  (wall write rolled back, leave still ok, daemon never died); all prior
  regressions pass. See `janitor-built.md`. Deploy: daemon only, online SQLite
  copy first, after Grok's review and Brandon's `deploy`. Parked: `rebind()` (3),
  `open` + orphans (4).
- **2026-09-19 -- daemon janitor (rest slice 2) deployed (daemon only).** Grok
  accepted (`GROK_REST_2_REVIEW.md`); Brandon said `deploy`. Drift check clean
  (five edited files + new `sqlx.hpp`), online SQLite backup first
  (`commons.sqlite.pre-janitor-20260919T084253Z`, left on the box), sync, `-j1`
  build with no reconfigure, restart. 40s window through nginx all 200 (lots,
  `1,1`, house placed), `/internal/friends` 404, no `failed`/sqlite lines in the
  journal, no restarts, DB integrity ok with counts unchanged, loopback bind. A
  real bound cache hit is not observable from outside. Parked: `rebind()` (slice
  3), `open` + orphans (slice 4, the only civic one).
- **2026-09-19 -- `rebind()` (rest slice 3) built, local-tested, not deployed.**
  Brandon: "BUILD rebind" (`GROK_THE_REST.md` slice 3; `cookie-bind.md` left the
  primitive open). A guest who logs in mid-session is now relabelled `u:<name>` IN
  PLACE on their next move: `PresenceStore::rebind` keeps x/y/facing/last_update,
  spends no cap slot, supersedes a second tab's older row, disambiguates the name
  ("alice (2)"); it requires the cookie to resolve AND the guest secret to still
  verify, and never applies to a house region. The move answer carries
  `promoted`/`bound`/`username`/`display_name`; the client applies it through the
  same `netApplyJoinJson` a join answer uses (so the self-ghost filter, nameplate,
  house key follow). Verified with real civic (one `u:` row, no ghost, guards,
  second tab, name collision) and in a real browser with the answer stubbed; all
  prior cookie/visit/policy/persist regressions pass. See `rebind-built.md`.
  Deploy: daemon, then client, after Grok's review and Brandon's `deploy`. Parked:
  `open` + orphans (slice 4, the only civic one).
- **2026-09-19 -- `rebind()` (rest slice 3) deployed (daemon, then client).**
  Brandon said `deploy`; Grok's review was not on file, so it went on his word.
  Drift check clean (three daemon files + the client), online SQLite backup first
  (`commons.sqlite.pre-rebind-20260919T085640Z`, left on the box), daemon `-j1`
  build with no reconfigure + restart (DB integrity ok, counts unchanged, no
  failures, no restarts), then the client (droplet = served = local
  `9a8ac18c...`). Real guest join/move/bad-secret/leave unchanged on prod; guest
  walk clean. The promotion itself is unproven on prod (needs a real mid-session
  login): log out, open the game, log in on LiquidGov in another tab, walk -- the
  nameplate should become your username with no second sprite. Parked: `open` +
  orphans (slice 4, the only civic one; Grok: not before 1-3 are live, which they
  now are).
- **2026-09-19 -- `open` houses (rest slice 4a) built, local-tested, not deployed.**
  Brandon: "BUILD open A, split, orphans log-only"; Grok's
  `GROK_REST_4_ANSWER.md`: 4a first with discovery A, 4b orphans later. **Fourth
  named civic edit:** same-box `GET /internal/blocked/<username>` (cookie = viewer,
  path = owner, `{"blocked":true}` if either side blocked the other, fails
  closed on no session/unknown owner/malformed). Daemon: `civic::block_answer`
  (15s cache of real answers only, plain-username check before the request line),
  `visit_allowed` lets a non-friend in only for `open` + a real `blocked:false`,
  `/presence/lots` = friends first then open houses of non-friends (cap 6, guests
  none, stops probing when civic is silent), setter accepts `open`. Client latch
  cycles friends -> open -> locked. Verified against real civic with seven
  accounts (blocks both directions, guest, injection guard, live policy changes,
  block eviction after the TTL, fail-closed with civic frozen) and in a browser
  with the request stubbed. See `open-built.md`. Deploy: **civic first** (drift-check
  the droplet's `http_server.cpp`), then daemon, then client, after Grok's review and
  Brandon's `deploy`. 4b orphans is its own BUILD (log-only first).
- **2026-09-19 -- `open` houses (rest slice 4a) deployed (civic, then daemon, then
  client).** Grok accepted (`GROK_REST_4A_REVIEW.md`); Brandon said `deploy`. Civic
  drift check first: the droplet's `http_server.cpp` equalled the previous git version
  (`81eb0e5e...`), only that file differed. Online game-DB backup
  (`commons.sqlite.pre-open-20260919T093423Z`, left on the box), civic `-j1` build +
  `liquidgov` restart (public `/internal/blocked/...` is 404; on-box it answers and
  refuses X-Forwarded-For; accounts store untouched), daemon `-j1` + restart (DB
  integrity ok, counts unchanged, all houses still `friends`), then the client
  (droplet = served = local `95f997e7...`). No failures, no restarts. An owner flipping
  a real house to `open` and a stranger's door lighting is unproven on prod (needs two
  real accounts). **4a is live.** Parked: 4b orphans (own BUILD: `user-exists`,
  log-only first, two passes, cap 3, backup).
- **2026-09-19 -- orphan watch, LOG-ONLY (rest slice 4b) built, local-tested, not
  deployed.** Brandon: "BUILD orphans log-only"; Grok's valves (`GROK_REST_4_ANSWER.md`).
  Civic: same-box `GET /internal/user-exists/<username>` (`exists` true/false for a
  well-formed name, `null` for a malformed one, never false). Daemon: `civic::user_exists`
  (Exists/Gone/Unknown; only an explicit false is Gone) and a ~10-minute pass on the
  existing hook that finds players rows whose account is gone and only LOGS `would reap`
  with a read-only footprint (inventory rows, placed rows, house instance). Valves: two
  consecutive passes, cap 3 per pass, online players skipped, stops at the first
  don't-know, suspect guard when every account reads gone. **No DELETE anywhere.**
  Verified against real civic with real deleted accounts (rows unchanged; SIGSTOP resets
  the streak; re-registered name clears it; cap defers the 4th; suspect guard). See
  `orphans-built.md`. Deploy: civic then daemon, no client, backup first; then read the
  journal (~20 min) -- expect the earlier throwaway accounts. The reap itself is a
  separate later BUILD Brandon names after reading the list.
- **2026-09-19 -- orphan watch (log-only) deployed (civic, then daemon).** Grok accepted
  (`GROK_REST_4B_REVIEW.md`); Brandon said `deploy`. Civic drift check clean (droplet =
  previous git version), online game-DB backup first
  (`commons.sqlite.pre-orphans-20260919T101314Z`, left on the box). Fixed a suspect-guard
  flaw found while reading the real `players` list (online players are skipped, so it
  compared against too few accounts): now against every known account (`81bc825`).
  Civic `-j1` + restart (public `/internal/user-exists/x` is 404), daemon `-j1` + restart
  at 10:17:21Z (startup line says LOG-ONLY; DB integrity ok, counts unchanged). Production
  `players`: persistverify9, vtowner, invverify1, Bbottin, ptvisitor -- expect the four
  throwaway names to show as would-reap. Nothing is deleted. The reap is a separate later
  BUILD after Brandon reads the list.
- **2026-09-19 10:37Z -- orphan watch's first list (nothing deleted).** After two passes:
  would reap `persistverify9` (house 3, 0 inv, 1 placed), `vtowner` (house 4, 0 inv, 1
  placed), `invverify1` (house 5, 2 inv rows, 1 placed); `ptvisitor` (house 6, empty)
  deferred by the cap, due next pass. `Bbottin` (real) exists and is not listed. All four
  are throwaways deleted via `/settings/delete` in earlier live tests. DB counts unchanged.
  The reap is a later BUILD once Brandon agrees the list.
- **2026-09-19 -- orphan REAP built, local-tested, not deployed.** Brandon: "BUILD orphans
  reap" after reading the first log-only list. Daemon only. The ~10-minute pass now deletes
  a qualifying name's inventory, house `placed` rows, `players` row and owned house
  `map_instances` row in ONE rolled-back-on-failure transaction. Still needs: explicit
  civic `exists:false`, two consecutive passes, cap 3 per pass, not online, suspect guard.
  New: an automatic online SQLite backup per reaping pass (`backups/commons.sqlite.pre-reap-*`;
  no backup, no reap), one last civic check right before each delete, and a
  `COMMONS_ORPHAN_REAP=0` kill switch (default ON). Verified with real civic: living accounts
  untouched, valid pre-reap backups, a new owner of a reaped name gets an empty house and no
  inventory, frozen civic reaps nothing, failed backup reaps nothing, kill switch works.
  (My first run used invalid one-letter usernames and exercised nothing; redone.) See
  `orphans-reap-built.md`. Deploy: daemon only, backup first, after Grok's review and Brandon's
  `deploy`; expect `persistverify9`, `vtowner`, `invverify1` at ~20 min and `ptvisitor` at ~30.
  Also noted: Grok's `civic-ratelimit.md` (explore, not a build): no limiter on `/internal/`.
- **2026-09-19 11:30Z -- orphan REAP deployed (daemon only).** Grok accepted
  (`GROK_ORPHAN_REAP_REVIEW.md`); Brandon said `deploy`. Drift clean (five daemon files),
  manual online SQLite copy first (`commons.sqlite.pre-reap-manual-20260919T112956Z`),
  `-j1` build + restart at 11:30:28Z; journal: `orphan watch is REAPING`; DB integrity
  ok, counts unchanged, no failures. Expected: 1st pass ~11:40Z (logs gone), 2nd ~11:50Z
  reaps persistverify9, vtowner, invverify1 (automatic backup first), 3rd ~12:00Z reaps
  ptvisitor; Bbottin untouched (civic says it exists). Kill switch:
  `COMMONS_ORPHAN_REAP=0` + restart. Result to be read from the journal and recorded.
- **2026-09-19 12:20Z -- orphan reap ran; four throwaways gone, Bbottin untouched.** Read
  (read-only) at 11:55Z and 12:20Z. 11:50:26Z: automatic backup, then REAPED
  persistverify9, vtowner, invverify1, 1 deferred by the cap; 12:00:27Z: second automatic
  backup, REAPED ptvisitor. Live DB: integrity ok, foreign_key_check clean, nothing
  dangling; players 5 -> 1 (exactly Bbottin), map_instances 6 -> 2 (demo, lab), placed 3 ->
  0, inventory 4 -> 2. Bbottin is kept because civic says it exists, not by name. Three
  backups on the box (manual, 11:50, 12:00), all integrity ok, the 11:50 one holds all 5
  players. No failures, no restarts. The inheritance hole (a re-registered deleted
  username getting the old player's house/inventory) is closed; the watch keeps running,
  kill switch `COMMONS_ORPHAN_REAP=0`. **Rest slices 1-4 and the reap are all live.**
  Nothing is parked from that list; `backup-strat.md` and `civic-ratelimit.md` are explore
  notes, not builds.
- **2026-09-19 -- hardening (four daemon fixes) built, local-tested, not deployed.** From
  Claude's survey, Grok's `GROK_HARDENING.md`. (1) guest secrets from `getrandom`, not a seeded
  Mersenne Twister; (2) guest registry capped at 256 -- **evicts instead of the spec's 503**
  (the client doesn't check the mint reply), never-used credentials expire in 2 min and go
  first; (3) 5s socket timeouts + 10s whole-request deadline, truncated bodies closed not
  dispatched; (4) `FriendsCache` no longer caches a civic failure (`resolve_friends` returns
  optional; unknown = never evict a friends-house visitor, never admit a new one, lots dark
  for that poll only). Tested against real civic incl. frozen-civic timing proofs; my first
  friends-cache test was wrong (froze civic after both caches had expired) and was redone.
  See `hardening-built.md` (also notes a possible civic-outage breaker for Grok to decide).
  nginx `limit_req` on `/guest` stays a later named deploy. Deploy: daemon only.
- **2026-09-19 12:55Z -- hardening deployed (daemon only).** Grok accepted
  (`GROK_HARDENING_REVIEW.md`: evict-not-503 correct, no civic breaker, nginx `/guest`
  limit later); Brandon said `deploy`. Drift clean (eight daemon files), manual online SQLite
  copy first (`commons.sqlite.pre-hardening-20260919T125451Z`), `-j1` build + restart at
  12:55:33Z; DB integrity ok, counts unchanged, journal clean, no restarts. Public guest
  mint/join/move/bad-secret-403/leave all as designed with OS-random ids and secrets; a
  stalled half-request now holds the daemon ~5s instead of forever. Unseen on prod: the cap
  under a real flood and a real civic hang. Remaining named items from the survey, all
  unbuilt: nginx `limit_req` on `/guest` (deploy), tests-in-CI, deploy scripts, backup
  retention, splitting `index.html`, `/health` hash.
- **2026-09-19 -- tests in CI built; CI is the last check.** Brandon: "BUILD tests in CI"
  (`GROK_NEXT.md` #1). `commons-daemon/tests/`: 11 files, ~300 real assertions, `lib.sh`,
  `run_all.sh`, frozen old-schema fixtures, and `civic_stub.py` (in-memory civic: the public
  signup/friends/block/delete routes plus the four `/internal/*` routes with civic's semantics).
  The stub is validated: the whole suite passes against BOTH the stub and the real
  `liquidgov_http` (~210s each). New workflow step runs it after the daemon build (installs
  `strace`; per-file and per-step timeouts). Mutation-checked: with the pre-hardening daemon put
  back the suite fails on the cap, friends-cache and socket-timeout tests -- and that check first
  showed the guest test would HANG on a daemon without timeouts, fixed (fail-fast probes). My own
  draft mistakes (invalid usernames earlier, rows created before the daemon was up, a NULL
  printing as 'None') were caught by asserting preconditions. No daemon/civic/client/nginx change,
  no droplet, no deploy. First CI run (d9d1b51): 10/11 files green incl. the strace check; `test_house` failed because
  `test_guest`'s strace wrapper was killed but its traced daemon lived on, holding :8765 (the next file's daemon died on
  bind(); health passed against the survivor). Harness fixed + `test_harness.sh` added. See `tests-built.md`. Still unbuilt: deploy scripts, nginx `/guest`
  limit, backup retention, `index.html` split, `/health` hash.
- **2026-09-19 -- `reqjson` corrected: latent bug, not harmless.** Grok's review of `c6a7a4f` (CI green,
  accepted) caught that I called the naive `find("\"key\"")` scanner harmless. Reproduced: name-before-fields
  bodies make a guest named `x`/`y` 400, `secret` 403, `id`/`facing` get a wrong "(2)" rename. The shipped client
  sends `display_name` last, so it does not bite today (all seven colliding names joined) -- luck of key order.
  Awaiting Brandon's `BUILD reqjson` (daemon only; parse `"key":` as a real key walk + `test_reqjson.sh`).
- **2026-09-19 -- `BUILD reqjson` built (daemon only, NOT deployed).** `reqjson.hpp` now walks the flat object
  (key, `:`, value, `,`) so a field is found only in KEY position; a value equal to a field name (a guest named
  `x`, name-first body) can no longer be read as a key. Same API/call sites; `field_num` stricter (JSON-number
  token only). `tests/reqjson_unit.cpp` (201 checks) + `tests/test_reqjson.sh` (102 assertions incl. name-first
  bodies through the daemon) run in CI via `run_all.sh`; mutation-checked against the old header. The live
  daemon still has the old parser until `deploy`. See `reqjson-built.md`. Next per Grok: deploy scripts, then nginx.
- **2026-09-19 -- `BUILD deploy scripts` built (files + fake-droplet tests; NO droplet contact).** `commons-daemon/deploy/`:
  `lib.sh` (shared config + the one exclusion list + a deployed-revision marker), `drift-check.sh` (droplet vs the
  LAST-DEPLOYED revision by checksum, so a hand edit on the box is reported, plus what this deploy will change),
  `backup-db.sh` (SQLite backup API, integrity_check, counts, never overwrites), the two syncs reworked (refuse a
  dirty/unpushed tree, write the marker, `-n`), `build-on-droplet.sh` (`-j1`, reconfigure only if CMake changed),
  `verify.sh` (systemd, journal, loopback, public routes, a real guest incl. one named `x`, row counts vs the backup),
  `deploy-daemon.sh` (the sequence; PLAN-ONLY without `--go`; refuses past an undeployed civic change). First real
  run: `deploy-daemon.sh --baseline 60f6745` (plan, read-only) on `deploy`. `tests/test_deploy.sh` (119 assertions,
  fake droplet behind a shim ssh) is mutation-checked (10 breakages) and found 2 real bugs in my own scripts;
  CI also runs `bash -n` + `shellcheck` on them. The reqjson fix is still not live (needs `deploy`). See
  `deploy-scripts-built.md`, `commons-daemon/deploy/README.md`. Still unbuilt: nginx `/guest`, backup retention.
- **2026-09-19 ~15:25Z -- reqjson DEPLOYED (daemon only), the first real run of the deploy scripts.** Brandon: `deploy`
  (plan-only: no drift, 10 files: `reqjson.hpp` + nine `deploy/` files) then `deploy` again (`--go --baseline 60f6745
  --guest-name x`). Backup `commons.sqlite.pre-a56eba3-20260919T152437Z` (integrity ok); `-j1` build, no reconfigure;
  restart; verify 32/32 incl. a real guest named `x` through nginx, journal clean, every row count unchanged. The
  droplet now has `/opt/commons-daemon/.deployed-rev` = a56eba3, so the next deploy needs no `--baseline`. Civic, client
  and nginx untouched. Unbuilt: nginx `/guest` limit (own deploy word), backup retention (backups now accumulate).
- **2026-09-19 -- `BUILD nginx /guest` built (config + CI tests; NOT installed, no droplet, no reload).** New
  `deploy/nginx-commons-http.conf` (`limit_req_zone $binary_remote_addr zone=commons_guest:1m rate=20r/m`, installed
  as `/etc/nginx/conf.d/commons-guest-zone.conf`) and one EXACT-match `location = /commons/presence/guest` in
  `nginx-commons.conf` (`burst=30 nodelay`, status 429, default HTML body on purpose: the client's `netMint` never
  checks status, a JSON body would loop it on 403). `/internal/` is never touched. **The numbers are my decision (no
  doc gave any): flagged for veto in `nginx-guest-built.md`.** `tests/test_nginx_guest.sh`: 12 static assertions
  (mutation-checked, 8 breakages) + a behavioural half with a REAL nginx in front of the real daemon, CI-only (no
  nginx locally). Needs its own `deploy` (nginx by hand). Unbuilt: backup retention; client `netMint` r.ok check.
- **2026-09-19 ~15:46Z -- nginx `/guest` rate limit DEPLOYED (nginx only, by hand).** Brandon: `deploy`; Grok had
  accepted the rates + the HTML 429. Read-only first (nginx 1.24.0, conf.d empty and included, no front proxy, live
  `/commons/*` blocks identical to the repo's), backup `liquidgov.bak-pre-guestlimit-20260919T154553Z`, zone installed
  as `/etc/nginx/conf.d/commons-guest-zone.conf`, the exact-match guest location inserted after `/commons/placed/`,
  `nginx -t` ok, reload. Verified: exactly one active `limit_req`, 5 mints all 200, `verify.sh --guest-name x` 22/22,
  civic 200, public `/internal/` 404, no 'limiting requests' in the log. The 429 itself was proven in CI, not by
  flooding. Side observation (pre-existing): something polls `GET /commons/recipes` every ~6s and 404s (not proxied
  by design). Unbuilt: backup retention (Brandon picks how many / how old); client `netMint` r.ok check.
- **2026-09-19 -- `BUILD backup retention` built (files + fake-backup tests; NO droplet, nothing deleted anywhere).**
  `deploy/prune-backups.py` (pure decision) + `prune-backups.sh [--delete]` (dry run by default). Brandon's policy: delete only
  if BOTH outside the newest 14 AND older than 30 days; newest 14 stay however old; anything within 30 days stays however
  many; never the newest; never < 24h (a hard floor, redundant at the defaults, independent if `--days` is overridden).
  Naming inspected: deploy `pre-<label>-<stamp>` and the daemon's `pre-reap-<stamp>` are ONE scheme, ranked together and
  treated identically; non-backup names are ignored. Age = the younger of the name's stamp and the mtime. Refuses on a failed
  listing or a future-dated backup. `test_retention.sh` (47) + `prune_backups_unit.py` (361, incl. 300 random directories),
  mutation-checked (12 breakages). Not scheduled; not run by `deploy-daemon.sh`. Today the droplet has ~a dozen backups all
  < 2 days old, so a first real run (a dry run, on `deploy`) would delete nothing for ~a month. See `backup-retention-built.md`.
- **2026-09-19 ~16:10Z -- backup retention: first real run = a DRY RUN on the droplet (Brandon: `deploy`).** Read-only,
  nothing deleted: 12 backups, keep 12, delete 0 (all from the last ~24h; dir 1.4M, disk 57%). **Finding:** every backup
  also has `-wal` (0 B) and `-shm` (32 KB) sidecars (24 of 36 files); the pruner ignores them, so deleting an old backup would
  orphan its sidecars. Proposed, not built: delete a backup's exact `-wal`/`-shm` together with it. Needs Brandon to name it.
- **2026-09-19 -- `BUILD sidecar pairing` built (files + tests; NO droplet, no `--delete`, nothing deleted anywhere).** Follow-up to the
  retention dry-run finding: `<backup>-wal`/`<backup>-shm` (EXACT names) now delete WITH their backup (sidecars first, then the
  backup; each name re-validated, and a sidecar's backup must be in the DELETE set) and are kept with a kept one; they never
  count in the newest 14 and are never judged by their own age. A sidecar whose backup is gone is an ORPHAN: reported, never
  deleted. The dry-run list is newest-first with each backup's sidecars right under it. Summary line now counts backups,
  sidecars, orphans, ignored. Unit checks 361 -> 378, `test_retention.sh` 47 -> 76 (real sidecar files, orphans, look-alikes),
  mutation-checked (8 more breakages). Today's droplet dry run would read `12 backups: keep 12, delete 0; sidecars: keep 24,
  delete 0`. Still not scheduled; no `--delete` until something is old enough (~a month). See `backup-retention-built.md`.
- **2026-09-19 -- `BUILD netMint check` built (client only; tested; NOT deployed).** `commons/index.html` `netMint()` now accepts a credential
  only from a 200 with string `id` + `secret` (the old one read any JSON, a JSON error body would have set `netReady` with `id` undefined
  and looped on 403s), retries every failure with capped backoff (2s,4s,8s,16s,30s cap, +/-20% jitter, honours a capped Retry-After)
  instead of leaving the player local-only until reload, and shares ONE mint loop between startup and `netHandleAuthFailure()`. The
  numbers are my decision (veto in `netmint-built.md`). `tests/netmint_unit.js` runs the SHIPPED block under node (fake clock, scripted
  fetch): 88 checks, mutation-checked (11 breakages), in CI via `test_netmint.sh`. Verified in a real browser against a real daemon:
  injected 429s retried at 0/1.7/6.5s then recovered; a real daemon restart gave 3 simultaneous 403s -> ONE mint -> rejoin. Client deploy
  needs `deploy` (`drift-check.sh --tree client --baseline <live commit>`, then `sync-client-to-droplet.sh`; no daemon restart, no nginx).
  Parked: `index.html` split, `/health` hash.
- **2026-09-19 ~16:47Z -- netMint check DEPLOYED (client only).** Brandon: `deploy` twice (read-only client drift check, then the
  sync). No drift; only `index.html` changed (sha256 8a74adbb..., byte-identical to the repo); `.deployed-client-rev` = `b04970e`
  (outside the served dir); daemon NOT restarted (`ActiveEnterTimestamp` 15:24:54Z, `NRestarts=0` unchanged); `verify.sh --guest-name x`
  22/22; live page has the `netMint:begin` block. The tree guard first refused on a stray `deploy/__pycache__/*.pyc` made by my
  retention unit test; deleted it and fixed the cause (`b04970e`: no bytecode written, syncs exclude `__pycache__`/`*.pyc`).
  Parked: `index.html` split, `/health` hash. `/commons/recipes` 404 left alone.
- **2026-09-19 -- `BUILD /health commit hash` built (daemon + verify.sh; tested; NOT deployed).** `GET /health` is now
  `{"status":"ok","commit":"<sha>[+dirty]|unknown"}` (additive: nothing parses the body; loopback-only). The droplet is not a git clone and
  deploys build with NO reconfigure, so the value is generated on EVERY `cmake --build` (custom target running
  `cmake/gen_build_info.cmake`; header rewritten only on change; only `src/buildinfo.cpp` includes it): env override > `<src>/.deployed-rev`
  (the marker the sync writes before the build) > git (+dirty) > unknown; only sha-shaped tokens get through (it lands in a C string and JSON).
  `verify.sh` now fails if the RUNNING commit differs from the marker (a stale, unrebuilt or unrestarted binary); skipped with a note when
  either side is missing. `test_health.sh` (57, incl. the real no-reconfigure incremental rebuild) + `test_deploy.sh` (131), mutation-checked
  (8). Format is my choice (veto in `health-commit-built.md`). A deploy will reconfigure (CMake changed). Parked: `index.html` split.
- **2026-09-19 ~17:23Z -- `/health` commit hash DEPLOYED (daemon).** Brandon: `deploy` twice (read-only plan, then `--go`). No drift; backup
  `commons.sqlite.pre-d9363b5-20260919T172306Z` (integrity ok); sync wrote the marker, then a RECONFIGURE + `-j1` build (CMake changed) picked it up;
  restart; verify 33/33 including the new check that the running commit equals the deployed revision (`d9363b5d7aa5`). Live `/health` now
  `{"status":"ok","commit":"d9363b5d7aa511beae70de082bb3203ad33e37ec"}`, loopback-only (`/commons/health` is 404 publicly). Row counts unchanged. Parked: `index.html` split.
- **2026-09-19 -- `BUILD index.html split`: asked before building (no spec).** No doc says how (only "one vanilla file, no build step"), and the
  file is 2,243 lines / 103 KB of shared global state (106 vars, 68 functions, load-order-dependent, one `"use strict"`). `CLAUDE_SPLIT_QUESTION.md`
  gives options (A classic `<script src>` files in the same global scope, recommended; B ES modules; C extract only CSS + networking; D don't) and
  the traps (`"use strict"` is per file; a multi-file deploy is not atomic: two-phase client sync). Proof plan: byte-equivalence of the JS,
  `node --check`, a CI file-reference test, a real-browser before/after parity check. Waiting on Brandon's one-letter answer; no code written.
- **2026-09-19 -- `BUILD index.html split` (A) built and proven; NOT deployed.** Brandon: "BUILD A. Prove load order in CI. Two-phase sync.
  Do not deploy." (`GROK_SPLIT_ANSWER.md`: classic `<script src>` files, one global scope, no modules.) `commons/index.html` is now a 42-line shell
  + `style.css` + eight scripts (`js/world, interiors, sprites, actors, net, input, interact, main`), each starting with `"use strict";`. Proof:
  the JS minus the added directives equals the old script body byte for byte; `tests/client_files_unit.js` loads the REAL files IN TAG ORDER under a
  stubbed DOM and fails on any ReferenceError (hoisting is what the split loses), checks the load-time code really ran by its effects, and its
  wrong-order controls throw; mutation-checked (11); a real-browser old-vs-new comparison found identical globals, function sources, values, baked
  world and sprite atlas, and identical runtime behaviour. Two-phase client sync (`index.html` last; nothing lands if phase 1 fails; 148 deploy
  assertions, 3 mutations). netMint tests repointed at `js/net.js`; CI gate 5 now matches any `commons/` file. See `split-built.md`. A client deploy
  needs `deploy` (client only, no daemon restart).
- **2026-09-19 ~18:17Z -- `index.html` split DEPLOYED (client only).** Brandon: `deploy` twice after review (CI green on `bf377c7`). Read-only client
  drift check first (no drift, exactly the ten expected files). The sync was delayed by a Windows host error (WSL could not start, `0x800705aa`, host
  memory pressure; nothing had reached the droplet), then re-checked the plan and ran the TWO-PHASE sync (scripts/stylesheet/images, then `index.html`
  last). All twelve client files identical to the repo on disk and as served (JS as `application/javascript`, CSS `text/css`); the live page has 8 external
  scripts and 0 inline; `.deployed-client-rev` = `cb982a4` (outside the served dir); daemon NOT restarted (17:23:28Z, `NRestarts=0`);
  `verify.sh` 23/23; a real browser on the live page entered as `x`, minted and joined, console clean. Parked list is now empty (`index.html` split done;
  `/commons/recipes` 404 left alone by instruction).
- **2026-09-19 -- `BUILD connection status`: asked before building (no spec).** My own suggestion, so no doc specifies it. Today a visitor gets NO signal
  when the client can't reach the daemon (move/poll failures are swallowed at ~10 sites; only the mint retry loop warns in the console). `CLAUDE_STATUS_QUESTION.md`:
  A = a small DOM badge outside the canvas (accessible, `role=status`), shown only after the problem lasts (4s with no credential, or 6 consecutive
  move/poll failures), hidden on the next success, one "Reconnecting…" line, observe-only (no request or retry change); B = on-canvas; C = reuse the
  toast; D = nothing. Waiting on Brandon's one-letter answer; no code written.
- **2026-09-19 -- `BUILD connection status` (A) built and tested; DEPLOYED (client only) from bd50119 on Brandon's `deploy`, verified: served files byte-identical, drift-check clean, verify.sh 23/23, daemon untouched.** Brandon: "BUILD connection status: A, as written." A small "Reconnecting…" pill,
  top centre, outside the canvas (`#connStatus`, `role=status`, empty when clear, paper/ink from the existing CSS, `z-index:15`, `pointer-events:none`, well away from the d-pad).
  State machine in `js/net.js` (`netStatus:begin/end`): shown when a mint loop has been pending >4s OR 6 consecutive heartbeats failed; hidden on the next success; no
  stay-clear timer; observe-only (no request, no retry change). **Deviation flagged:** the heartbeat is the move POST only, not the 1-4 polls per tick, or an ordinary
  restart would show it after ~1s. `netstatus_unit.js` (31) + `test_netstatus.sh` (29), mutation-checked (14). That run found a real bug in my own wrappers: under Windows node the
  unit's exit code was lost (`PIPESTATUS` after `$(...|tr)`), so a failing unit passed locally (CI unaffected); fixed in netmint/client_files/netstatus. Real browser
  + real daemon (frame loop driven from a timer, the pane being hidden): badge at the 6th failed heartbeat, hidden 257ms after the daemon answered, a refused remint
  shows at 5s and clears on recovery, no overlap with the d-pad on desktop or a 375x812 phone. Client deploy needs `deploy` (index.html, style.css, js/main.js, js/net.js). See `connection-status-built.md`.
- **2026-09-19 -- `BUILD` objects, step B (region scatter) built, tested and DEPLOYED (d2b6859, client only, verified).** Brandon: "can we finish populating the game with objects?" -> question doc (`CLAUDE_OBJECTS_QUESTION.md`) -> "all of that" -> my order B, A, C, D -> `build`
  (taken as step 1). B = 168 props from the six existing prop tiles (`SCATTER` data in `js/world.js`, placed last by `scatterRegions()`, plain ground only, refusals recorded). A test forbids the spine, jog, region seams, name strips,
  door/sign rings, spawn, waypoints and flood-fills the real collision array so nothing is sealed (14 + 7, 9 mutations; a first sealed-pocket mutation was vacuous and was redone). **Found and fixed a live bug:** `forceRedraw` was declared in
  main.js (last) but assigned by sprites.js image `onload`s, so a cached image firing between two script tags threw a strict-mode ReferenceError (benign in effect; declaration moved to sprites.js; `client_files_unit` now fires image onloads after each
  script, 55 checks). Client-only deploy needs `deploy`. Still to come, one BUILD each: A (lab lookups, daemon migration), C (house furniture), D (pick-ups, needs a question doc). See `scatter-built.md`.
- **2026-09-20 -- `BUILD pwa` built, tested and DEPLOYED (9e4611a: client, then civic with chown before the build; verified live).** Social and Civic get their own webmanifests (`/manifest-social.webmanifest` start_url `/feed`, `/manifest-civic.webmanifest` start_url `/proposals`, the shells' own standalone redirect targets, not the shells),
  picked by `page_wrap` by title like Mail; The Commons gets `commons/manifest.webmanifest` (start/scope `/commons/`) + generated apple-touch-icon/192/512 (`art/make_icons.py`, the plaza from above, maskable-safe) and the /apps tile shows it. `pwa_check.py` 71 checks, 16 mutations (Mail as a control caught my extractor bug; a first no-op sync-exclude mutation was redone).
  Real browser (Commons under a /commons/ prefix) and real compiled civic verified locally. Not tested on a device; iOS ignores manifests. Deploy order was client first, then civic with chown BEFORE the build. **nginx follow-up DONE on `deploy nginx`: `/commons/manifest.webmanifest` was `application/octet-stream` (stock mime.types has no webmanifest); fixed with an exact-match location + `default_type application/manifest+json` (NOT a `types{}` block, which would replace the whole table for `/commons/`); other types verified unchanged; mirrored in `deploy/nginx-commons.conf`; see `pwa-built.md`.**
- **2026-09-20 -- `BUILD www apex` built, tested and APPLIED (nginx only; verified live; certbot renewal proven by dry-runs before and after).** `www.liquidgov.us` served a full second copy of the site (200 on every path; `http://www` stayed on www; one cert already covers both names). Two small nginx edits (`deploy/nginx-www-apex.py`, idempotent, refuses unknown shapes):
  443: `if ($host = www.liquidgov.us) { return 301 https://liquidgov.us$request_uri; }` above the locations; port 80: certbot's www redirect goes straight to the apex. `server_name` keeps both names (cert + certbot unchanged). `deploy/nginx-www-apex.sh` = plan-only by default, `--go` backs up, `nginx -t`, reloads, verifies, auto-rolls-back.
  `test_nginx_www` 34 + a real-nginx behavior script (17, control included), the latter also run once on the droplet's own nginx as a scratch loopback instance (removed; live config md5 unchanged). Applied on Brandon's `deploy` after waiting for another session's certbot dry-run to finish (a certbot run edits nginx config temporarily); baseline and post-change `certbot renew --dry-run --no-random-sleep-on-renew` both succeeded for both names; backup `/var/backups/nginx-www-apex-20260920T041810Z/`. Grok's 'acme-challenge on www must not 301 to apex' line does not hold (nginx authenticator; proven by the dry-runs). **Found separately and fixed by another session:** certbot had been crashing (pip `cryptography 50` shadowing apt's); see `certbot-fix-built.md`. See `www-apex-built.md`.
- **2026-09-20 -- certbot was broken on the droplet; FIXED and verified (no service restarts, nginx untouched).** `certbot.service` failed 2026-09-19 14:41 UTC (exit 1); `certbot --version` crashed `AttributeError: module 'lib' has no attribute 'GEN_EMAIL'`. Cause: `pip install pywebpush` (2026-09-16) put `cryptography 50.0.1`/`cffi 2.1.1` in `/usr/local/lib/python3.12/dist-packages`, which precedes the apt tree on `sys.path` and shadows apt `cryptography 41` that apt `pyOpenSSL 23.2` needs. Can't downgrade (pywebpush needs cryptography>=47) or upgrade pyOpenSSL (<=25.3 same error, 26.x dropped `X509Req` that certbot 2.9 uses). Fix = `PYTHONPATH=/usr/lib/python3/dist-packages` for certbot only: systemd drop-in `/etc/systemd/system/certbot.service.d/pythonpath.conf` + `/usr/local/bin/certbot` wrapper. Proven: `certbot --version` 2.9.0, `certbot renew --dry-run` all simulated renewals succeeded, `systemctl start certbot.service` exit 0 (Result=success). Cert expires 2026-12-11. Rollback + backups (`/var/backups/certbot-fix-20260920/`) in `TheCommons/certbot-fix-built.md`. Lesson: a system-wide `pip install` into `/usr/local` can shadow apt libs and silently break unrelated tools; prefer a venv for `webpush.py`.
- **2026-09-20 -- `webpush.py` now runs from its own venv (`/opt/lg-webpush-venv`); the 20 system-wide pip packages that shadowed apt are gone.** Root cause of the certbot break removed; certbot works bare again (the PYTHONPATH drop-in + `/usr/local/bin/certbot` wrapper were REMOVED later the same day; verified `systemctl start certbot.service` exit 0). Run `/opt/lg-webpush-venv/bin/python /opt/liquidgov/deploy/webpush.py <cmd>` (nothing automated calls it; `send` is manual). `deploy/webpush.py` + `deploy/webpush-requirements.txt` now tracked. Details + undo in `TheCommons/certbot-fix-built.md`. Never `pip install` into `/usr/local` on the droplet; use a venv.
- **2026-09-20 -- `BUILD workshop` (Brandon: "A, E") built, tested and DEPLOYED (client only, `e6bd485`; two guest tabs on production saw each other and left cleanly).** The Workshop Lane shutter is now a real door (existing `T_DOOR_DEMO`, prompt Enter) into a plain 14x10 shared room, `instance:workshop` (lamp, six benches, worktable + stools, crates, rug, one sign "Sawdust and a bench. Nothing to make here yet."). Client only: no crafting, no items, no new art, no daemon/civic/nginx/schema change (daemon treats `instance:` keys as opaque). `world_scatter_unit` 15 -> 27 checks, incl. a flood fill of the room that caught a sign sealing a corner tile on its first run; real-browser guest test enters/reads/leaves, presence lands in `instance:workshop` and rekeys back; mine and demo house regression-checked. Not exercised: two clients, a bound player. **E: Privacy/Terms left as is** (a lawyer's job, not a rewrite). Also corrected the stale parked list: `rebind()` and orphan reaping were already built and deployed 2026-09-19. `TheCommons/workshop-built.md`.
- **2026-09-20 -- Commons coherence/fun review (analysis only, nothing built; `TheCommons/CLAUDE_COHERENCE_QUESTION.md`).** Findings: nothing to do after ~5 min (11 never-consumed lookups); the world is empty at real traffic (nginx: guest mints from 1-6 distinct IPs/day, incl. our own testing); the civic tie is one-way (Hall is a link, the Square notice post is inert); the 11 items are all historical ink/pigment chemistry but go nowhere; no audio, no first-visit guidance; oak gall and oak-gall tannin glyphs are pixel-identical. Options A polish+guidance (client), B visitors' book with preset stamps (daemon+client), C1 workshop swatch-combining, C2 materials unlock civic-canvas colours (civic), D nothing. Recommendation A, then B, C is Brandon's call. Awaiting his letters.
- **2026-09-20 -- the Studio SPEC written (nothing built; `TheCommons/studio.md`, review request `CLAUDE_STUDIO_QUESTION.md`).** Brandon's direction, in his answers: the point of the game is things to DO; realistic chemistry (real ingredients, roughly real times, no quantities); regions become material sources; pigments you make are your palette; the self-portrait lives in the GAME (not civic's `/me/portrait`) and starts in charcoal, unlocking colours; materials are consumed, sources renew; free starter charcoal stub. Civic is untouched (its drawing tool has unlimited colours + image import, so it cannot be gated). Reopens knowingly: crafting ban, 'no stoichiometry/safety', counts-never-go-down, 'no menu'. Guests stay RAM-only (gather + session charcoal drawing); making/keeping needs an account. Five slices (daemon core, world sources, bench, easel+portrait, tuning), each its own BUILD. Open: time scale (real <=3d, 7d cap), recipe review owner, painting visibility (friends only). Recipe table is UNVERIFIED and must be reviewed before it is built. Waiting on Grok's `GROK_STUDIO_ANSWER.md` and Brandon's sign-off.
- **2026-09-20 -- Studio spec v2: Grok's review folded in (`TheCommons/studio.md`; nothing built, no catalog rows, no BUILD).** Applied: discovery = you hold every input (free-form side mix is rate-limited, cannot discover); homemade vinegar cut (Orchard keg instead); whiting -> shell white; alum crystals dropped; woad rewritten as a simplified lake; painting never refunds; server rules tightened (transactions, UNIQUE gather, no client timestamps, palette indices only, poll rate, orphan reap extended, guest path local); slice order now 1 daemon core, 2 charcoal easel + portrait + phone layout, 3 world sources + bench + notebook + lab conversion (one deploy), 4 rest of ladder, 5 tuning. My three deviations from Grok, flagged: pixel-level painting cost (his net-count formula allows free repaints), portrait GET visit-checked from slice 2, charcoal 8 h with the real-world figure disputed (2-4 h vs ~10 h). Recipe marks are Grok's and still unverified: I reference-check each line before `catalog.sql`, then Brandon signs the frozen table. Holding for that sign-off.
- **2026-09-20 -- Studio reference pass done (`TheCommons/studio-refs.md`; nothing built, no catalog rows, no BUILD).** Grok accepted the three v2 deviations and pinned pixel index 0 = empty. Verbatim, greppable sources (Cennini/Herringham, Merrifield + Bolognese MS., Theophilus/Hendrie, Pliny 35.25): charcoal, red ochre, carbon ink, verdigris, weld lake confirmed; shell white, gum, tannin, iron-gall, madder, woad confirmed with corrections; iron acetate and several game times unsourced. Cennini ch. 33 does give a clock ("till morning", overnight), which supports 8 h. Woad source has no ash. Six proposed corrections C1-C6 await Brandon; the notebook shows a real range only where a source gives one. Not verified: Coates 10 h, shell-white weathering; CAMEO was 403. Holding for sign-off.
- **2026-09-28 -- PresenceStore clock injected (was: reaped on real wall clock while `Occupant.last_update_ms` was stamped by main.cpp's test-clock-aware `now_ms()`).** `PresenceStore(Clock)` in `presence.hpp`; main.cpp passes `now_ms`; default (empty) = real clock. Constraint now documented in the header. `test_studio.sh` workaround removed and a reap-follows-test-clock check added (confirmed it FAILS on the old code, passes on the fix; full 26-file suite green). `guest.cpp` still reads the real clock for its own TTLs, but it stamps and reaps internally so it is self-consistent -- no trap there. Uncommitted.

## Split: civic site vs game

- **2026-09-16 — decided.** Persistent FireRed/Sims/AC-style world is **not**
  grown inside `liquidgov_http` / LGP1. Two programs, one person.
  - LiquidGov keeps: login, friends, mail, votes, profile canvas, Juketube, civic pages.
  - Game keeps: tiles, positions, houses, inventory, lab, mining, visiting.
    Own process, own files. A game write must not take down Genesis.
  - Glue only: session cookie, friends as a *query* (not a copied graph),
    `/proposals` as an in-world door, blob+feed for photo mode. Game stores
    `lg_username` as a reference, never a second login.
  - Juketube-shaped hangout on this droplet (one screen, ~2 Hz, RAM presence)
    is a waiting room, not the concept. Don’t sell it as Animal Crossing.
- Chemistry: real *names and reaction classes* as a lookup table is fine.
  Real stoichiometry/safety is a different product. Mining without heat/grade/alloy
  is named fantasy tiers. Neither is a reason to pick this stack.
- Profile canvas = bitmap to hang on a wall. House layout =
  `{item, x, y, rot90}` rows. Don’t reuse the canvas as the house.
- **Do not start the game in this repo until Brandon explicitly says so.**
  When it starts, it is a sibling process + SQLite, not new LGP1 tags.

## Data: what lives where

- **2026-09-16 — decided.** Normalize catalogs and ledgers. Don’t NoSQL the civic site.
  - LGP1 stays the civic store. It is a tagged dump, not a database.
    Do not add game tags. Do not migrate LiquidGov to Mongo “to modernize.”
  - Game ledger (inventory, placed objects, recipes, trades): SQLite,
    ~3NF catalogs / 2NF instances. Trades are `BEGIN/COMMIT`.
    LGP1 cannot roll back a half-write.
  - Hot presence (position, who’s in the plaza): memory (or Redis later).
    Snapshot on timer/leave, not 10 Hz flushes.
  - House snapshot JSON is a *cache* derived from `placed` rows, not the only copy.
  - NoSQL that *does* fit: key–value for live presence; a document for a house
    blob you always load whole. Not the inventory ledger. Not a second friend graph.
  - If a column can be derived from LiquidGov or from `items`, don’t store it as truth.

## Messages home-screen app

- **2026-09-16 — confirmed live.** Checked droplet: `src/http_server.cpp` md5
  matches the WSL working copy exactly, `liquidgov_http` binary built the same
  day, service active. Curled production: `/mail` renders via
  `page_wrap("Messages", ...)` with `mail_install_hint()` in its own body (not
  site-wide), `manifest-mail.webmanifest` serves `start_url:"/mail"` live, and
  `/app/mail` already carries the Messages-specific `apple-mobile-web-app-title`
  / icon / manifest. Nothing left to implement — this item is closed.
  - Original ask, for context: iOS “Add to Home Screen” captures the
    **current URL**, so `/apps` → `/app/mail` (install copy) → icon would open
    the install page, not `/mail`. Fix was to nudge the install hint from
    inside live `/mail` itself with page-specific manifest/icon/title, which
    is what's now confirmed running.
  - Already-saved wrong icons from before the fix cannot be patched; those
    users delete and re-add from `/mail`.

## Standing product pins (copy, don’t reopen)

- Claude is sole droplet implementer of `/opt/liquidgov`. Grok: GitHub + prompts, no SSH.
- `lg-deploy` rsync as root left `/opt/liquidgov` `root:root` → store write SIGABRT.
  Directory must be `liquidgov:liquidgov` after rsync. Never rsync `*.lgp`.
- Friends are mutual accept. No age checkbox. Phone is signup binding, not login.
- Platform community ≠ AVND locality. No React/Vite/Node UI.
- Do not put game world state in LGP1 or `http_server.cpp`.
