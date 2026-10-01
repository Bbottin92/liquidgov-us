# Property coherence: a gate at the edge, and a visit that shows their plot — proposed

Brandon, verbatim: "I want my own outdoor property that I can customize, and I want to be able to visit a
friend's property, or go into town on the main map or walk around and collect stuff." He then described
the entrance he actually wants: a path to the edge of the map, and a gate that asks where you are going.
Home, a friend, or a group. This doc picks that entrance. It does not rebuild gather, and it does not
redesign the shop.

## What's already there, checked in the current code

- **Your plot is a 16x16 instance, not a patch of the outdoor grid.** `ownHouseSpace` (`interiors.js`) is
  built with `fillProperty`: grass, a hedge border, the door, the wall slot, the latch, the two frames.
  You enter it from `houseDoor` at tile (25, 47) on Homestead Row (`world.js`). The daemon stores whose
  it is (`instance:house:<name>`). There is still no per-player tile grid. Outdoors is one `tiles` array.
  Walking off the edge is already blocked (`actors.js` refuses a tile outside the grid). A gate has to be
  a tile on the map. It cannot sit past the edge.
- **The demo house is a second room, and it is what a visit actually opens.** `fillHouse` still builds
  the furnished room into `demoHouseSpace`. The door at (26, 47) is `demoDoor`. `tryInteract` enters
  `visit` with that same furnished room. A friend's grass is never what you walk into.
- **Homestead Row is eight doors in one wall** at y=47. Yours, the demo, and six boarded friend slots.
  `/presence/lots` already returns only people who are online (mutual friends, then open houses, six at
  most). The boarded doors are empty slots drawn as doors. A lit one already draws that person's name.
- **The game does not know about groups.** The daemon asks civic for `/internal/friends`,
  `/internal/blocked`, `/internal/session-user`, and `/internal/user-exists` (`civic.cpp`). It never asks
  which mail group or community you are in. Every property has one owner, and `place_floor` refuses
  anyone else (`not_your_house`). "Whoever is in the group can change it" is not a rule that exists yet.
- **The south edge under Creek is empty grass.** The spine is the path at x=34 and x=35, from y=20 down
  to `shore.y1` (79), and it was pinned there so it would not run into row 4. Row 4, column 1, is the
  undressed grass south of Creek. That is the edge.
- **Town and gathering are already on the outdoor map.** The Square is north of Homestead Row.
  `CLAUDE_PROPERTY_PHASE5.md` puts the shop there. The gather spots are already live.

## Decision 1 — a visit shows their plot, not the furniture

Remove the demo door. Nothing enters `demoHouseSpace`. Leave `fillHouse` in the file. A visit enters a
space built like `ownHouseSpace`: grass, hedge, door, wall slot. No latch and no easel. The frames still
draw, and Enter on them still only toasts. Fence posts still block. The room under those things is their
plot.

## Decision 2 — the doors come off Homestead Row. A path leads to one gate.

Yours, the demo, and the six friend slots all come off the wall at y=47. Those tiles become wall. A row
of doors was a directory. The directory moves to a question, which is what he asked for.

The spine continues south from y=80 on x=34 and x=35, onto the empty grass, and stops at a gate. The
gate reuses `T_DOOR_DEMO`. No new tile. You stand on the path tile south of nothing — the path tile just
north of the gate — and Enter. The button says "Enter · Where".

Enter opens a list, not a warp. The list is Home, then one line for each name `/presence/lots` already
returns. Home enters your plot. A name enters that person's plot, the grass one from decision 1. The
daemon still decides whether you are allowed in. A guest at the gate gets the login toast and no list.
If nobody else is online, the list is Home and nothing else. There is no "Away" row.

Walking back out puts you where you were standing, which is the path in front of the gate.
`exitToOutdoor(null)` already does that.

## Decision 3 — the plot stays an instance

The gate does not make the plot a rectangle of the shared map. It is still the grass instance. That is
the part that can be reached by a path. A private patch of `world.js`, different for each player, still
needs a tile grid stored per player. This doc does not start that.

## Decision 4 — Group is a real choice, and it is not this build

The list he described has a third word, Group. One shared plot, changed by the people in a group, not by
one owner. Two things are missing, and both are server work.

The daemon cannot ask civic who your group is. Mail groups and communities exist on the site. Nothing in
`civic.cpp` fetches them. And `place_floor` allows only the owner. A shared plot that only one member
can edit is not maintained by the group.

So this build's list is Home and the people who are online. Group is the next build, not a gray button
that does nothing: `BUILD property group`, one shared instance, a civic answer for membership, and
place/take allowed for a member. Not this `BUILD`.

## Phase 5

The shop in `CLAUDE_PROPERTY_PHASE5.md` stays the town. This doc does not move it. "Go into town" is the
Square plus that door. "Walk around and collect stuff" is the gather spots already on the map.

## Out of scope, said plainly

No per-player outdoor tile storage. No new region and no new tile. No group plot, no new civic route, no
change to who may place on a house. No change to gather, the bench, the hammer, the floor cap, or
pickup. No offline mailbox and no player-to-player trade. No shop. `fillHouse` stays unused. The Dunes
stay as they are. The six-name cap on `/presence/lots` stays.

## Tests

- `client_files_unit.js`: Homestead Row has no `demoDoor`, no `houseDoor`, and no `friendDoor`. The
  spine's path continues south of y=79 on x=34 and x=35, and a gate interactable sits on that path. The
  space a visit enters is grass and hedge, with no `T_TABLE` and no `T_CHAIR`. Choosing Home is the
  owner's plot. Choosing a name from the lots list is a visit. The list has no Group line.
- No new daemon test. `/presence/lots` is not modified.

## Deploy

Client only. No schema, no route, no catalog row. Holds for a literal `deploy`, same as always.

---

**Say `BUILD property coherence` to build this as written, or tell me what to change first.**
