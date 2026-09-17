# Atlas — The Commons

Sibling spec, not the game binary. Locked facts (title, region names/order,
Homestead = gate, GBA/FireRed tiles) come from `Persistence/notes.md` → Game
heading and are not reopened here. Region **size** and **camera** below are
Claude's rewrite of `GROK_BET.md`'s numbers — see `pushback.md` for why.

## Grid

Uniform **24×20 tiles** per outdoor region (down from Grok's 32×32 — see
pushback.md). 3×4 supermap = **72×80 tiles** total, one continuous canvas,
soft seams (paths continue edge to edge, no per-region border dressing that
would show a seam).

Grid coordinates below are `(col,row)`, 0-indexed, matching the locked 3×4
layout. Tile AABB = `[col*24, col*24+23] × [row*20, row*20+19]`, used verbatim
as the presence region id in `presence.md`.

| Region | Grid | Tile AABB (x,y) |
|---|---|---|
| North Woods | (0,0) | 0–23, 0–19 |
| Lookout | (1,0) | 24–47, 0–19 |
| Orchard | (2,0) | 48–71, 0–19 |
| Workshop Lane | (0,1) | 0–23, 20–39 |
| the Square | (1,1) | 24–47, 20–39 |
| Hall Lawn | (2,1) | 48–71, 20–39 |
| Lab Yard | (0,2) | 0–23, 40–59 |
| Homestead Row | (1,2) | 24–47, 40–59 |
| Garden | (2,2) | 48–71, 40–59 |
| Quarry Ridge | (0,3) | 0–23, 60–79 |
| Creek | (1,3) | 24–47, 60–79 |
| Shore | (2,3) | 48–71, 60–79 |

"Lightly dressed" means, everywhere below: one set piece cluster occupying a
minority of the region, open walkable ground for the rest. Nowhere in V1 is
wall-to-wall decoration — empty reads as "quiet town," not "unfinished,"
because the loop is visiting, not exploring for content.

## The 12 outdoor regions

### North Woods (0,0)
- **Job:** quiet edge wander, no transaction, optional scenic loop.
- **Set piece (1):** one large tree/stump cluster with a sit-log.
- **Doors/warps:** none. Continuous edges: south → Workshop Lane, east → Lookout.
- **Collision:** solid tree line along the outer (north) map edge as a hard
  world wall; scattered solitary trees as soft obstacles; open grass through
  the middle third.
- **Lightly dressed:** the one tree cluster plus a handful of solitary trees;
  majority open grass.

### Lookout (1,0)
- **Job:** vista / photo-op beat. Encourages standing still and looking.
- **Set piece (1):** a raised plateau (walkable, +1 visual layer) with a
  railing and a bench, facing south toward Shore's sightline.
- **Doors/warps:** none. Continuous edges: west → North Woods, east → Orchard,
  south → the Square.
- **Collision:** railing solid along the bluff drop edge; plateau and
  surrounding grass open.
- **Lightly dressed:** plateau + railing + bench + one tree; rest open.

### Orchard (2,0)
- **Job:** soft secondary green space, "sit and be seen."
- **Set piece (1):** 4–6 decorative fruit trees in loose rows (no farming,
  no harvest interaction).
- **Doors/warps:** none. Continuous edges: west → Lookout, south → Hall Lawn.
- **Collision:** trees solid, planted with wide walk lanes between rows.
- **Lightly dressed:** one orchard block over roughly a quarter of the
  region; rest open grass/path.

### Workshop Lane (0,1)
- **Job:** implies a future crafting building, currently closed — honesty,
  not a teaser countdown.
- **Set piece (1):** closed-shutter storefront facade + a bench outside.
- **Doors/warps:** the shutter is decorative; interacting shows "Closed for
  now" and nothing else. No interior exists.
- **Collision:** building footprint solid along one edge; lane and bench open.
- **Lightly dressed:** one facade + light street scatter (a barrel, a crate);
  rest open lane.

### the Square (1,1)
- **Job:** the hub. Four-way junction to Lookout (N), Hall Lawn (E), Workshop
  Lane (W), Lab Yard (S). Highest expected concurrent presence.
- **Set piece (1):** a low fountain or civic notice-post, centered, something
  to stand near.
- **Doors/warps:** none. Continuous edges on all four sides.
- **Collision:** fountain/post center solid, walkable ring around it;
  otherwise fully open with wide radiating paths.
- **Lightly dressed:** fountain + two benches at opposite edges is the whole
  of it. Emptiness here is deliberate — a plaza holds people, not scenery.

### Hall Lawn (2,1)
- **Job:** solemn approach to the Hall door. Pewter, not a casino marquee.
- **Set piece (1):** the Hall door itself, framed by two low hedges/pillars.
- **Doors/warps:** **Hall door** — interact prompt "Hall" → one-line confirm
  → opens `https://liquidgov.us/proposals` in a new tab. No interior exists
  in V1; the door on the lawn *is* the entire interaction.
- **Collision:** hedge border solid; small open forecourt in front of the
  door for a couple of visitors to stand together.
- **Lightly dressed:** door + hedge frame is the entire content; rest is
  quiet lawn.

### Lab Yard (0,2)
- **Job:** gateway to the one indoor content beat — three real-chemistry
  lookups (see `art.md`/lab section below and pushback.md for the recipe
  rewrite).
- **Set piece (1):** the lab building exterior + a small sign.
- **Doors/warps:** **Lab door** → warps to Lab interior (20×16).
- **Collision:** building footprint solid; door tile is the walk-in trigger.
- **Lightly dressed:** one building + a couple of exterior barrels/crates;
  rest open yard.

### Homestead Row (1,2)
- **Job:** the **gate**, not 12 lots. A short row of doors: door 0 is always
  the shared demo house; doors 1–6 are dynamic online/friends slots.
- **Set piece:** the door row itself is the content — no separate set piece.
- **Doors/warps:** 1 **demo house** door (always live, warps to the shared
  demo house interior, 16×16) + up to **6** dynamic doors. A dynamic door
  resolves to that specific bound player's house instance only if they're
  online/friended and bound; otherwise it renders boarded, matching Quarry
  Ridge's board motif for visual consistency (a closed door reads the same
  everywhere on the map). **In V1 preview, with no cookie glue yet (see
  glue.md), all 6 dynamic doors render boarded** — only the demo house door
  is functional. Overflow past 6 is explicitly not handled in V1.
- **Collision:** row of door-fronts solid except at the door tiles themselves.
- **Lightly dressed:** the row + doors is the whole region.

### Garden (2,2)
- **Job:** calmer counterpart to Orchard, reads as Homestead's front yard.
- **Set piece (1):** a flower-bed cluster + a low sitting wall.
- **Doors/warps:** none. Continuous edges: west → Homestead Row, north → Hall
  Lawn.
- **Collision:** flower beds solid (low box, not walk-through); sitting wall
  solid; otherwise open.
- **Lightly dressed:** one bed cluster + wall; rest open grass.

### Quarry Ridge (0,3)
- **Job:** the honest dead end. Communicates "not built yet" without being
  coy about it.
- **Set piece (1):** a boarded mine adit + a sit-rock + a small sign (e.g.
  "Closed."). No ore, no interior — matches the locked pin.
- **Doors/warps:** none. The adit is decorative and non-interactive beyond a
  look/read prompt on the sign.
- **Collision:** adit face solid; rock solid (interact-only, no seating
  animation needed for V1).
- **Lightly dressed:** adit + rock + sign cluster; rest open stony ground.

### Creek (1,3)
- **Job:** a visiting corridor, water as scenery, no swimming.
- **Set piece (1):** a small footbridge crossing the creek + one bench.
- **Doors/warps:** none. Continuous edges: north → Homestead Row, east → Shore.
- **Collision:** water tiles solid (no swim); bridge tile walkable across;
  banks open.
- **Lightly dressed:** one water ribbon + one bridge; rest open bank grass.

### Shore (2,3)
- **Job:** southern bookend to Lookout — a calm dead end for visiting/photo.
- **Set piece (1):** a short dock/pier into the water + a driftwood log to
  sit near.
- **Doors/warps:** none. Continuous edges: west → Creek, north → Garden.
- **Collision:** water solid past the dock edge; dock walkable; sand/bank open.
- **Lightly dressed:** one dock + one log; rest open sand/bank.

## Interiors (extra, not on the outdoor canvas)

Sizes and contents match `GROK_BET.md` where it wasn't overridden.

- **House (demo, and later per-player instances) — 16×16.** Contents: a
  table, two chairs, the wall slot (civic profile canvas — empty frame in
  V1 until glue), a rug. V1 ships exactly one interior *template*; the demo
  house is the only instance actually reachable pre-glue (see v1.md).
- **Lab — 20×16.** Contents: three lookup stations (see pushback.md for the
  rewritten third recipe) plus a workbench visual. Each station's output is
  placeable/wall-able, never consumed into a stoichiometry system.
- **Hall — skipped in V1.** The Hall Lawn door is the entire interaction;
  no interior file exists.
- **Mine — absent.** Quarry Ridge's boarded adit is a decorative dead end on
  the outdoor map, not a warp target.
