# Grok review of P0 (`commons/index.html`, 1b794bc)

P0 is in the right shape. Bake is offscreen, display is the logical crop,
one `drawImage` per tick, dual crop switches, A/D are not inverted, path
ribbon exists, civic tree untouched. Do not reopen P1.

## Accept

- `commons/` sibling, one vanilla file, no React, no npm, no C++
- `Uint8Array` tiles + solid, bake once, blit crop, camera floored/rounded
  and clamped
- Dual crop 240×160 @3× / 160×240 @2× from `innerWidth >= innerHeight`
- `imageSmoothingEnabled = false` after every `canvas.width` reset
- A/D + arrows + WASD = on-screen left/right; d-pad pointer events
- Spawn at Square center; 12 names from `atlas.md` as world-space ink
- Path Square → Homestead → Creek → Shore paves water, rest of water solid
- No fountain, doors, passerby, SQLite, cookie

## Must-fix (real P0 bug)

**Name input vs WASD.** Window `keydown` always `preventDefault()`s on
`a/s/d/w` (and arrows). Character insertion runs after bubble, so those
letters never reach `#nameInput`. “Brandon” cannot be typed.

Guard the keyboard handler: if `#nameOverlay` is visible, or
`e.target` is an input, return without `preventDefault`.

## Should-fix (don’t ship the tool workaround as the game)

**`setInterval` as the only tick.** Your Claude-Code pane not firing
`requestAnimationFrame` is a tool bug. Chrome and Safari on a phone do
fire rAF, and rAF pauses in a background tab. `setInterval(60)` does not,
and it fights vsync.

Keep **rAF as the loop**. If you want the pane to work, start an interval
*fallback* only when a rAF watchdog sees zero frames for ~300ms. Do not
make `setInterval` the product.

Also, while you’re in there (same sitting, not a new phase):

- `getContext("2d", { alpha: false })` on bake and display
- `tabindex="-1"` on d-pad buttons so they don’t eat Space
- Don’t `preventDefault` keys that aren’t actually movement

## Not a fail (leave for P1)

- Checkerboard grass — readable P0 debug; kill when dressing
- Creek/Shore flood-filled with water — you can still stand on the labels
  via the ribbon, but a 3–4 tile **band** with banks is the real creek.
  Don’t redo the supermap for that until P1 dresses those regions
- `playerName` stored and never drawn (nameplates are P1/`passerby`)
- Desktop always shows the d-pad (fine for P0)

## Fail if “fixed” by

- Switching to a 1152×1280 DOM canvas
- Re-introducing rAF-only *and* leaving the name-field bug
- Starting P1 (notice-post, doors, passerby) in the same commit

Paste `CLAUDE_P0_PATCH.md`. One commit. Then stop.
