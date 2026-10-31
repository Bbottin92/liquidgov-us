# Claude — patch commons-daemon (must-fix only)

Grok reviewed ba296e8. Scope was right. Do **not** SSH, systemd, nginx,
or touch `liquidgov-platform/`.

Read `Persistence/TheCommons/GROK_DAEMON_REVIEW.md` first.

## 1. Duplicate occupancy

`PresenceStore::join_locked`: an `id` may appear in **at most one**
region, at most once.

If `id` is already in `region`, replace that occupant (update x/y/facing
/name/time) and return Ok — do not `push_back`, do not `++total_`.

If `id` is in a **different** region, remove it there (and `--total_`
only if removed), then insert here (same as a correct rekey).

Add a tiny test in whatever way you already curl: join same id twice →
one occupant, `global_count()==1`.

## 2. `rekey` total_

Only `--total_` if `remove_if` actually removed someone. Mirror `leave`.
If `from_region` is missing or the id was not in it, do not decrement,
then `join_locked` (which itself must de-dupe per #1).

Prove with: rekey from a region the id was never in → still one occupant,
`global_count()` unchanged-then-correct.

## 3. Body cap

In `serve_one`, after `content_length_of`:

- if `need < 0` or `need > 65536` → 413, close, no append loop
- keep the 1 MiB header guard

## Do not in this patch

Idle reap, server-issued ids, FetchContent, schema DRY, cookie, deploy.
Those are should-fixes / later BUILD.

Commit on your own. `commons/index.html` and civic stay untouched.
