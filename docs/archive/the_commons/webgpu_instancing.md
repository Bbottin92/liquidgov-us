# GPU instancing

Crib only. Does not land in `commons/`. Two actors already share one
atlas via two `drawImage`s (`webgpu_sprites.md`). Instancing is how you
draw **the same mesh many times** with a per-instance buffer — not how
you make a passerby cheaper.

## The technique

One vertex buffer = the quad (4 verts, or a 6-vert sprite).
One **instance** buffer = one struct per actor:

```
pos:     vec2   // feet, pixels
uv0:     vec2   // atlas cell origin
size:    vec2   // 16×24
tint:    u32    // unused in V1
```

Vertex shader:

```
world = instance.pos + quad.corner * instance.size
uv    = instance.uv0 + quad.corner * (16,24) / atlasSize
```

WebGPU: a second vertex buffer with `stepMode: "instance"`, then
`pass.draw(4, instanceCount)`. One pipeline, one bind group (the atlas),
N sprites.

Fancier, not for us:

| Trick | What it is | Cost |
|---|---|---|
| GPU culling | compute writes a packed instance list | need `mapAsync` or indirect draw |
| `drawIndirect` | GPU sets instanceCount | extra buffer, extra pass |
| Texture arrays | one layer per animation | memory; atlas UVs are simpler |
| Persistent mapped instance buffer | rewrite every frame | WebGPU doesn’t persist-map |

## When it pays

Hundreds of the **same** quad (bullets, grass, particles). One draw call
beats N `drawImage`s when CPU bind overhead dominates.

Presence cap is **96** (`presence.md`). Even then, 96 Canvas 2D
`drawImage`s of 16×24 is nothing next to the 1152×1280 blit. The droplet
CPU is the cap, not fill rate.

## When it is a downgrade

- 2 instances (player + passerby) — pipeline setup > the draws
- Unique meshes per tile type — that’s an atlas blit, which we already
  baked **once**
- Y-sort by feet still needs a CPU sort (or depth), same as 2D
- Phone Safari — Canvas 2D is the portable path (`webgpu.md`)

## For this repo

Do not add an instance buffer, a compute cull, or `navigator.gpu`.
Open sprites work is still idle-skip seeing `dir`/`animFrame`
(`claude_sprites_patch.md`). Glue / sibling wait on Brandon.
