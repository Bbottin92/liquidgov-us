# Art — The Commons

GBA / FireRed lean, locked in `notes.md`. Tile/sprite sizes below match
`GROK_BET.md`. Camera is Claude's rewrite — see `pushback.md`.

## Tiles

- **16×16 px** tiles, matching FireRed convention.
- Uniform **24×20 tile** regions (see `atlas.md`) → each region is 384×320 px
  of source art; the full 3×4 supermap is 1152×1280 px.
- Soft seams: the outer tile ring of each region must visually continue its
  neighbor's terrain (grass meets grass, path meets path) since the world is
  continuous, not warp-tiled. Treat the 12 regions as one map cut into
  authoring chunks, not 12 independent pieces of art.

## Sprites

- **Player: 16×24** (fits FireRed's slightly-taller-than-tile convention),
  4-direction, 2-frame walk cycle. One look in V1 — no character creator.
- **Online others:** identical sprite, small ink-colored name label above
  the head. No visual distinction between guest and bound players in V1
  (nothing to distinguish — see `glue.md`).

## Camera

Two crop shapes, not one fixed resolution — this is the deviation from
`GROK_BET.md`'s flat 240×160 everywhere (see `pushback.md` for why):

| Context | Logical viewport | Integer scale | CSS size |
|---|---|---|---|
| Desktop / landscape | 240×160 (15×10 tiles) | 3× | 720×480 |
| Phone / portrait | 160×240 (10×15 tiles) | 2× | 320×480 |

Both crops use the same tile art and the same camera logic: follow the
player, clamp to the region's (or supermap's) edge, no free cam. The phone
crop simply shows more north/south and less east/west, which suits a
walking game better than forcing landscape or letterboxing a 240-wide frame
into a ~390–430 css-px-wide portrait viewport.

`image-rendering: pixelated` (or the canvas-scaling equivalent) on both, so
integer scale actually reads crisp instead of browser-smoothed.

## Palette

Outdoor must not read as the civic site's parchment (unreadable-as-grass).
Locked from `GROK_BET.md`, unchanged — Claude agrees with this bet as-is:

| Use | Hex |
|---|---|
| Ink (outlines, text) | `#1a1814` |
| Navy (Hall roof, civic accents) | `#1e2d42` |
| Brass (Hall door, sparingly) | `#c4a574` |
| Grass A/B | `#4a7c59` / `#3d6b4f` |
| Path | `#c4b49a` |
| Water | `#3d5a73` |
| Wood | `#6b4a32` |
| Stone | `#8a8378` |
| Interior wall (paper) | `#f7f1e4` |

Interior walls run warm/paper on purpose — the wall-slot profile canvas
should feel like the civic site hung inside a house. Outdoor stays
grass/dirt/water and never borrows the paper tone.

## V1 drawable set (keep this list short)

The point of "lightly dressed" is a small, reusable tile/sprite budget, not
12 bespoke regions' worth of unique art:

**Outdoor tiles:** grass A/B, path, water, dirt/stone ground, tree, hedge,
fence/railing, water's-edge/dock plank, bridge plank, flower bed, sign post,
bench, well/fountain, boarded-adit face, door frame (generic, reused for
Hall/Lab/Homestead/demo-house), closed-shutter facade, building wall/roof
(one style, reused for Workshop/Lab/Hall exterior massing).

**Interior tiles:** paper wall, floor (wood, one style), rug, table, chair,
wall-slot frame (empty state + filled state), workbench, shelf.

**Sprites:** player (4-dir × 2-frame), name label glyph set (reuse civic
site's ink-on-paper text rendering conventions where practical), lab-output
props (bottle, hung page, small chalk block — see `pushback.md` for the
third recipe's rewrite), demo-house rug/table/chair variants if visually
distinct from the tile-set versions above (prefer reusing the tile art
directly as sprites where the two would otherwise duplicate).

Nothing above requires a unique asset per region — the same tree, bench, and
fence tiles dress North Woods, Orchard, and Garden differently only by
arrangement, not by needing separate art.

## Controls (both platforms)

- **Touch:** on-screen d-pad, thumb-reachable in the phone portrait crop's
  bottom third. If the d-pad ever needs a show/hide state (e.g. hidden while
  a dialog/prompt is open), drive it with `style.display` directly rather
  than the `hidden` attribute — the civic site's own UI has repeatedly hit
  `[hidden]` losing to an element's own inline `display` value, and the game
  client will likely inherit similar inline-styled UI patterns.
- **Keyboard:** arrow keys + WASD.
- **A/D = left/right on screen**, not strafe-inverted. This is a walking
  visiting game, not a shooter; the intuitive reading of A/D for anyone
  who's touched a keyboard is left/right, and inverting it to "strafe
  relative to facing" would be a surprise with no payoff here.
- **Interact:** one prompt at doors/set pieces (`Enter` on keyboard, tap on
  touch) — no RPG menu, no inventory screen beyond whatever V1's lab-output
  pickup needs (see `v1.md`).
