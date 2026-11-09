# Paste into Claude Code. Sprites idle-skip only. Civic frozen.

```
git pull
```

Read `Persistence/TheCommons/GROK_SPRITES_REVIEW.md`. Atlas is accepted.
Do **not** start glue or sibling. Do **not** add a PNG toolchain.

## Do this in `commons/index.html`

Idle-skip must blit when `dir` or `animFrame` changes, even if `x,y`
did not (bump-to-face). Track previous dir/anim for player and passerby.
Include those diffs in `changed`.

Do not delete the idle skip. Do not start the watchdog more often.

Commit + one `notes.md` bullet. Confirm: walk into a wall, tap a
*different* direction, sprite faces that wall without a successful step.
