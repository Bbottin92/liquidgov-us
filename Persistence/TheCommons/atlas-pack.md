# Texture atlasing — two sheets, already

Explore, not a BUILD. Slice 1 stays `paintTile` + a tighter 16×24. Do not
turn this into a PNG packer or a TexturePacker JSON.

## What this game actually has

1. **World bake** — one offscreen canvas, all 72×80 outdoor tiles (and each
   interior) drawn **once**. That *is* the tile atlas. Camera `drawImage`s a
   crop. `canvas.md`. Do not also blit per-tile from a 16×16 sheet every
   frame; that is strictly slower here.
2. **Sprite atlas** — 4×2 cells, 16×24, painted once. Player, passerby,
   occupants share it. `art.md` / `sprite-opt.md`.

A third "master atlas" (tiles + sprites in one PNG) buys nothing: tiles are
already composited into the bake, sprites need alpha, the bake is
`alpha: false`.

## Strategies that do not apply

| Technique | Why not |
|---|---|
| Guillotine / skyline packers | ~10 sprite cells. Hand grid. |
| TexturePacker / hash JSON | No build step (`wasm.md`). |
| Multi-page atlases | One page of 8 frames. |
| Premultipled + NPOT tricks | Integer 3×/2× CSS scale, `pixelated`. |
| Per-region atlases | One continuous supermap. |

## If a PNG ever exists (unnamed)

One `tiles.png` (16×16 grid, ids in row-major matching the `T_*` enum) and
one `sprites.png` (4 dir × 2 frame). 1px transparent pad **only** if we
ever draw at non-integer scale. Bake still stamps from that sheet **once**
into the world canvas. Until that BUILD is named, `paintTile` *is* the
tileset.

Fail: a runtime packer, slicing a generated 1024 sheet, merging bake +
sprites onto one GPU texture.
