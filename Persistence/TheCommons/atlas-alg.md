# Atlas packing algorithms — none of these earn a slot

Explore, not a BUILD. Slice 1 is still `paintTile` + a tighter 16×24.
`atlas-pack.md` already said no TexturePacker. This is why the algorithms
don't get a vote.

## The input we actually have

Eight **identical** 16×24 rectangles (4 dir × 2 frames). Tiles are not
packed at all: they are stamped into the world bake (`atlas-pack.md`).

Bin packing (shelf, guillotine, MaxRects, Skyline-BL, growing binary tree)
exists to pack **unequal** rects into a power-of-two texture with few gaps.
That problem is NP-hard. Our instance is 8 congruent cells. The optimum is
a **4×2 grid**, waste 0, lookup `dir * 2 + frame`. Any heuristic is a
worse API for the same pixels.

## What the names mean (so nobody "improves" it)

| Family | Idea | For us |
|---|---|---|
| **Shelf** | Left-to-right rows; new shelf when width overflows | 4×2 *is* two shelves of 4. Done. |
| **Guillotine** | Split leftover L into two rects after each place | Splits a uniform grid into a tree. Lookup gets worse. |
| **MaxRects / Skyline** | Track free rects / skyline; pick Best Short Side etc. | For mixed UI icons, not 8 clones. |
| **Growing bin** | Expand canvas when full | Atlas is painted once at load to a known size. |

Rotation (pack 16×24 as 24×16) would mean a runtime rotate on every blit.
Don't.

## Fail if "explored" by

- A JS packer, `potpack`, `maxrects-packer`
- Per-frame repack when someone faces a new way
- Power-of-two padding to 256 for a GPU that we are not using
- Changing slice 1 into "export a packed PNG"

Keep the 4×2. Paint it once. `drawImage` the cell.
