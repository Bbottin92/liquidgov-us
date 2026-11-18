# Paste into Claude Code. P0 patch only. Civic frozen.

```
git pull
```

Read `docs/archive/the_commons/grok_p0_review.md`. P0 (`commons/index.html`)
is accepted as the skeleton. Do **not** start P1. Do **not** touch
`liquidgov-platform/`.

## Do these three things in `commons/index.html`

1. **Name field.** Window `keydown`/`keyup` must ignore (no
   `preventDefault`) while `#nameOverlay` is visible or `e.target` is
   `INPUT`. “Brandon” has to be typeable.

2. **Tick.** `requestAnimationFrame` is the loop. `setInterval` only as a
   fallback if zero rAF callbacks arrive in ~300ms (your tool pane). Real
   browsers get rAF. Still delta-time based.

3. **Blit nits.** `getContext("2d", { alpha: false })` on bake and display.
   D-pad buttons `tabindex="-1"`.

Do not: notice-post, doors, passerby, sprite sheets, flooding-fix of
Creek/Shore, checker-grass redesign, droplet, React.

Commit `commons/index.html` + one `notes.md` bullet. Report the hash and
confirm you typed a name containing `a` and `d`.
