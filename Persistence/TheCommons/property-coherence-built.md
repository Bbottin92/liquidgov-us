# Property coherence: a gate at the edge — built, not deployed

Brandon: `BUILD property coherence`, approving `CLAUDE_PROPERTY_COHERENCE.md` as revised (`d0db810`).

## What changed

- **Homestead Row** no longer has a demo door, your door, or the six friend doors. Those tiles are wall. The spine through the wall is still path.
- **The south edge.** The path at x=34 and x=35 continues from y=80 through y=83, onto the empty grass south of Creek. The gate is `T_DOOR_DEMO` at (34, 84). You stand on the path at (34, 83). The button says "Enter · Where".
- **The list** is Home, then each name from `GET /presence/lots`. No Group line. A guest is told to log in and the list does not open. Home enters `ownHouseSpace`. A name enters `visitSpace`.
- **A visit** is the grass-and-hedge plot, not `demoHouseSpace`. No latch, no easel. The frames still draw, and Enter on them still only toasts. `fillHouse` is still in the file and still unused.
- The daemon is unchanged. The lots route is unchanged. Walking back out still returns you to where you were standing.

## Tests

`client_files_unit.js` 155/0. `world_scatter_unit.js` 33/0. The new path did not seal a walkable tile and did not take a scatter prop.

## Deploy

Client only. Holds for a literal `deploy`.
