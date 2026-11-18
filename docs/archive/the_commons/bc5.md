# BC7 vs BC5 — color versus normals, still not Canvas

Explore, not a BUILD. Follows `bc7.md` / `bc6h.md`. Slice 1 is still
`paintTile`.

## Side by side

| | **BC7** | **BC5** (3Dc / RGTC2) |
|---|---|---|
| Channels | RGB + **alpha**, 8 bpp | **Two** UNORM channels (RG), 8 bpp |
| For | albedo, sprites, UI | Tangent-space **normal maps** (XY; Z reconstructed) |
| DXGI | `BC7_UNORM` | `BC5_UNORM` |
| Block | 16 bytes / 4×4 | 16 bytes / 4×4 (two BC4 blocks) |
| Alpha / blue | Yes / full RGB | No. Reconstruct `z = sqrt(1-x²-y²)` in the shader |
| Looks bad on | — | Anything that needs a third color or coverage |

BC5 is why source engines ship `_n` maps as two channels. It is **not** a
smaller BC7. Grass `#4a7c59` has three channels; the player sprite needs
alpha. BC5 would throw both away and invent a fake Z.

Same wall as the others: Canvas 2D cannot sample RGTC. No normal maps in
The Commons (`art.md`: one look, GBA tiles, no lighting pass).

## Fail if "explored" by

- A normal map for the sit-rock
- Reconstructing Z in `paintTile`
- BC4/BC5 in `index.html`
- Changing slice 1

Keep 8-bit RGBA. No BCn family at all.
