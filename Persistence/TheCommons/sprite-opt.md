# Sprite optimization — The Commons already has the ones that matter

Explore, not a BUILD. Slice 1 is still "paint the remaining tiles + tighten
the 16×24". Do not read this as permission to add frames, GPU, or a PNG
pipeline.

## What the client already does (keep)

- One atlas, painted **once**, then `drawImage` per actor per dirty frame.
- 4 directions × 2 walk frames, 16×24. Integer dest, `imageSmoothingEnabled =
  false`, integer CSS scale (`art.md`).
- Idle-frame skip: no blit if nothing moved, faced, or toasts.
- Caps: 24 / region, 96 global. A batcher for 3 sprites is theatre.

Canvas 2D has **no instancing**. 24 `drawImage`s from one atlas is the
fast path. WebGPU/WGSL notes already said no.

## What actually helps at this scale

| Do | Don't |
|---|---|
| Keep dest x/y **integers** (subpixel = blur + extra work) | `ctx.filter`, shadows, `globalAlpha` per sprite |
| 1px transparent pad in the atlas if you ever scale non-integer | Pack frames flush then draw at 3× (bleed) |
| One atlas for player + passerby + occupants (same look) | A canvas per occupant |
| Skip the draw when `dir`/`anim`/`x`/`y` unchanged (already) | Rebuild the atlas every walk frame |
| `alpha: true` **only** on the sprite atlas (bodies need it) | `alpha: true` on the world bake |

`putImageData` is slower than `drawImage` of a baked atlas. Don't go back.

## Fail if "explored" by

- 8-frame walk, skeletal animation, normal maps
- A sprite batch library, Pixi, Phaser
- Texture atlases compiled by a toolchain (no build step, `wasm.md`)
- Changing slice 1 into a PNG export job

Slice 1 still: more `paintTile` ids + a clearer 16×24. Same two-frame walk.
