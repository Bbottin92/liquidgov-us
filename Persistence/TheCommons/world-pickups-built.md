# World pickups D — oak galls, shells, apples. Built, not deployed.

`GROK_OBJECTS_ANSWER.md` D, after A went live. Daemon + client. **Not
deployed.** No civic, no nginx, no limestone, no mine pickup.

## What it is

Three outdoor lookups, same loop as the lab: Look, +1, never consumed,
per player. Bound inventory persists them; guests keep them in RAM.

| Where | Marker (existing tile) | slug |
|---|---|---|
| North Woods (17,6) | the solitary oak | `oak_gall` |
| Shore (62,73) | crate on a dirt beach off the bridge | `seashell` |
| Orchard (56,10) | flowerbed pile between the rows | `cider_apple` |

Trees are not harvested. Quarry Ridge is untouched.

## Catalog

`catalog.sql` INSERT OR IGNORE adds the three as `category 'material'`,
`wall_slot_ok` 0, no recipes. Live already has 8 lab rows; a restart
grows to 11. Fresh DBs do seed (3) then catalog (8) = 11.

## Client

`WORLD_PICKUPS` in `interiors.js` (slug check vs SQL). `world.js` plants
the three interactables in `dressRegions` before scatter. Shore gets a
2×3 dirt tongue off the bridge so the beach is not a sealed water pocket.
`interact.js` shares the lab pickup path.

No new tile ids, no `tiles.png` regen (`make_art.py` still NTILES=41).

## Tests

- scatter unit: 14/0, including flood-fill through the new beach
- client files: 55/0
- slug python: 8 lab + 3 world = 11, no lime, no limestone
- `test_migrations.sh` now asserts 11 items / 8 recipes on old-shape and
  fresh DBs (CI runs the daemon)

## Deploy (when Brandon says `deploy`)

Same command as A, new label:

```
cd /home/bbottin/lg
git pull
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label world-d
```

Daemon first (catalog rows), client second. Expect verify: items 8→11,
recipes unchanged at 8. Then: hard-refresh, Look at the North Woods oak,
the Orchard pile, the Shore crate.
