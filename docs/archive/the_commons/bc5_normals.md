# BC5 normal-map encoding — how, and why we don't

Explore, not a BUILD. Follows `bc5.md`. Slice 1 is still `paintTile`.
No sit-rock normal map.

## What gets stored

A tangent-space normal is a unit vector `(x, y, z)` in −1…1, usually
remapped to 0–1 in a texture (`n * 0.5 + 0.5`). The surface is mostly
`(0, 0, 1)` in tangent space, so **Z is almost 1** and can be rebuilt:

```
z = sqrt(max(0, 1 - x*x - y*y))
```

BC5 stores **only X and Y**, each as a separate BC4 (RGTC1) 4-bit-per-pixel
block, 8 bpp total. The shader samples `rg`, expands to −1…1, rebuilds Z,
normalizes. That is the whole trick: throw away the channel that was a
function of the other two, spend the bits on the two that actually wiggle.

DXT5nm was the older hack: put X in alpha (DXT5's good channel) and Y in
green. BC5 is the honest two-channel version (3Dc / ATI2).

`BC5_SNORM` can store signed XY directly and skip the 0.5 bias. Same idea.

## What goes wrong

- Unit-length is assumed. Encoding error + `sqrt` can denormalize; shaders
  often `normalize` again.
- Z is always **positive**. You cannot encode a normal that points into the
  surface without a third channel or a different space.
- There is **no lighting pass** in The Commons. A reconstructed Z has
  nowhere to go: no Lambert, no bump on the bake, Canvas 2D is a blit of
  sRGB.
- Canvas cannot sample RGTC (`tex_compress.md`).

## Fail if "explored" by

- `sqrt(1-x*x-y*y)` in `paintTile`
- A `_n.png` next to the tiles
- DXT5nm in the sprite atlas
- Changing slice 1

Keep painted RGBA. No normals.
