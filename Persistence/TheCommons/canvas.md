# Canvas 2D — what still matters after the blit

P0 already does the only large optimization: bake 1152×1280 offscreen,
blit a 240×160 / 160×240 crop, `{ alpha: false }`, smoothing off, integer
camera, ignore DPR, rAF as the loop. Do not add WebGL or a worker.

This file is leftover 2D hygiene for P1. 150 tiles on screen is not a
budget problem. `fillText`, context state, and drawing when idle are.

## Do in P1 (cheap, real)

**1. Bake the region labels.** They are static world-space strings.
`fillText` every frame is the slowest thing P0 still does. Stamp them
onto the bake once (same 8px ink). They scroll for free with the blit.
Dynamic nameplates (`passerby`, guests) stay over-blit.

**2. Skip the frame if nothing moved.** If `dx,dy === 0` and crop
unchanged and no other sprite moved, `return` before `drawImage`.
rAF still runs (vsync); you just don’t touch the GPU. Pair with
`document.hidden` → don’t start the 60Hz watchdog (parked P0 nit).

**3. One sprite atlas, one `drawImage` per actor.** Player / passerby
become 16×24 frames in a single image. No `<img>` per direction, no
`save/restore` per sprite, no `globalAlpha`, no `shadowBlur`, no CSS
filters on `#game`.

**4. Set context state as rarely as possible.** `font`, `fillStyle`,
`textAlign` once per batch, not per label. `fillRect` / `drawImage`
beat `beginPath` + `arc` for this art.

**5. Keep HUD in the DOM.** D-pad stays HTML. Don’t add a second
full-size canvas for chrome.

## Don’t bother (this map is too small)

- Dirty-rect of individual tiles
- Chunk streaming / region canvases
- `OffscreenCanvas` in a worker
- `createImageBitmap` unless a profiler says the canvas-source blit is hot
- `putImageData` / `getImageData` (kills the GPU path)
- Stretching the blit to 720×480 in 2D (CSS 3× is cheaper)
- `willReadFrequently`, `desynchronized`
- Object pools for 1 player + 1 passerby

## Fail on review

- `fillText` of static labels every frame after P1 (should be baked)
- `canvas.width =` inside the loop (clears the bitmap)
- `getImageData` for collision (we have `Uint8Array solid`)
- A second 1152×1280 layer in the DOM
- Watchdog interval running while `document.hidden`
