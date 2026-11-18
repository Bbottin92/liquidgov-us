# Tile collision — The Commons (P1)

Not a new lock. Player stays free-move (not FireRed tile-snap). This is
the rule set `commons/index.html` should implement, including the P1
patch sitting in `claude_p1_patch.md`.

## What we have now

- `Uint8Array solid` parallel to tiles. 1 = blocked.
- Player is a 12×12 AABB (`half = 6`) on 16×16 tiles, 76 px/s, axes
  separated (slide along walls). Good. Keep that.
- Query is **four corners** of that box. For a box smaller than a tile,
  overlapping a solid tile puts a corner inside it. Fine. Do not sample
  every pixel.
- Out of bounds = blocked. Then a second **clamp** to world pixel edges.
  Redundant but harmless.
- Passerby does **not** collide. It must therefore stay on walkable
  tiles by waypoint, or it moons over water.

## Solids (P1)

| Tile | Solid? | Why |
|---|---|---|
| water | yes | no swimming |
| path / bridge | no | ribbon through water |
| grass / dirt | no | |
| wall / hedge | yes | |
| bench | yes | sit-prop, bump it |
| notice-post | yes | one tile, not on spawn |
| boarded door | **yes** | “Closed.” is a wall with a prompt |
| demo door | **yes** | Enter warps; do not walk through |
| Hall door | **yes** | confirm is the interaction, not stepping in |
| fountain | gone | notice-post replaces it |

Interact stays Chebyshev ≤ 1 on the door tile (stand *next to* a solid
door). Do not auto-warp on overlap.

## The spawn rule (the fountain bug)

Never place a solid on the spawn tile, and never spawn overlapping a
solid. After dressing, assert:

```
!boxBlockedAt(player.x, player.y, player.half)
```

If that fails, move the prop, not the player. Square spawn stays region
center.

If the player is ever inside a solid (load, resize, future warp),
**push out** along the last move axis one pixel at a time; don’t freeze
all four directions. The fountain trap was “every corner already in
solid, so every candidate move is blocked.”

## Do not

- Tile-snap the player (this is a continuous outdoor canvas)
- Collide passerby with the player (they’re a label, not a body)
- Use `getImageData` / color-key collision
- Store region id on the tile for collision
- Diagonal “cut the corner” special cases — axis split already stops
  that if both adjacent tiles are solid

## Passerby

Waypoints are tile centers on **path or bridge only**. Update does not
call `boxBlockedAt`. If a waypoint is in water, they walk on water.
That’s a data bug, not a reason to add NPC collision.

## Reviewer fail

- Spawn overlapping solid
- Walk through a boarded door
- Passerby waypoint in water
- `solid[]` rebuilt from a table that still lists `T_FOUNTAIN`

## `dt` (see `dt.md`)

`frame()` clamps to 1/20 s so a tab-return cannot step ~28 px and
4-corner through a 1-tile door. Do not add CCD. If speed changes, derive
`DT_MAX = TILE / (speed * 2)`.

