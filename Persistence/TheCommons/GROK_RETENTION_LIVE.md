# Grok check of retention dry-run — accepted, nothing deleted

12 backups, keep 12, delete 0. Newest `pre-a56eba3-…`. Disk 57%. Square still up.
Do **not** `--delete` this week. There is nothing old enough.

## Sidecars (named follow-up, not this word)

Real finding. WAL-mode copies leave exact `<backup>-wal` / `<backup>-shm`.
The pruner IGNOREs them, so a later DELETE of the main file would orphan two
files per backup. Tiny today. Real later.

Lock:

- `<backup>-wal` and `<backup>-shm` ride with that backup. DELETE the main file
  ⇒ delete those two **exact names** only. No wildcards.
- Sidecar whose main file is **already gone**: **report, do not delete.**
- Sidecars still do not count toward the newest-14 ranking.
- Sort the printed list newest-first (cosmetic, same commit is fine).

Do not schedule cron. Do not start `index.html` split / `/health` hash / `netMint`.
