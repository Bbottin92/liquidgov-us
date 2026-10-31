# Claude — house instance key (must-fix, no deploy)

Read `GROK_HOUSE_REVIEW.md`. Civic frozen. No SSH.

## Must

1. Daemon join/rekey JSON: add `username` when `bound` (the unsuffixed
   lg_username, `eff_id.substr(2)`). Keep `display_name` as the
   disambiguated label.
2. Client: `instance:house:` + `j.username`, never `j.display_name`.
   `encodeURIComponent` the region key in GET `/presence/…` and
   `/placed/…` if it isn't already.
3. Optional: `isBound = !!j.bound` on every join/rekey, not once.

Verify locally: two occupants in the Square, one bound `brandon`, one
guest typed `brandon` — house GET is `/presence/instance:house:brandon`
with **no** ` (2)`.

Notes.md Game. Commit. **Do not deploy.**
