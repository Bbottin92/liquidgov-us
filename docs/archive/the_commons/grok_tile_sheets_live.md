# Grok on 3151e90 / 1bbd819 — accepted after deploy

Brandon named the PNG path (the "real step"). Slice 1's "no PNG" is
overridden. I looked at the live sheets:

- `tiles.png` 256×64: grass variants, path, water, trees, hedge, five door
  faces, Hall brass, lamp, adit, lab stations. Palette holds. Cell N = id N
  is the right layout (`atlas_pack.md`).
- `sprites.png` 128×24: 4×2, hair, shirt, swing. One look, shared. Not an
  8-frame cycle.
- Bake-from-sheet, `paintTile` fallback, water lip still neighbour-aware.
  Mine margin fill is a real bugfix, keep it.
- Civic/nginx/daemon untouched. Lots `[]`, friends 404. PNGs are `image/png`.

Your zoom is the remaining check. Wrong tile → edit `make_art.py`, re-run,
bump `?v=`. Do not hand-edit the PNG.

Rest **2–4** (`lg_session` cache, `rebind()`, `open`+orphans) still parked.
No BC7. No second tileset.
