# WebGPU compute shaders

Not a BUILD. Does not reopen WASM, sprites, glue, or sibling.

## The honest scale (again)

The display is **240×160 or 160×240** logical pixels, integer-scaled in
CSS. One `drawImage` of a baked 1152×1280 atlas. 150 tiles on screen.
Player + passerby are two sprites.

A compute shader has nothing to do. There is no particle sim, no light
cascade, no GPU culling, no SDF font, no 10k agents. Presence is 2 Hz
and 96 caps (`presence.md`). Collision is a CPU `Uint8Array`.

Canvas 2D is the renderer `art.md` asked for: nearest-neighbor, GBA
palette, `image-rendering: pixelated`. WebGPU’s default path is
linear/sRGB and a swapchain. You would spend the slice fighting it back
to 16×16 tiles.

## What people mean and why it misses

| Idea | Why not |
|---|---|
| Compute for tile lighting / day-night | Out of V1. A multiply on the blit is a 2D `fillRect` if we ever want dusk. |
| GPU collision | Worse than `solid[y*w+x]`. Readback is the slow path `tilemap.md` already banned. |
| Simulate 96 walkers on GPU | Cap is 96 because of the **droplet CPU**, not fill rate. |
| Upscale 240×160 with a sharpen shader | Integer 3×/2× CSS is the look. A shader would blur or oversharpen FireRed. |
| One WebGPU context “for the future” | Safari/iOS still the phone QA target. Canvas 2D is the portable path. |

WebGL was already a fail in `tilemap.md`. WebGPU is the same fail with a
newer badge.

## Where GPU compute *would* belong (not this product)

A different game: 3D, millions of particles, ML upscale. The Commons is
a walkable civic plaza. The expensive remaining work is **art** (16×24
walk cycle) and **glue**, not shaders.

## Reviewer fail

- `navigator.gpu` in `commons/index.html`
- A fullscreen quad that samples the bake (that’s `drawImage`)
- Compute-based collision or presence
- Dropping Canvas 2D “so we can do lighting later”

Mapping / readback crib: `webgpu_mapping.md`.
Textures / UV crib: `webgpu_textures.md` (not mappable).
Sprite quads / instancing crib: `webgpu_sprites.md` (two `drawImage`s stay).
