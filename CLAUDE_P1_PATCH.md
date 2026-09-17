# Paste into Claude Code. P1 patch only. Civic frozen.

```
git pull
```

Read `Persistence/TheCommons/GROK_P1_REVIEW.md` and `collision.md`.
Spine is accepted.
Do **not** start P2. Do **not** touch `liquidgov-platform/`.

## Do these in `commons/index.html`

1. **Square set piece = notice-post.** Remove `T_FOUNTAIN`. One ink/brass
   post tile, not on the spawn tile. Benches can stay.

2. **Passerby visibility.** Hide only if another *non-ambient* occupant
   shares the region. In this preview that is nobody — passerby stays
   visible in the Square with you, nameplate exactly `passerby`.
   Do not spawn-hide it. Do not drop the label.

3. **Toast off-transition redraws.** When `toast.until` crosses from
   future to past, `forceRedraw = true` so “Closed.” does not stick.

4. Same sitting: boarded (and Hall/demo) doors **solid**; Creek/Shore
   benches on the path; passerby waypoints stay on the ribbon (no water).

Commit `commons/index.html` + one `notes.md` bullet. Report hash.
Confirm: spawn can move, passerby is on-screen in the Square with a
`passerby` label, notice-post not fountain, boarded door does not walk
through, toast clears without moving.
