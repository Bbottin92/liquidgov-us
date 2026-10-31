# The Studio, slice 1: daemon core — built, tested, NOT deployed

Brandon: `BUILD studio 1`, the trigger `studio.md` itself names, sent after that file's own reference pass
(`studio-refs.md`) had already landed. **Daemon only** (`commons-daemon/`): schema, the frozen catalog, gather/bench/
mix/collect, discoveries, consumption, migration, the orphan-reap extension, tests. **No client, no droplet.**
`studio.md`'s own gate ("Brandon signs the frozen table and says `BUILD studio 1`") is treated as satisfied by the
trigger itself, since it came after the reference pass was posted — the exact reading taken is spelled out below and
in `studio.md`'s own "Open decisions" section, so it is easy to overrule before the next slice.

## What "signed" means here

The frozen table is the v2 recipe table **with corrections C1-C6 applied in full**, plus the three deviations
`studio.md` flagged for Brandon (pixel-level paint cost — not actually built until slice 2's easel; visit-checked
portrait from slice 2 — ditto; 8h charcoal, which this slice's `duration_s` does ship). C1-C6: no ash anywhere (the
charcoal by-product and the woad-blue ingredient are both dropped); the madder history line notes lake pigments are
later/northern; iron acetate stays, labelled explicitly folk and unsourced; unsourced game times say so in their own
note text (`real_range_note`); the "weathered for years" shell-white claim is gone; charcoal's history line is
Cennini's own "left overnight", not a "closed pot" (which the safety rule bans outright).

## Schema (`Persistence/TheCommons/schema.sql`, symlinked into `commons-daemon/`)

- `items` gains `pigment_hex` (nullable — set only on something the easel can actually paint with; NULL on a raw
  material or an intermediate like tannin, binder, iron acetate).
- `recipes` gains `duration_s`, `tier`, `real_range_note`, `history_note` — all NULL on every pre-Studio row (the 3
  seed lookups and the 5 Lab-A ones): a Lab-A recipe is a lookup, not a benchable batch, and the bench refuses
  anything with `duration_s IS NULL`.
- Four new tables, all additive: `batches` (a started bench batch; `started_ms`/`ready_ms` are server-clock only —
  the client is never trusted with either), `discoveries` (existence *is* the discovery; the PK makes a second
  collect of the same recipe a no-op), `gather_cooldowns` (per player, per fixed spot slug), `paintings` (declared
  now, unused until slice 2 — the orphan reap needed to know about it from slice 1, see below).
- Both a fresh database (the columns/tables are on the `CREATE TABLE` statements directly, same shape
  `inventory.updated_at` already used) and an existing one (`Database::ensure_studio_columns()` /
  `ensure_studio_tables()`, the same `ALTER TABLE` / `CREATE TABLE IF NOT EXISTS` pattern as
  `ensure_inventory_recency()` / `ensure_visit_policy()`) converge on the identical shape — proven in
  `test_migrations.sh` on a frozen old-shape fixture.

## The catalog (`commons-daemon/catalog.sql`, `INSERT OR IGNORE`, idempotent)

27 new items (11 raw materials, 16 pigments/intermediates) and 13 new recipes, all `station='bench'` — a different
mechanic from every Lab-A `'lab'` lookup on purpose (a timed batch, not an instant Look). Every real-world substance
that a Lab-A item already represents gets its **own, distinct** Studio item (`vinegar`, `tannin`, …): Lab-A's five
finished items stay untouched legacy trophies until slice 3's lab conversion (`studio.md`'s own locks table), so
nothing here silently repurposes them. `oak_gall` and `seashell` **are** reused as-is — they were always gathered
raw materials (World Pickups D), never a finished trophy.

Tiers 1-6, strictly increasing along every input->output edge except where two recipes sit in the *same* tier and
one legitimately feeds the other (yellow ochre -> red ochre; gum solution -> carbon ink and -> iron-gall ink) — a
graph query in `test_studio.sh` asserts no input item is ever the output of a **strictly higher** tier, which is
enough to rule out any real cycle (a cycle would need a back-edge, and a total order via `tier` cannot have one).
Every `recipe_inputs.quantity` is exactly 1 (`studio.md` decision 2: no stoichiometry — the game tracks "do you hold
this", never a ratio). Charcoal alone yields four outputs (black + three greys); every other recipe yields one.

Cut per `studio.md`: homemade vinegar from apples, alum crystals. Not added: Prussian blue, lime, malachite.

## Daemon API (all under `/presence/`, bound identity only, no guest fallback — `Unbound is 403 not_bound` per
`studio.md`'s own line; no nginx change, same reasoning as every other `/presence/` route)

- `POST /presence/gather {spot}` — `spot` is one of 13 fixed slugs in a static table (`studio.cpp`), never
  client-authored data. The cooldown set and the inventory grant are **one transaction**, race-safe via a single
  `INSERT ... ON CONFLICT DO UPDATE ... WHERE next_ms <= now`: a first-ever gather at a spot always succeeds (the
  INSERT branch has no `WHERE` to satisfy); a later one only if the cooldown has actually passed.
- `POST /presence/bench {recipe}` — refuses a recipe with `duration_s IS NULL` (`unknown_recipe`, same as a typo'd
  slug — the bench does not know Lab-A lookups exist). Slot cap **3** uncollected batches per player, checked inside
  the same transaction as every input deduction: a guarded `UPDATE ... WHERE quantity >= needed` re-checks stock
  *inside* the transaction (not just an earlier read), and the **whole thing rolls back**, including every input
  already deducted, the moment any single input comes up short — proven by mutation (below), not just asserted.
- `POST /presence/mix {slugs}` — up to 4 slugs (a 5th rejects the *whole* array, never silently truncates), matched
  against a sorted-multiset table of `studio.md`'s own fixed fail texts, else "Nothing useful comes of it." Never
  touches inventory, never returns a `recipe_id` — it cannot start a batch or discover anything. Its 1-per-5s rate
  limit is a plain RAM map in `main.cpp` (no mutex needed: this daemon is single-threaded and synchronous, same
  reasoning `HouseStore`'s own header already gives).
- `POST /presence/collect {batch_id}` — authoritative on the batch row's own `ready_ms`, never the client's claim.
  `collected` flips 0->1 exactly once (a guarded `UPDATE ... WHERE collected = 0`, so a second collect of the same
  id is `already_collected`, not a second grant, even if two requests raced). Grants every `recipe_outputs` row to
  the **shared** `inventory` table (the same one `HouseStore`'s lab pickups already write to — one inventory per
  player regardless of source) and records the discovery (`INSERT OR IGNORE`, so a later batch of an
  already-discovered recipe still grants materials but reports `first_discovery: false`).
- `GET /presence/studio` — inventory (via `HouseStore::inventory_of`, not duplicated), open batches, discoveries,
  active cooldowns, the unlocked palette (discovered **and** `pigment_hex` set — an intermediate like tannin never
  shows up here even once discovered), and every recipe this player can start right now.

## A test-only clock seam, added because it did not already exist

`studio.md`'s own data model section says "a test-only clock seam exists" — it did not; slice 1 had to build one, since a
batch can take up to 7 real days (verdigris) and no test may actually wait one. `COMMONS_TEST_CLOCK_FILE` (an env var
naming a file `now_ms()` reads on every call instead of the real clock) lets a test move time forward between requests
with no daemon restart. **Found the hard way:** the seam is global, so freezing it for one test's convenience also
freezes the orphan watch's *own* internal timing (`now_ms() - last_pass_ms` never advances) — `test_studio_orphans.sh`
first silently never fired a single orphan pass during a real 9-second sleep because of exactly this. That test now
runs on the real wall clock on purpose; `test_studio.sh` (no orphan-pass code in play) uses the frozen seam freely.

## Tests

`tests/test_studio.sh` (**88 checks**): the catalog's own soundness (tiers acyclic, no self-loop, no stoichiometry,
C1's `ash` fully gone — as an exact slug, not a substring: `seashell` legitimately contains "ash" and an earlier,
looser check flagged it as a false positive), every unbound route refused, gather (unknown spot, grant, cooldown,
release), bench (unknown recipe, a Lab-A lookup specifically refused, missing inputs, **a multi-input recipe holding
the first ingredient but not the second — the first is rolled back too**, the slot cap enforced independently of
material), collect (not ready, ready, first discovery, idempotent, ownership, unknown id, a second batch of an
already-discovered recipe), makeable-now appearing/disappearing with stock, and mix (fixed texts, the rate limit,
the cap of 4 rejecting the whole array).

`tests/test_studio_orphans.sh` (**10 checks**, its own file/civic/daemon — see the clock-seam note above for why):
an account's batch + cooldown are gone after reaping, a kept neighbour's are untouched, and nothing anywhere
references a `player_id` with no `players` row. **A real ordering bug found and fixed by this test itself**:
`main.cpp`'s orphan pass must call `studio.reap_orphan()` **before** `house.reap_orphan()`, not after — house's own
reap is what deletes the `players` row, and Studio's reap resolves its `player_id` by username. Reaping in the
wrong order does not silently leak, though: `PRAGMA foreign_keys = ON` makes the *players* delete itself throw (a
row with FK-referencing batches/cooldowns still attached) and the whole thing rolls back — a safe failure, but the
account would be stuck retrying forever. Mutation-checked: swapping the two calls back makes this test fail on
exactly the two assertions that matter (`reaped` / `players row is gone`), confirmed, then restored.

`tests/test_migrations.sh` extended: the new columns and all four new tables land correctly on a frozen **old-shape**
fixture, converging on the identical shape a fresh database gets; item/recipe counts updated (11->38, 8->21).

`tests/test_client_files.sh` fixed, not just extended: its own catalog-parity check asserted the client's known item
slugs were **exactly equal** to every catalog row — true before this build, but slice 1 is server-only by design, so
the daemon now legitimately has 27 items the client has never heard of. The assertion now checks the direction that
actually matters (every slug the client uses is a real catalog row) instead of exact equality; the reverse would
have permanently red-lighted CI for a slice whose whole point is "no client yet".

**Mutation-checked** (beyond the two above): the bench's per-input deduction loop, with its `sqlite3_changes() == 0`
early-return changed to commit the transaction instead of rolling it back — `test_studio.sh`'s new multi-input case
(scrap_iron deducted, then vinegar found missing) catches it immediately; restored and reconfirmed green.

**Full suite**: 25 files, 0 failed, after these fixes. Ports/processes swept clean between runs (this daemon's
`/tmp` is tmpfs and can be wiped by an idle WSL VM between long tool calls — hit this mid-build and re-ran the
affected steps from scratch rather than trust a half-finished directory).

## Limits, said plainly

- **No client anywhere.** Nobody can reach any of this from the game; slice 1 is proven only against the daemon
  directly (real compiled binary, curl-equivalent test helpers).
- **The gather-spot table is data, not placement.** All 13 spots (including the two Lab ones, so gather/bench/collect
  are fully testable now) exist server-side; nothing places a marker for them in the world until slice 3.
- **Pixel-level paint cost and the portrait's visit-check are not built.** Both are slice 2 (the easel does not
  exist yet); only the frozen-table reading that assumed them is recorded here.
- **The recipe/history text is accurate to the reference pass, not copy-edited for the notebook UI.** Nothing
  renders it yet; slice 2+ is free to reword without touching data.

## Deploy

`studio.md`'s own line: "every droplet touch needs the literal `deploy` word (daemon deploys: drift-check, online
DB backup first)." Not deployed; holding for review. When it is, this is a **daemon-only** deploy (no client sync at
all) — the standard recipe (drift-check, an online backup via `Database::backup_to` before the restart, the daemon
picks up the new schema/catalog via its own `ensure_*`/`ensure_catalog` on startup) with a live check limited to
`GET /health` and the new endpoints answering `403 not_bound` for an unauthenticated request, since nothing else can
exercise them without a bound test account.
