# The rest of the map — built, not deployed

Brandon, playing it: no direction, no objectivity, no aesthetic, just randomness.
`3121a46` already put a one-tile branch on the Workshop, the Lab, the Hall and the
mine, and took the gather glyphs off the trees. This pass does not move those
branches. The bridge tile at (34,69) is still a bridge.

What was still true after that: the grass was a 1-tile checker, so a lane had no
edge. The Lookout, the Orchard and the woods were still open grass with nothing
leading into them. The Lab branch stopped at the spine, so the east doors and the
Garden were still off it. Hall Lawn and the Square still wore the corner-tree stamp
every region got. Ochre and alum were still two unmarked tiles south of the mine lane.

## What changed

`commons/js/world.js`

- Grass is one colour. The darker green is only the shoulder of a lane or the bridge.
- Lanes added, grass or dirt only, so a prop or the existing branches stay:
  - spine continued north to the tile under the Lookout railing, then east and north to the tile above the Orchard keg
  - that lane continues south down Hall Lawn and stops on the tile above the Hall door (the branch from `3121a46` still arrives from the west)
  - a woods track west of the Workshop facade, stepping around the tree at (4,17), meeting the Workshop crossbar; a spur stops beside the gall oak, and the track itself passes beside the twigs
  - the Lab/Homestead row continued east to the Garden beds, plus one step south to the tile above the woad
  - a spur south off the mine branch to the tile north of the ochre and the alum
- Scatter moved, not added (still 202, none refused): the Square's south corner trees up to the north edge; Hall's corner trees in line with the Orchard axis; Hall's four flowerbeds into a row in the Garden.

`commons/js/interiors.js`

- Workshop rug extended one tile toward the door and one toward the easel. Lab gets a rug down the open column from the door to the stations. The inventory-chip colours from `36730bb` are untouched.

## Checked

`world_scatter_unit.js` 31 checks, 0 failed. `client_files_unit.js` 105 checks, 0 failed.

## Left alone

Creek and Shore banks are still the old scatter. The bridge is the direction there.
No HUD arrow. No quest, no XP, no day-night.
