# Property coherence: one door that is yours, and a visit that shows their plot — proposed

Brandon, verbatim: "I want my own outdoor property that I can customize, and I want to be able to visit a
friend's property, or go into town on the main map or walk around and collect stuff." He then hit three
things in the live game that make that sentence feel untrue. This doc picks one fix for each. It does not
rebuild gather, and it does not redesign the shop.

## What's already there, checked in the current code

- **Your plot is a 16x16 instance, not a patch of the outdoor grid.** `ownHouseSpace` (`interiors.js`) is
  built with `fillProperty`: grass, a hedge border, the door, the wall slot, the latch, the two frames.
  The table, chairs, rugs, shelves, lamp, and flowerbed are not in it. You enter it from `houseDoor` at
  tile (25, 47) on Homestead Row (`world.js`). The daemon stores whose it is (`instance:house:<name>`).
  There is still no per-player tile grid. Phase 1 found that, and it is still true: one shared `tiles`
  array for the outdoors, and a room template baked once on the client.
- **The demo house is a second room, and it is what a visit actually opens.** `fillHouse` still builds
  the furnished room (`T_TABLE`, two `T_CHAIR`s, rugs, workbenches, crates, lamp, flowerbed) into
  `demoHouseSpace`. The door at (26, 47), one tile right of yours, is `demoDoor`, prompt `Enter`.
  Nothing on screen says what it is. `tryInteract` (`interact.js`) enters `demoHouse` for that door, and
  enters `visit` with **the same `demoHouseSpace`** for a friend. A friend's grass, hedge, and latch are
  never what you walk into. Their placed things are drawn on top of the furniture, because floor items
  are data and the room underneath is this template.
- **Homestead Row is eight doors in one wall** at y=47. Yours at x=25 (`T_DOOR_HOUSE`, prompt `Enter`,
  so the button reads "Enter · Enter"). The demo at x=26. Six friend slots at x=29, 32, 37, 40, 43, 46,
  baked as boarded, prompt `Away` until `GET /presence/lots` fills one. A filled slot is not a new tile.
  `main.js` draws the house-door sprite and the friend's name over the boarded tile, and only then. An
  empty slot has no name. With one player online, six of the eight doors are a locked facade.
- **The lots list is already only people who are here.** `/presence/lots` returns mutual friends who are
  online and not locked, then online `open` houses, six at most. It does not return placeholders. The
  six boarded doors are the client drawing a slot where the list had no name.
- **Town and gathering are already on the outdoor map.** The Square is region (1, 1), notice-post and
  two benches, directly north of Homestead Row. `CLAUDE_PROPERTY_PHASE5.md` (named, not built) puts the
  shop door in that Square. North Woods, Quarry Ridge, Creek, Shore, and the Dunes already have gather
  spots. That half of his sentence is done.

## Decision 1 — take the demo door off the wall, and stop visiting the furniture

The furnished room was kept so a new player could see a lived-in example. Nothing labels it, it sits
against your own door, and it is the room a friend is shown instead of the plot you actually built. An
example that replaces the real thing is not an example.

Remove the `demoDoor` at (26, 47). That tile becomes wall, like the rest of the facade. Nothing in the
game enters `demoHouseSpace`. Leave `fillHouse` in the file. Phase 1's tests pin that the unused
template is still furnished, and deleting it is a cleanup, not this fix.

A visit enters a new space built the way `ownHouseSpace` is built: `fillProperty`, grass, hedge, door,
wall slot. No latch, and no easel. The latch is the owner's. The frames still draw for a visit, and
Enter on them still only toasts, which is already how `interact.js` treats `mode === "visit"`. Fence
posts still block, because `actors.js` already treats `visit` like `house`. What changes is the room
under those things. You see their plot.

## Decision 2 — a door only exists when a person is behind it

The six friend tiles are baked as wall, not as boarded doors. `findNearestInteractable` ignores a lot
whose `friend` is null, so an empty slot does not say `Away` and does not say `Enter`. When
`/presence/lots` names someone, the overlay that already draws their door and their name still runs, and
the prompt is their name. The button reads "Enter · alice", and the nameplate above the door is the same
name it already draws.

Yours stays where it is. Its prompt becomes `Your plot`, so the button reads "Enter · Your plot" instead
of "Enter · Enter". A guest still gets the login toast and does not go in. Guests never poll lots, so
they see one door and a wall, which is the truth.

The server list is unchanged. Six is still the cap. Open houses still use a leftover slot. This pass
does not invent a directory of offline friends, and it does not keep a dark door "so the row looks
full."

## Decision 3 — the plot stays an instance. A private patch of the shared map is not this game

He asked for an outdoor property, and the box fails that if "outdoor" means a rectangle of `world.js`
that only he can change. That cannot be done with the grid this game has. Outdoors is one `tiles` array.
Collision, paths, scatter, the camera, and presence all assume that one array. A plot that is yours and
not also everyone else's needs a tile grid stored per player. There is no such store. Building one is a
different project from anything in phases 1 through 4, and this doc does not start it.

What he can have, and already almost has, is a grass plot reached from the outdoor map. Homestead Row is
on that map, beside the Square. You walk there, you go through your door, you place things, you walk
back out. A friend does the same and sees your grass, not a spare living room. Calling that a room with
grass wallpaper was fair while a visit opened the furniture. After decision 1 it is the property.

## Phase 5

`CLAUDE_PROPERTY_PHASE5.md` is the town. The shop stays a door in the Square, coins stay a server-written
column, and the sell list stays the ten-minute gathers. This doc does not move the shop, reprice it, or
add a second town. "Go into town on the main map" is the Square plus that door. "Walk around and collect
stuff" is the gather spots that are already live. Neither is rebuilt here.

## Out of scope, said plainly

No per-player outdoor tile storage. No new region. No change to gather, the bench, the hammer, the floor
cap, or pickup. No change to `/presence/lots` or to who is allowed through a friend's door. No offline
mailbox and no player-to-player trade. No shop. `fillHouse` stays in the file, unused. The Dunes stay
as they are.

## Tests

- `client_files_unit.js`: Homestead Row has no `demoDoor`. (26, 47) is wall. The house door prompt is
  `Your plot`. A lot door with `friend === null` is not what `findNearestInteractable` returns when you
  stand on it. A lot door with a friend set uses that name as its prompt. The space a visit enters is
  grass and hedge, and it does not contain `T_TABLE` or `T_CHAIR`. `ownHouseSpace` is unchanged.
- No new daemon test. `/presence/lots` is not modified.

## Deploy

Client only. No schema, no route, no catalog row. Holds for a literal `deploy`, same as always.

---

**Say `BUILD property coherence` to build this as written, or tell me what to change first.**
