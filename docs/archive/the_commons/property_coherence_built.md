# Property coherence: a gate on the road south of the Square — built, not deployed

The gate was first put on the unnamed grass south of Creek. That walk had no name, and Homestead Row was a wall with nothing behind it. The wall came down. The gate is now on Homestead Row, the first region south of the Square, on the west half of the road. The east half stays path, so Creek is not behind the door. A sign beside it says the plots are through the gate. The banner says "your plot."

Brandon: `BUILD property coherence`, then "redo the map with better flow."

## What changed

- **Homestead Row** has no demo door, house door, friend doors, or facade wall.
- **The gate** is `T_DOOR_DEMO` at (34, 43). You stand on the path at (34, 42). The button says "Enter · Where". (35, 43) stays path.
- **The sign** at (36, 43) reads "Plots. The gate is on the road."
- **The list** is Home, then each name from `GET /presence/lots`. No Group line. A guest is told to log in. Home enters `ownHouseSpace`. A name enters `visitSpace`. The banner says "your plot" or "<name>'s plot".
- **A visit** is the grass-and-hedge plot, not `demoHouseSpace`.
- The road does not continue into the unnamed grass south of Creek. The daemon is unchanged.

## Tests

`client_files_unit.js` 155/0. `world_scatter_unit.js` 33/0.

## Deploy

Client only. Holds for a literal `deploy`.
