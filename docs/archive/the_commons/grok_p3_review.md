# Grok review of P3 (`commons/index.html`, c996a3d)

P3 is the right chunk. Twelve outdoor regions are now dressed. Do not
open a P4. Glue / droplet / sprites are not this file.

## Accept

- North Woods: north tree line + solitary trees + 2×2 cluster with sit-log
- Lookout: walkable plateau, solid railing, bench, one tree off it
- Orchard: six trees in two loose rows, no harvest
- Workshop Lane: wall facade, shutter (`T_DOOR_BOARDED` + type `shutter`)
  toasts "Closed for now.", bench, two crates, no interior
- Garden: flower-bed cluster + low sitting wall, both solid
- Quarry Ridge: adit decorative (not an interactable), sit-rock, sign
  "Read" → "Closed."
- Same tileset, no new interiors, no SQLite, civic tree untouched
- P0–P2 pins held (notice-post, passerby, walk-in lab, RAM inventory)

## Parked (not a new patch unless Brandon wants them)

- On-screen button caption is still hardcoded `Enter`. Prompts are
  Try / Read / Look / Closed. Function is correct (nearest wins).
- Lookout railing is at `y=10`. Landscape crop is 10 tiles tall, Square
  starts at `y=20`, so desktop does not actually see the Square from
  the vista. Move plateau/railing south (`railing ≈ y=18`) if we reopen
  dressing. Portrait can already see further south.

## Fail if “fixed” by

- A Workshop or Mine interior
- Harvest on orchard trees
- Adit as a door
- IndexedDB / localStorage inventory
- Touching `liquidgov-platform/`

P3 is done. Outdoor V1 definition of done is met.


---

## Follow-up (138d5ae) — accepted

Claude read `sibling.md` correctly: droplet work is a new BUILD, not a
P4 in `index.html`. Held production.

Both parked nits closed:

1. `interactBtn.textContent` follows `nearestInteractable.prompt`, else
   `"Enter"`. Button still hidden outdoors when nothing is in range.
2. Lookout plateau `y=10–15`, railing `y=16`, bench on the plateau, tree
   still off it at `(28,13)`. Landscape crop from the rail reaches Square
   `y=20`.

No remaining P3 nits. Glue / sprites / sibling still wait on Brandon.
