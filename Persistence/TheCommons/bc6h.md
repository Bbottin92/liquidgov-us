# BC7 vs BC6H — different jobs, still not ours

Explore, not a BUILD. Follows `bc7.md`. Slice 1 is still `paintTile`.

## Side by side

| | **BC7** | **BC6H** |
|---|---|---|
| For | LDR color, sRGB or UNORM, **alpha** | HDR **lighting**: RGB float, no useful alpha |
| DXGI | `BC7_UNORM` / `_SRGB` | `BC6H_UF16` / `_SF16` |
| Bits | 8 bpp, 16 bytes / 4×4 | 8 bpp, same block size |
| Range | 0–1 (or sRGB) | unsigned or signed half-float, values >> 1 |
| Modes | 8, partitions, extra endpoints, alpha | 14, light-map / skybox friendly |
| Looks bad on | UI if you used BC1 instead | Albedo, sprites, anything LDR |
| Typical use | albedo, sprites, UI in VRAM | irradiance, HDR probes, bloom sources |

Both are BPTC, desktop-native, encode-heavy, **GPU sample only**. BC6H is
not "better BC7". It is the HDR cousin. Putting grass `#4a7c59` through
BC6H is a category error: you would expand it to float, compress as light,
and get banding plus no alpha for the player.

The Commons has no HDR buffer, no IBL, no bloom. The bake is 8-bit sRGB in
a Canvas 2D bitmap. Neither format is reachable from `drawImage`
(`tex-compress.md`).

## Fail if "explored" by

- A float canvas, `EXT_color_buffer_float`, BC6H sky
- Encoding the lamp tile as HDR "because it's bright"
- WebGPU *just* to get BPTC
- Changing slice 1

Keep 8-bit RGBA. No BC7, no BC6H.
