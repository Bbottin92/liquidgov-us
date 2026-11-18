# Texture compression — Canvas 2D never sees a GPU block

Explore, not a BUILD. Slice 1 is still `paintTile`. Do not add Basis,
KTX2, DXT, or a WASM transcoder.

## What those formats are

GPU **block** compressors: BC1–7 (DXT/BPTC, desktop), ETC2/EAC (GLES),
ASTC (mobile), PVRTC (old iOS). Fixed 4×4 (or ASTC variable) blocks in
VRAM, sampled by the GPU, never expanded to RGBA unless you ask.

**Basis Universal / KTX2 / UASTC** transcode *to* one of those at load, in
WASM, so one file hits many GPUs. That is a WebGL/WebGPU pipeline.

Canvas 2D `drawImage` / `fillRect` wants **decoded RGBA** in a
`CanvasImageSource`. The browser already expanded any PNG/JPEG to RGBA
before your blit. There is no `drawImage` of BC3. Putting a transcoder in
`index.html` would expand to RGBA **anyway**, then you blit. Extra code,
same pixels, worse start time.

## Our sizes

Outdoor bake: 72×80 tiles × 16 px = 1152×1280 RGBA ≈ **5.6 MB** of bitmap,
allocated **once**. Sprite atlas: 64×48 if 4×2 of 16×24, noise. Compressing
either for VRAM is solving a 2005 phone problem we do not have.

PNG on the wire (if a sheet is ever named) is the right *file* format:
lose-less, 8-bit, no build step. Do not gzip-then-DXT a 6 kB sheet.

## Fail if "explored" by

- `WEBGL_compressed_texture_*`, KTX2, basis_universal, crnlib
- ASTC for the grass dither
- A 4×4 block encoder in the bake (block artifacts on 16×16 tiles, and
  Canvas cannot sample them)
- Changing slice 1 into an asset-pipeline BUILD

Keep RGBA in two canvases. Paint once. Blit.
