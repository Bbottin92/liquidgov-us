# Grok review of sprites (`commons/index.html`, 23c0a11 / 7d32165)

The atlas is the right shape. One 16×24 look, 4 dirs × 2 frames, one
`drawImage` per actor, feet-anchored, `alpha:true` only on the atlas,
`wpDir` rename is correct, civic untouched, no GPU.

## Accept

- Shared atlas for player and passerby
- Facing from **input**, walk cycle from **successful** step
- Nameplate above the taller sprite
- Idle legs when not moved

## Must-fix

Idle-skip does not see a facing change. `movePlayer` can set `dir` while
returning `false` (hold a new key into a wall). `changed` is only
`playerMoved` / passerby x,y. The sprite keeps the old cell until you
actually walk.

Treat visual dirty as:

```
dir or animFrame changed  →  blit
```

Track `prevPlayerDir` / `prevPlayerAnim` (and the same for passerby).
Bump-to-face was the point of `updateFacingAnim`; skipping the frame
undoes it.

## Not a fail

- Procedural fillRect figure instead of a PNG (V1 look; a pixel sheet
  can replace the atlas painter later without touching movement)
- Player has no own nameplate (art.md labels *others*)
- Vertical facing wins on diagonals

## Fail if “fixed” by

- Dropping idle-skip entirely
- `navigator.gpu` / a PNG toolchain / npm
- Touching `liquidgov-platform/`

Paste `CLAUDE_SPRITES_PATCH.md`. Then glue waits on Brandon.

---

## Follow-up (9139c99 / d04d813) — accepted

`prevPlayerDir` / `prevPlayerAnim` and the passerby pair are in `changed`.
Idle-skip kept. Civic untouched. Sprites slice is closed.

Glue (cookie read) then sibling still wait on Brandon.
