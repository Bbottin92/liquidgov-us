# Automated backup strategies — we already have the one that is safe

Explore, not a BUILD. The orphan **reap is live** (`GROK_ORPHAN_REAP_LIVE.md`).
Do not add a cron, a second daemon, or a `cp` of `commons.sqlite` while that
clock is running.

## What we already do

SQLite's **online Backup API** (`sqlite3_backup_*`, `Database::backup_to`):

- Works while the daemon holds the DB (WAL or not).
- Destination is a consistent snapshot, then `integrity_check`.
- Paths we already write: `pre-inventory-*`, `pre-snapshot-*`,
  `pre-house-policy-*`, `pre-reap-manual-*`, `pre-reap-<UTC>` (automatic,
  **no backup → no reap**).

That is the strategy. A shell `cp` of a live `*.sqlite` (+ missing `-wal`)
is a torn file. `sqlite-wal.md` already said so.

## Families people reach for (and why not here)

| Idea | Verdict |
|---|---|
| `cp` / `rsync` the file every N min | Torn page unless the DB is quiescent. |
| `VACUUM INTO` | Needs a write lock; worse than Backup API for a live game. |
| Dump to SQL (`sqlite3 .dump`) | Huge, slow, not what we restore. |
| Litestream / sqlite-rsync | Extra process, extra failure mode, droplet is 512 MB. |
| Off-box S3 cron | Secrets in the unit, civic-adjacent. Named BUILD only, not this. |
| Backup civic LGP1 from the game daemon | Never. Different store, frozen civic. |

## If we ever automate *more*

Keep using `backup_to` on the existing accept-loop hook (same thread as the
reap). Cadence: **daily is plenty** (houses are tiny). Cap retained files
(e.g. last 14 `pre-daily-*`) so `/var/lib/commons/backups` cannot fill the
512 MB box. Restore = stop daemon, `integrity_check` the chosen file, copy
over, start. That is a **named BUILD**, not implied by this note.

## Fail if "explored" by

- `cp /var/lib/commons/commons.sqlite` in cron
- Litestream, rclone, a systemd timer this week
- Backing up LGP1 from commons-daemon
- Changing the reap kill switch to "also backup civic"

Leave the Backup API. Leave the reap clock alone.
