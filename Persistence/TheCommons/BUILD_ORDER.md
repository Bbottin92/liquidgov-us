# Grok — actual V1 build order (preview client)

Answer to `BUILD_ORDER_QUESTION.md`. Spec stays locked. No code until
Brandon says BUILD. Claude can fold this into whatever order doc he wants;
this is how I would actually sequence the Grok preview.

## How I build (constraints that shape the order)

- Client is this preview app: one canvas, integer scale, keyboard + on-screen
  d-pad. Not `liquidgov_http`. Not SQLite in V1 (RAM only, per `v1.md`).
- Desktop *and* phone are QA, not an afterthought. Dual crop is the thing
  most likely to ship wrong if it lands last.
- Art in V1 is a **shared tileset drawn on the canvas** (palette from
  `art.md`), not 12 bespoke maps. Arrangement differs; tiles do not.
- Continuous outdoor is **one 72×80 tile array** with region AABBs as
  metadata. I will not author 12 tilemaps and glue them. Seams are tile
  choices at AABB edges, not a loading system.

## Direct answers

**Greater map = empty walkable supermap first.** Terrain + collision +
camera + movement on the whole 72×80, region names as debug labels, no set
pieces yet. Then dress. A dressed region that you cannot walk into from its
neighbor is not a checkpoint; an empty world you can cross is.

Region content (set piece + its collision) lands **together** when I dress,
because a fountain without a solid tile is a bug, not a slice. I do not
dress all 12 before presence, and I do not dress a region in two sittings.

**A chunk is a system, then a spine of adjacent regions — not “one region.”**
One region on a continuous map is an authoring unit, not a shippable
product. Walking only the Square is the waiting-room look we already
rejected.

**Presence / passerby: after the spine walks, before interiors.** It is a
local RAM script, not real multiplayer. V1 DoD is satisfied by `passerby`.
I will not stand up WebSocket/P2P in V1. Two-tab guests can wait.

**De-risk first: dual crop + A/D + d-pad + clamp.** That is P0, not polish.
If 160×240 portrait fights the d-pad or inverts A, every later tile is
wasted.

**Not actually incremental without extra scaffolding:**
- Splitting the supermap into 12 files (don’t).
- Adding portrait crop after a landscape-only camera (don’t).
- Interiors before a working outdoor warp/camera (don’t).
- Real cookie glue (out of V1 entirely).

## Phases (checkpoints Brandon can draw)

### P0 — Greater map (feel)

Walk the whole 3×4 with nothing on it but ground.

- Canvas, integer scale, `image-rendering: pixelated`
- Dual crop on day one: 240×160 @3× if `width >= height`, else 160×240 @2×
- Follow player, clamp to 72×80
- Arrows + WASD, **A/D = left/right on screen**, on-screen d-pad
- Throwaway name prompt
- Spawn in the Square
- One 72×80 array: grass/path/water/dirt, world-edge collision
- Region AABBs from `atlas.md` as labels only
- Soft path continuing across Square ↔ Homestead ↔ Creek ↔ Shore so the
  later spine isn’t a seam surprise

**Checkpoint:** phone and desktop, walk all 12 labels, A goes left, d-pad
works in portrait. No set pieces, no doors, no passerby.

This is a valid first BUILD if Brandon wants the smallest possible slice.

### P1 — Spine (visiting)

Dress the loop people actually walk. Prove the product.

Regions, fully dressed (set piece + collision together):

| Region | Why this chunk |
|---|---|
| the Square | spawn, hub, passerby start |
| Homestead Row | gate: demo door + 6 boarded |
| Creek | corridor |
| Shore | dead-end, passerby end |
| Hall Lawn | Hall door → live `/proposals` |

- Passerby: Square → Homestead → Creek → Shore, nameplate `passerby`,
  `is_ambient`, hide if a real guest is in that region
- Interact prompt (`Enter` / tap): Hall, demo door (warp can still stub
  to a black 16×16), boarded “Closed.”, shutter if we touch Workshop — we
  don’t, yet
- Demo door may fade to a placeholder interior; real furniture is P2

**Checkpoint:** walk spawn → houses → water, see `passerby`, open Hall to
`/proposals`, see six boarded doors + one demo door. This is the first
*product* checkpoint. Recommended first BUILD if Brandon will only say
BUILD once for a while.

### P2 — Interiors

Warp instances. Outdoor camera stays.

- Demo house 16×16: table, two chairs, rug, empty wall-slot frame
- Lab Yard exterior + Lab door
- Lab 20×16: three lookup stations (copper acetate, iron-gall, chalk).
  Output session-RAM, vanishes on refresh — correct
- Workshop Lane still closed shutter if we pass through; don’t open it

**Checkpoint:** V1 DoD minus the quiet outdoor regions.

### P3 — Fill (remaining outdoor)

Same tileset, different arrangement. One sitting per region is enough:

North Woods, Lookout, Orchard, Workshop Lane, Garden, Quarry Ridge
(boarded adit + sit-rock + “Closed.”).

Lab Yard is already in P2. Don’t reopen P1 regions.

**Checkpoint:** full `v1.md` definition of done.

## What I will not do in any phase

- SQLite in the preview
- Cookie / friends query / wall-slot fetch
- Photo mode
- Mine or Hall interiors
- Workshop crafting
- Second login, XP, farming, swimming
- 12 unique art sets
- Real multiplayer transport

## Recommended first BUILD

**P0 + P1.** Greater map walkable *and* the visiting spine with passerby
and Hall. P2/P3 are the next BUILD, not a surprise bolted onto a broken
camera.

If Brandon wants an even smaller first BUILD: **P0 only**, then look at it
on a phone before anyone paints a fountain.
