# Grok bets (not locked — Claude, fight these)

Atlas / title / V1 slice / glue rules are **already locked** in `notes.md`.
These are implementation bets. If a bet hurts visiting, phone, 512 MB, or
honesty, override it in `pushback.md` and ship the better version.

## Feel

A small civic town in FireRed’s *language*, not a theme park and not a
waiting-room radio. Quiet density. One set piece per region. No quest log,
no bangs over heads, no XP. Visiting is the loop: you see someone walk
from the Square toward the Shore.

Hall is solemn (Pewter, not a casino). The Square is Pallet-scale. Houses
are modest. The boarded mine is honest, not coy.

## Grid

- Outdoor: **32×32 tiles per region** → one **96×128** canvas (3×4).
- House interior: **16×16**. Lab: **20×16**. Hall interior: skip in V1
  (door on Hall Lawn *is* the warp).
- Soft visual seams (paths continue). Region id = AABB, for presence.

Fight this if 32×32 reads empty at GBA scale, or 96×128 is too much paint
for V1. A 24×20 region is an acceptable counter.

## Camera / input

- Logical **240×160** (GBA), integer scale (3× desktop, 2× phone).
- Follow player, clamp to world. No free cam.
- Touch: on-screen d-pad. Keyboard: arrows + WASD. **A/D = left/right
  on screen** (not strafe-inverted).
- Interact: one prompt at doors (`Enter`, `Hall`, `Lab`). No RPG menu.

Fight 240×160 if it wastes a phone. Do not leave A/D inverted.

## Palette (outdoor must not be parchment)

Civic site is parchment `#efe8d8` / ink `#1a1814` / navy `#1e2d42` /
stone `#6b6458` / brass `#c4a574`. **Do not tile the world in parchment**
(unreadable grass).

Bet:

| Use | Hex |
|---|---|
| Ink (outlines, text) | `#1a1814` |
| Navy (Hall roof, civic) | `#1e2d42` |
| Brass (Hall door, sparingly) | `#c4a574` |
| Grass A/B | `#4a7c59` / `#3d6b4f` |
| Path | `#c4b49a` |
| Water | `#3d5a73` |
| Wood | `#6b4a32` |
| Stone | `#8a8378` |
| Interior wall (paper) | `#f7f1e4` |

Interior walls run warm/paper so the **profile canvas wall slot** feels
like the site hung in a house. Outdoor stays grass/dirt/water.

## Sprites

- Tiles **16×16**. Player **16×24** (or 16×32), 4-dir, 2-frame walk.
- One player look in V1. No character creator.
- Online others: same sprite, name in ink above head, small.

## Homestead / interiors

- Homestead Row is a **gate**: path + up to **6** online/friends doors.
  Overflow is later. V1 always has **one demo house** door.
- Houses are instances. Not 12 lots on the canvas.
- Wall slot = civic profile canvas. Not the floor. Empty frame in V1
  until glue.
- Demo house: table, two chairs, wall slot, a rug. Enough to visit.

## Lab (feeds visiting, is not the game)

Three lookups, real names, output **placeable / wall-able**:

1. Copper + acetic acid → copper acetate (green) → bottle on a shelf
2. Iron + tannin → iron-gall ink → a page you can hang
3. Calcium carbonate → chalk / lime → a small placeable

No stoichiometry engine. No “salt in a bag.” No fourth recipe unless
you have a better three.

## Hall / mine / workshop

- Hall Lawn door → `https://liquidgov.us/proposals` (new tab). One-line
  confirm. No cloned civic UI. No Hall interior in V1.
- Quarry: boarded adit + sit rock + honest sign. No ore, no interior.
- Workshop Lane: closed shutter + bench. No crafting loop.

## Presence / data

- Per-region ~2 Hz even on one canvas. RAM. Snapshot on leave / ~30s.
- Schema is SQLite (3NF catalog / 2NF instance) even if Grok’s preview
  first mocks it with a seed. Design the real tables.
- Guests: throwaway name, walk the 12, no bound house.
- Do not copy the friend graph. Query contract in `glue.md` only.

## Aesthetic don’ts (V1)

No isometric, no 3D, no day/night, no weather, no swimming, no farming,
no combat, no soundtrack required, no emoji chrome, no purple/gold
“game UI,” no second social network.
