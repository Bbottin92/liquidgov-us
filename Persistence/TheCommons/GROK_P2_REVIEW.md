# Grok review of P2 (`commons/index.html`, e67d88e)

P2 is the right chunk. `activeSpace` is the correct generalization.
Do not start P3.

## Accept

- Lab Yard exterior: 7×6 wall, south-face walk-in door at (11,48), sign.
  Door is *not* solid and *not* in `interactables` (Hall/demo family).
- Exit lands at `LAB_DOOR_TILE.y + 1` so walk-in does not oscillate.
- Demo house 16×16: table, two chairs, rug (walkable), empty wall-slot
  on the wall row, Enter in / Enter out, restore saved outdoor pixel.
- Lab 20×16: three stations, chalk recipe has no lime, session `inventory`
  only, gone on refresh, no SQLite.
- Collision uses `activeSpace` (same 12×12 box, 1/20 dt clamp).
- Station interact is checked *before* interior-Enter-exit.
- P0/P1 pins held: notice-post, passerby visible, doors solid, toast
  off-transition, dual crop, civic tree untouched.
- Six quiet outdoor regions still P0 grass. Workshop not opened.

## Parked (not a new patch)

- On-screen button caption is hardcoded `Enter`. At a station it still
  *does* Look (nearest wins). Set `textContent` from
  `nearestInteractable.prompt` when P3 opens, not now.
- Lab pickups are a RAM counter, not placed in the house. Correct for
  this preview (`v1.md`). Wall-slot stays empty until glue.
- Outdoor lab is smaller than the 20×16 interior. Fine (GBA buildings).

## Fail if “fixed” by

- Making the Lab door Enter-to-confirm
- Faking a bitmap in the wall-slot
- Wiring SQLite
- Dressing North Woods / Lookout / Orchard / Workshop / Garden / Quarry

P2 is done. Do not start P3 until Brandon says so.
