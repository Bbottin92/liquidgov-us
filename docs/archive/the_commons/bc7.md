# BC7 — why people like it, and why we cannot use it

Explore, not a BUILD. Follows `tex_compress.md`. Slice 1 is still
`paintTile`. Do not encode the bake as BC7.

## What BC7 actually buys

BC7 (DXGI `BC7_UNORM`, OpenGL `BPTC`) is an 8-bit-per-pixel **block**
format: every 4×4 is 16 bytes. It has 8 modes (partitions, extra color
endpoints, optional alpha). Versus BC1 (DXT1, 4 bpp, no decent alpha) and
BC3 (DXT5, 8 bpp, weak color), BC7 is the one that still looks like the
source on normal maps *and* UI *and* RGBA sprites, with much less block
posterization. Desktop GPUs since ~2012 sample it natively. Mobile often
doesn't (ASTC instead). Encode is **slow** (mode search); decode on CPU is
not free. Basis UASTC exists to transcode *into* BC7 at load.

That is a real win if you are filling VRAM with 2k albedo in WebGL2/WebGPU
and the adapter exposes `texture-compression-bc`.

## What it does not buy here

`CanvasRenderingContext2D` has no compressed-texture upload. `drawImage` of
a BC7 blob is not a thing. To use BC7 we would: leave Canvas 2D (forbidden
by `wasm.md` / the WebGPU notes), or decode BC7 → RGBA on the CPU and blit
anyway — **larger code, same RGBA, worse load**, which is `tex_compress.md`'s
whole point.

Our sprite atlas is 8 cells. BC7's 4×4 blocks would also sit awkwardly on
16×24 (height 24 is not a multiple of 4 — you'd pad to 16×28 and waste).
The grass dither is 2×2 dots; BC7 partitions are 4×4. You would *see* the
blocks on a 3× GBA crop.

## Fail if "explored" by

- `BC7_UNORM` in `index.html`
- ispc/bc7enc in the daemon
- Padding the player to 16×32 "for BC7"
- A WebGPU path "just for the sprite sheet"

Keep RGBA canvases. No BC7, no ASTC, no Basis.
