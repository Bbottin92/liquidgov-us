# Tileset PNG + player sheet — built, local-tested, not deployed

Brandon: "BUILD tileset PNG + player sheet" (Grok's graphics note and
`atlas_pack.md`'s "If a PNG ever exists": one `tiles.png`, 16×16 grid, ids
row-major matching the `T_*` enum; one `sprites.png`, 4 dir × 2 frame; the bake
stamps from the sheet **once**). Client only. Daemon, civic, nginx, schema
untouched. No deploy.

Who drew it: I did, from `art.md`'s V1 drawable list, with a small generator
(`docs/archive/the_commons/art/make_art.py`, stdlib only). No character creator,
no per-region art, no runtime packer.

## What ships

- **`commons/tiles.png`** (256×64, 2.5 KB): cell N = tile id N for the 36 `T_*`
  ids; cells 48–51 are grass variants (A phase 1,2 = 48,49; B = 50,51) so the
  position-hash variation from the dither round survives. Drawn: dotted grass, a
  pebbled path, rippled water, a real tree on grass, a leafy hedge, plank walls,
  five door faces (plain, house, boarded with crossed planks, Hall brass panels,
  Lab with a window), notice post and sign board, plank bridge and floor, stone
  low wall with mortar, crate, adit rock, lamp glow, mine door with timber posts,
  table, chair, rug, wall-slot frame, workbench, and the three lookup stations
  each with a small emblem. Reuse is by arrangement, per the budget.
- **`commons/sprites.png`** (128×24, alpha, 379 B): the same 16×24 player, 8
  cells (`dir*2 + frame`, DOWN/UP/LEFT/RIGHT), now with hair, a shaded shirt,
  swinging arms, and a back-of-head view. Player, passerby and every occupant
  still share the one atlas.
- **Client (`commons/index.html`):** `drawTile()` stamps from the sheet;
  `paintTile()` (the dither round) stays as the **fallback**. The sheet loads
  asynchronously, so the world first bakes with `paintTile`, then every baked
  space (outdoor + each interior, tracked in `spaceRebakers`) re-stamps once when
  the PNG arrives at exactly 256×64 (sprites: 128×24) and the next frame
  redraws. Offline, a 404, or a wrong-sized file leaves the painted look in
  place. The water lip stays a small neighbour-aware overlay (it depends on the
  neighbours, so it isn't 16 edge variants in the sheet). Camera, collision, the
  bake-once-then-blit shape and networking are untouched.
- **Generator QA:** `make_art.py` parses the client's own `T_*` ids and `COLOR`
  table and fails if any tile's mean colour strays more than 60 (85 for the
  notice post) from its `COLOR`, so the sheet can't drift off-palette or out of
  id order. Output is byte-identical run to run.

## One adjacent fix (found while testing this)

A room smaller than the viewport (the 16×12 mine) left the canvas below it
holding whatever the previous frame was, which was the outdoor stone ground:
real, latent, and made obvious by the new art. `drawSpace()` now fills the
margin with the page's black when the space is smaller than the viewport
(outdoors pays nothing), and the bottom `[Enter] to step back outside` hint and
toasts switch from ink to paper when they land on that black margin, so they
stay readable in the mine.

## Tested (local, real browser, scratch daemon)

- Loaded as a guest: `tileSheet` set at 256×64, five spaces re-baked, console
  clean. Looked at: Square path and grass, the creek with lip and bridge planks,
  North Woods trees, the Homestead facade (house/demo/boarded doors, plank walls),
  the Lab (plank floor, workbench, three stations), the mine (stone walls, lamp,
  sign, sit-rocks), the player facing up/down/left/right.
- Walked into the mine with a real Enter keypress from outdoors: no stale pixels,
  hint and the sign toast readable on the margin.
- **Fallback:** a copy of the page served with no PNGs (404): `tileSheet` stays
  null, the painted look and original figures render, the game runs.
- Not exercised: the sheets through nginx (it serves `/opt/commons/` as-is, so
  `image/png` is expected; the deploy check confirms it), a real bound player.

## Deploy (when Brandon says `deploy`, after Grok's review)

Client only: `sync-client-to-droplet.sh` now carries **three** files
(`index.html`, `tiles.png`, `sprites.png`; nginx already aliases the whole
directory, no nginx change). Order does not matter (the page falls back if a PNG
isn't there yet), but sync all three together. Check: served hashes of all three
match local, both PNGs come back `200 image/png` through nginx, and a guest
walks the Square, the Lab and the mine with a clean console. `tiles.png?v=1` is
the cache key: bump `v` in `index.html` whenever the art changes.

## Not here

Day/night, weather, shaders, unique mine art, normal maps (`bc5_normals.md`),
a second tileset per region, a character creator. Tuning any single tile is a
one-line edit in `make_art.py` plus a re-run.

## Deployed (2026-09-19, Brandon: "deploy")

Grok's review was **not** on file when Brandon said `deploy`; I went on his
word and say so here. Client only, via `sync-client-to-droplet.sh` to
`/opt/commons`, chown `liquidgov`. Daemon, civic, nginx untouched.

- **Drift check:** the dry-run sent exactly `index.html` (changed) and the two
  new PNGs; the droplet's `index.html` was still the last deployed hash
  (`dde34287...`).
- **Hashes, droplet = served = local:** `index.html` `1e924625...` (was
  `dde34287...`), `tiles.png` `853cdf21...`, `sprites.png` `ae31b218...`.
- **Through nginx:** `tiles.png` and `sprites.png` both `200 image/png`
  (2534 B / 379 B); `/commons/` 200; `/commons/presence/lots` 200; public
  `/internal/friends` 404; both services active. The served page references
  `tiles.png`.
- **Guest walk on production:** `tileSheet` loaded at 256x64 and all five baked
  spaces re-stamped; the Homestead facade shows the new doors and plank walls; the
  Lab shows the plank floor, workbench and stations; the mine (entered with a real
  Enter keypress) has stone walls, the lamp and the sign, black margin below the
  room, and the sign toast is readable on it; the player figure renders in all
  frames I looked at. Console clean.

Not exercised on production: a bound player (the sheet path is identical for
bound and guest). If any tile reads wrong, it is a one-line edit in
`make_art.py`, a re-run, and a client sync (bump `?v=` in `index.html` so
browsers refetch the PNG).
