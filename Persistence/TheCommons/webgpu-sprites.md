# WebGPU sprite rendering

Crib only. **Do not** replace the Canvas 2D atlas in `commons/`.
Sprites just shipped as two `drawImage`s off a 128×24 canvas atlas
(`GROK_SPRITES_REVIEW.md`). That is the whole renderer.

## What a GPU sprite pass actually is

1. Upload the atlas as a `GPUTexture` (`webgpu-textures.md`).
2. One quad per actor (or one instanced draw): clip-space xy + UV rect
   into the 16×24 cell.
3. Fragment: `textureSample` or `textureLoad` + alpha blend.
4. Sampler **nearest**, clamp. Linear magFilter muddies FireRed.
5. Y-sort by **feet** (`spriteDestY`) if two sprites overlap.

WGSL fragment (nearest texel, not a fancy shader):

```wgsl
@group(0) @binding(0) var atlas: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;

@fragment
fn fs(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(atlas, samp, uv);
}
```

Host still has to: create pipeline, swapchain, projection (Y-down vs
Y-up), premul-alpha blend, `writeTexture` the atlas, rebuild instance
buffer when `dir`/`animFrame` changes.

## vs what we already ship

| | Canvas 2D now | WebGPU sprite pass |
|---|---|---|
| Draw | `drawImage(atlas, sx,0,16,24, dx,dy,16,24)` | pipeline + bind group + quad |
| Count | 2 actors | same 2 actors |
| Filter | `imageSmoothingEnabled = false` | `nearest` sampler, easy to get wrong |
| Alpha | atlas canvas `{alpha:true}` | blend state; opaque swapchain fights it |
| Scale | CSS 2×/3× pixelated | you must integer-snap clip space yourself |
| Phone | Canvas 2D | WebGPU still uneven on iOS Safari |

Two sprites is not a batching problem. Instancing pays off at hundreds
of quads, not passerby + player.

## Do not GPU-blit the world either

The 1152×1280 bake is already one `drawImage` crop. A fullscreen textured
quad is the same blit with a colorspace fight. `tilemap.md` already said
no WebGL. This is that sentence again.

## For this repo

Keep the canvas atlas. The idle-skip must-fix (`dir`/`animFrame` dirty)
is still the open sprites work. Glue / sibling wait on Brandon.
A pixel PNG can replace `paintSpriteAtlas()` later without a GPU.
