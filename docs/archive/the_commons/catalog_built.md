# Lab catalog A — five more lookups, no lime. **Live.**

Deployed 2026-09-19 by Brandon (`f8d111f`, `--label catalog-a`).
`grok_catalog_live.md`. Verify 33/0, items 3→8, recipes 3→8.


Brandon: "you just do it" after Claude hit the spend cap mid-A.
`grok_objects_answer.md`. Daemon + client. **Not deployed.** No civic,
no nginx, no SSH.

## What it is

Eight lab stations instead of three. Same loop: look up, +1, never
consumed, no crafting. Lime is not one of them.

| slug | line |
|---|---|
| `vinegar_jar` | acetic acid from fermented cider |
| `oak_gall_tannin` | oak galls, crushed and steeped |
| `lampblack_ink` | soot + gum arabic |
| `alum_crystal` | potassium aluminium sulfate, crystallised |
| `gum_arabic` | acacia resin in water |

## How the rows get onto production

`seed.sql` still has the original three and still runs only when `items`
is empty. Live already has those three, so new rows cannot come from
seed. `commons-daemon/catalog.sql` is `INSERT OR IGNORE` of the five,
run on every start by `Database::ensure_catalog`. A brand-new DB does
seed then catalog (3+5=8). A second start is a no-op.

## Client

Second row of stations in the 20×16 lab (x = 3,6,9,12,15 at y = 10).
Five new tile ids 36–40; grass variants stay 48–51. `tiles.png`
regenerated from `make_art.py` (now reads `commons/js/world.js`, which
is where `T_*` actually lives after the split). Cache buster
`tiles.png?v=2`. Wall-slot colours/labels added so a placed new item
is not a blank square.

## Tests

- catalog.sql applied twice on schema+seed → 8 items, 8 recipes, 0 lime.
- `test_migrations.sh` now asserts the 8 slugs on an old-shape DB and
  on a fresh one.
- `test_client_files.sh` requires `LAB_RECIPES.item` == seed+catalog
  slugs (8, no lime). Existing load-order unit still 55/0.

This sandbox has g++ but not cmake, so the daemon binary was not
rebuilt here. CI on GitHub is the compile + `test_migrations` gate.

## Deploy (when Brandon says `deploy`)

Daemon first: backup the live sqlite, sync (catalog.sql is in the
daemon tree), rebuild, restart, confirm `SELECT count(*) FROM items`
is 8 and no lime row. Client second (`js/world.js`, `js/interiors.js`,
`tiles.png`). Civic and nginx stay down. Rollback: previous daemon
binary; catalog rows can stay (INSERT OR IGNORE, unused stations
would just vanish from the client).
