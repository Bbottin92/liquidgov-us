# Grok review of P1 (`commons/index.html`, 2eb59a9)

Spine is the right chunk. Hall confirm+open, 7-door facade, baked labels,
idle skip, `document.hidden` watchdog, civic tree untouched. The spawn
trap you caught in testing was real. Do not start P2.

## Accept

- Only Square / Homestead / Creek / Shore / Hall Lawn dressed
- Homestead: 1 demo door + 6 boarded, path kept through the facade
- Hall: `confirm` then `window.open` `/proposals` with `noopener`
- Demo door → black placeholder, Enter back out (furniture is P2)
- Labels stamped on the bake; blit skipped when idle
- Watchdog no-ops while `document.hidden`
- `isAmbient` is a flag, not a string match
- Name-field WASD guard still holds

## Must-fix

**1. Notice-post, not a fountain.** Locked when P0 was scoped: Square set
piece is a civic notice-post. `T_FOUNTAIN` and the spawn-overlap hack are
symptoms of the wrong prop. One tile, ink/brass post, benches may stay.
Do not leave a fountain in the file.

**2. Passerby must be visible in the Square.** Hide rule as coded:
`visible = playerRegion !== pbRegion`. You spawn in the Square; passerby
spawns in the Square; it hides. Camera from spawn cannot see Homestead.
The Square is empty — the waiting-room look this crutch exists to prevent.

The lock was “hide when a *second real occupant* is already there,” not
“hide whenever the local player is.” V1 has one real guest (you). Passerby
stays visible, nameplate `passerby`. Hide only if another non-ambient
occupant shares the region (nobody, today).

**3. Toast can stick.** Idle-skip redraws while `toastActive`, then skips
the first frame after `toast.until`. “Closed.” stays on the bitmap until
the player moves. Force a redraw on the off-transition.

## Should-fix (same sitting)

- `T_DOOR_BOARDED` (and probably Hall / demo) **solid** — walking through
  a “Closed.” door is a hole in the gate
- Creek/Shore benches sit in flooded water, unreachable; put them on the
  path/bridge
- Last passerby waypoint is in water (`Shore x1-4, y1-1`). Keep the patrol
  on the ribbon; no walking on water

## Not a fail (P2 / P3)

- Checker grass, Creek/Shore still flood-filled
- fillRect actors, no sprite atlas yet
- Interior is a black stub (that was P1)
- Player has no own nameplate

## Fail if “fixed” by

- Deleting the passerby
- Unlabeled passerby
- Starting P2 furniture / lab
- Touching `liquidgov-platform/`

Paste `CLAUDE_P1_PATCH.md`. One commit. Then stop.
