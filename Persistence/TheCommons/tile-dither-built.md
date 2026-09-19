# Tile dither — built, local-tested, not deployed

Brandon: "BUILD tile dither" (Grok's graphics note: "Dither and edge the
existing 16×16 ids in the bake ... Same palette. Same blit. Hours, not a new
pipeline."). **Client only**, `commons/index.html`. Daemon, civic, nginx,
schema untouched. No deploy.

## What changed

The bake used to be one `fillRect` per tile. It is still **one bake, one
blit, the same `COLOR` table and the same tile ids**. A new `paintTile()`
paints the base colour, then a little detail inside the tile:

- **Grass A/B:** a sparse 2×2px checker of slightly darker and lighter dots on
  a 4px pitch. Which phase a tile gets comes from a fixed hash of its
  position (three phases), so the lattice doesn't read as one repeated
  stamp; the same tile always gets the same pattern.
- **Water:** a darker 2px lip on each edge that meets a non-water tile (bank,
  bridge). The map edge gets no lip.
- **`T_BENCH`** (every bench, sit-rock and sit-log, including the mine's two):
  two broken lines of wood grain, a lighter top edge, a shaded underside.

Every shade is derived from the tile's own `COLOR` by mixing toward black or
white (`shadeHex`), so **no new palette entry, no asset, no new draw path**.
The outdoor bake and `buildInterior()` both use it; interiors get the bench
grain (the mine) and nothing else changes there.

## Tested (local, real browser)

- Looked at it: grass reads as a soft checker rather than a flat field; the
  pond edge shows a lip against the bank and the bridge; the bench at the
  North Woods shows grain.
- Pixel check on the baked canvas: a grass tile has 3 colours (base + dot
  tones), a bench tile 3, the mine's bench 3; a water-edge pixel is
  `rgb(44,65,83)` against `rgb(61,90,115)` for the interior.
- Cost: all 5,760 tiles repainted in ~51ms, **once** at load (the bake is
  unchanged in shape, it is not per frame).
- Console clean. Collision, interaction and networking are untouched (this
  never reads or writes `tiles`/`solid`, only paints from them).

## Not here

The tileset PNG and player sheet (Grok's "real step") -- not named. No
day/night, weather, shaders or per-region art. Nothing about the player
sprite. Deploy is one client sync when Brandon says `deploy`.

## Deployed (2026-09-19, Brandon: "deploy"; Grok accepted in `GROK_TILE_DITHER_REVIEW.md`)

Client only, via `sync-client-to-droplet.sh` to `/opt/commons`, chown
`liquidgov`. Daemon, civic, nginx untouched. Droplet and served `/commons/`
both md5 `dde34287...` = local (was `e99898fa...`); served page contains
`paintTile`; `/commons/` and `/presence/lots` 200, public `/internal/friends`
404; both services active.

Checked on production as a guest (`dither-check`): the page loads, the Square
grass shows the checker, the creek shows its lip against the bank and the
bridge, console clean. Grok's caveat stands: if the 2x2 dots "crawl" at real
play zoom, say so and the lattice gets thinned (no PNG to fix a dot).
