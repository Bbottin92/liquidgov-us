# Tilemap — P0 rendering notes (Grok, for Claude)

Not a new lock. Spec sizes stay: 16×16 tiles, 72×80 supermap, dual crop
15×10 / 10×15. This is how to draw that without wasting time.

## Scale (so we don’t “optimize” the wrong thing)

| Thing | Size |
|---|---|
| Tiles on the world | 5,760 |
| Pixels if fully prerendered | 1152×1280 ≈ 5.6 MB RGBA |
| Tiles on screen | **150** landscape / **150** portrait |
| Draw calls that matter | one `drawImage` of the crop, per frame |

72×80 is a town, not a streaming open world. No chunks, no quadtrees, no
WebGL, no tile compression, no 12 canvases, no DOM-per-tile.

## Do this in P0

**1. Data.** One `Uint8Array(72 * 80)` of tile ids. Index `y * 72 + x`.
A second `Uint8Array` (or a high bit) for collision: 0 walk, 1 blocked.
Region id is **not** stored per tile — compute from AABB (`x/24`, `y/20`).

**2. Bake once, blit the crop.** Offscreen canvas 1152×1280. Paint every
ground tile **once** (colored rects are enough in P0). Each frame:

- camera pixel = player pixel − viewport/2, then clamp to `[0, 1152-vw]` × `[0, 1280-vh]`
- `drawImage(baked, camX, camY, vw, vh, 0, 0, vw, vh)` on the logical canvas
- integer-scale that canvas to CSS (3× or 2×)
- draw the player sprite **after** the blit, in viewport space

Do **not** loop 72×80 tiles every frame. Do **not** loop 150 `fillRect`s
every frame either once the bake exists. Viewport culling of live tiles
is the fallback if you skip the bake; the bake is simpler.

**3. Camera stays pixel-aligned.** Round camera to whole pixels (not
subpixel, not tile-snapped). Dual crop only changes `vw,vh`. Same bake.

**4. Path ribbon** is just tile id `path` on Square→Homestead→Creek→Shore.
Baked. No extra layer.

**5. Labels.** After blit, fillText the region name at its AABB center
minus camX/camY. Skip if the center is off-screen. Ink `#1a1814`, small.

## Don’t do this in P0

- Chunked loading / region canvases stitched at seams (fights continuous)
- Dirty-rect of individual tiles (150 tiles is already the whole frame)
- Sprite-batch WebGL
- Storing 12 maps and concatenating at runtime
- Animating water (would dirty the bake; P0 water is a static tint)

## When P1 lands

Set pieces that don’t move stay on the **bake** (re-bake that region’s
AABB, not the world). The player, passerby, and nameplates stay
over-blit. If water ever animates, a second tiny overlay for those tiles
only — not a second supermap.

## Failure modes (reviewer will check)

- Blit source rect not clamped → seams or empty bars at the world edge
- Scale via CSS `width` without `image-rendering: pixelated` → blur
- Camera in tile units mixed with pixel units → A still “works,” world
  crawls wrong
- Recreating the bake every frame → you didn’t optimize, you inverted it
