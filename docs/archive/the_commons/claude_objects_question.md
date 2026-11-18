# Question — "finish populating the game with objects": which objects? (no code written)

Brandon asked: *"can we finish populating the game with objects?"* No doc specifies an unfinished object list, and the locked specs point in
different directions, so this is one question. One letter unblocks the build. Nothing here touches the droplet.

## Facts (repo at `9bf2b84`; live client = bd50119)

- **The world is dressed to spec.** All 12 outdoor regions have their `atlas.md` set piece plus collision (P1–P3, `world.js` `dressRegions`);
  `atlas.md` says each is "lightly dressed" on purpose (e.g. Workshop Lane = one facade, a bench, a barrel, a crate). There is no
  "unfilled region" in the docs.
- **The item catalog is 3 rows, by design:** `copper_acetate_bottle`, `iron_gall_ink_page`, `chalk_block`, from 3 lab recipes with no inputs
  (`seed.sql`, `pushback.md`: "real-name lookups, not a crafting economy"). The lab interior has 3 stations that give them; the house has a
  table, two chairs, a rug and one wall slot; the "mine" and the placeable/wallable/`placed` flow exist (`house_policy_built.md`, `mine_built.md`).
- **There are no item sprites.** `art.md` locks a figure-only atlas, so items are drawn as a flat colour square plus a short label
  (`interiors.js` colour table). New objects can follow that, or be drawn as tiles like the barrels and crates.
- **Seeding gotcha:** `db.cpp` runs `seed.sql` only when `items` is empty (its own header). The live database already has the 3 items, so new
  catalog rows need a small **migration** in the daemon (there is a migration test harness) and therefore a daemon deploy, not just a client one.
  Pure world/tile dressing is client only.
- The client does not fetch catalogs from the daemon (`deploy_plan.md`): the item colours and lab recipes are baked into `interiors.js`.

## What "objects" could mean

| | Meaning | Touches | Cost / risk |
|---|---|---|---|
| **A. More lookups** (recommended) | More real-name lab items and stations, e.g. lime water, verdigris, vinegar, soot ink, alum, gum arabic (I'd propose 6; you strike). Each is a catalog row, a recipe row, a colour and label, and a lab station. | daemon seed + migration, client, tests | Smallest surface; content is yours to choose. Needs a **daemon** deploy. |
| **B. More scatter in the regions** | Extra barrels, crates, signs, flower beds, stumps, benches, sitting or blocking. | client only (`world.js`) | Cheap and safe, but `atlas.md` asked for "lightly dressed"; more props change the look you locked. |
| **C. Furniture for houses** | More placeable pieces (shelf, lamp, plant, bed) for the per-player house, with the wall slot as is. | catalog, `placed` flow, client, tests | Uses the existing place flow; needs new catalog rows, so migration + daemon deploy. |
| **D. Pick-ups lying in the world** | Objects you walk up to and collect (shells at the Shore, stones at the Quarry). | new mechanic: daemon, client | Biggest: a new interaction and rules (respawn? shared or per player?). `pushback.md` argued against a resource economy. |
| **E. Something else** | Tell me the picture in your head. | | |

## My recommendation

**A, with a list you sign off first.** It is the only reading that continues an existing spec (the lab lookups), it stays inside "no crafting
economy", and every row is real chemistry. I would send the 6 candidate names with one-line facts for you to keep or cut, then build only what you keep.
If you meant the world looks sparse, that is **B**, client only, and I would show it in a screenshot per region before touching the files.

## If A, the shape (so the answer is one letter)

Catalog + recipe rows, the migration (idempotent; the seed for fresh databases stays in step with it), lab stations and colours in the client,
tests (a migration on an old-schema fixture, a fresh seed, the client catalog matching the seed slugs), then **hold for review**. Deploy is two
words from you: the daemon first (backup, plan, `--go`), the client after, as before.

## Answer needed

One letter (A / B / C / D / E) and, for A or C, "your list" or "use yours".
