# C1 furniture + C2 floor-place. **Live** with D (`ccdf06f`).

Deployed 2026-09-19 by Brandon (`--label objects-d-c`). `GROK_OBJECTS_LIVE.md`.

Brandon: "lets just finish this give me all of the stuff and deploy."
A is already live. D was built at `d010a05`. This commit adds C1 and C2
so one deploy ships D+C1+C2.

## C1 — same furniture in every house (client only)

Shelf (workbench tiles), lamp, crate-bookcase, a plant, a second rug.
Existing tiles, no new art, no daemon change. Demo house and your house
share `fillHouse`, so both get it. Visitors see it too.

## C2 — put the last pickup on the floor

- In **your** house: face a clear floor tile, Enter. Cap 8. Face the
  door or a wall and Enter still leaves.
- Stand next to a floor item, **Take**. It is not consumed from
  inventory; taking just lifts it so you can set it down again.
- Visitors see the items, cannot move them.
- Durable in `placed` (`is_wall_slot = 0`). The wall slot is unchanged.
- Demo house and lab stay wall-only RAM, as before.

## D (already in this tree)

Oak galls in North Woods, cider apples in the Orchard, shells on the
new Shore sand. Catalog 8→11 items, 8 recipes. No limestone.

## Deploy

```
cd /home/bbottin/lg
git pull
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label objects-d-c
```

Expect verify: items 8→11, recipes 8 unchanged. Then hard-refresh.
