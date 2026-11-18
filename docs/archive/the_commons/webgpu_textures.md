# WebGPU textures (not buffer mapping)

Crib only. Still **no** GPU in `commons/` (`webgpu.md`). Textures
**cannot** `mapAsync`. That is the whole point of this note.

## Textures are not mappable

| | Buffer | Texture |
|---|---|---|
| CPU map | `mapAsync` + `getMappedRange` | **none** |
| GPU → CPU | copy to staging **buffer**, then map | `copyTextureToBuffer` → staging buffer → map |
| CPU → GPU | `writeBuffer` / map-write | `queue.writeTexture` / `copyBufferToTexture` |

A 1152×1280 bake as a `GPUTexture` is a one-way upload. To read a
pixel back you copy a rectangle into a buffer and wait on `mapAsync`
(`webgpu_mapping.md`). Tile collision must not do that.

## Upload

```
tex = device.createTexture({
  size: [w, h],
  format: "rgba8unorm",
  usage: GPUTextureUsage.TEXTURE_BINDING |
         GPUTextureUsage.COPY_DST |
         GPUTextureUsage.RENDER_ATTACHMENT
})
queue.writeTexture(
  { texture: tex },
  rgbaBytes,                 // tightly packed Uint8Array
  { bytesPerRow: w * 4 },    // 256-byte align if using copyBufferToTexture
  [w, h]
)
```

`writeTexture` is the small/medium path. `copyBufferToTexture` needs
`bytesPerRow` **256-aligned**. Forget that and you get a silent scramble.

## Sample vs load (WGSL)

```wgsl
@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;

// Filtered, UV 0..1 — needs a sampler
let c = textureSample(tex, samp, uv);

// Integer texel, no sampler — what a tile atlas actually wants
let c = textureLoad(tex, vec2<i32>(tileX, tileY), 0);
```

Pixel art sampler if you sample at all:

```
device.createSampler({ magFilter: "nearest", minFilter: "nearest",
                       addressModeU: "clamp-to-edge",
                       addressModeV: "clamp-to-edge" })
```

Linear magFilter is how FireRed turns to mud. Integer CSS 2×/3× on a
Canvas 2D canvas already *is* nearest (`art.md`).

## Mapping in the 3D sense

“Texture mapping” = assign UVs on a quad. A fullscreen blit of the bake
is two triangles and a UV rect. That is `ctx.drawImage(baked, sx,sy,sw,sh,
0,0,dw,dh)` in Canvas 2D — one call, no pipeline, works on the phone QA
target. A WebGPU blit is a render pipeline, bind group, sampler, and a
swapchain colorspace fight.

## For The Commons

Keep the bake as a 2D canvas. `drawImage` the crop. Do not upload 5.6 MB
RGBA to a `GPUTexture` every dress, and do not `textureLoad` collision.
Next visible work is still sprites / glue / sibling.
