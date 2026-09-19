# Backup retention — built (files + fake-backup tests; NOT run on the droplet)

Brandon: "BUILD backup retention: keep newest 14, drop older than 30 days, never the newest, never a pre-* younger than
24h. Files + fake-backup tests. No droplet. No deploy." Locked interpretation (Brandon): **delete only if it is BOTH
outside the newest 14 AND older than 30 days**; the newest 14 stay however old, anything within 30 days stays however
many; dry-run by default, an explicit flag deletes. **No droplet was contacted; nothing was deleted anywhere.**

## What ships (`commons-daemon/deploy/`)

- `prune-backups.py`: the decision as a **pure function** (no file access): input `<mtime> <name>` lines, output a verdict
  per file. Unit-testable to death, and it is.
- `prune-backups.sh [--delete] [--keep N] [--days D]`: lists `/var/lib/commons/backups` over ssh, runs the decision,
  prints every file's verdict and the total it would free; **dry run unless `--delete`**. With `--delete` it re-validates
  every name against the backup pattern before `rm --`, then re-lists and checks that every backup marked KEEP is still there.
  Not scheduled anywhere and not called by `deploy-daemon.sh`: retention is a separate, deliberate step.

## The two naming schemes, and how the policy treats each

I inspected both (`main.cpp` `orphan_backup`, `backup-db.sh`) and the ones already in the notes. They are **one scheme with
two producers**, `commons.sqlite.pre-<label>-<UTC YYYYmmddTHHMMSSZ>`:

| producer | label | example |
|---|---|---|
| deploy script (`backup-db.sh`) | a short sha or a word | `commons.sqlite.pre-a56eba3-20260919T152437Z`, `...pre-hardening-...` |
| the daemon's automatic pre-reap backup | `reap` | `commons.sqlite.pre-reap-20260919T115026Z` |
| hand-made earlier deploys | a word | `...pre-reap-manual-...`, `...pre-house-policy-...`, `...pre-orphans-...` |

**The policy treats them identically**: one combined ranking by time, so "the newest 14" means the newest 14 backups of
any producer (not 14 of each). Every real backup here is a `pre-*`, so "never a pre-* younger than 24h" applies to all of
them. **Note the redundancy, said plainly:** at the locked defaults the 30-day rule already keeps anything younger than a
month, so the 24h rule can never be the one that saves a file. It is implemented as an *independent hard floor*, so it still
holds if someone runs `--days 0` (tested: `--keep 0 --days 0 --delete` keeps the newest and everything younger than 24h).
The alternative (14 of each producer) would keep more; the combined reading was chosen as the literal one and is a one-line
change if wanted. Anything in the directory that is **not** a `commons.sqlite.pre-<label>-<stamp>` regular file (`-wal`,
temp files, other names, directories) is IGNOREd: never counted, never touched.

## The rules, in the order they are applied to each backup (newest first)

1. the newest backup: KEEP.  2. rank <= 14: KEEP.  3. younger than 24h: KEEP (hard floor).  4. age <= 30 days: KEEP
(exactly 30 days is kept; "older than", not "at least").  5. otherwise DELETE.

**Age and rank use the younger of the timestamp in the name and the file's mtime**, so a file only looks old if BOTH say so:
a backup that was restored, copied or touched recently is kept even if its name is old.

**Refusals** (nothing is deleted, and it says why): the listing fails (missing directory, ssh down), which is an error and
never "empty"; a backup dated more than an hour in the future (a wrong clock or file), exit 3. `--keep`/`--days` must be
whole numbers; a more aggressive value than the locked 14/30 prints a WARNING.

## Tests (in CI via `run_all.sh`)

`tests/prune_backups_unit.py` (**361 checks**): today's droplet (nothing deleted), a 34-backup spread (exactly the newest 14
kept, 20 deleted, including 40-day-old ones kept because they are among the newest 14), 20 ancient (14 kept), 160 within 20
days (none deleted), the 30-day boundary to the second, the newest kept even when ancient or with `--keep 0`, the 24h floor
with `--days 0`, the mtime/name age rule both ways with a control, both producers ranked together, non-backup names ignored
and not counted, the future-date anomaly, and **300 random directories** checking the invariants (newest kept, >= min(n,14)
kept, every DELETE is outside the newest 14 and older than 30 days and 24h, non-backups never deleted, pruning twice deletes
nothing more). `tests/test_retention.sh` (**47 assertions**) drives the real script over a shim ssh against a real temp
directory with controlled mtimes: dry run by default and removes nothing; `--delete` removes exactly the right 20 and leaves
the 14 newest, the non-backups and a look-alike directory; a second run is a no-op; the touched old file survives; a missing
directory and an anomaly delete nothing; overrides warn; bad flags are refused. **Mutation-checked, 12 breakages** (no
30-day condition, no newest-14 condition, newest not special, no 24h floor, name-only age, mtime-only age, "at least 30",
no future-date check, a loosened name pattern, deletes by default, a failed listing treated as empty, an anomaly not
blocking): each fails the test. The file-level `shellcheck` step in the workflow lints the new script too.

## Limits, said plainly

- **Never run against the droplet.** The first real use is a dry run: `deploy/prune-backups.sh` (no flags) on `deploy`, read
  the verdict list, then `--delete` on a second `deploy` if it looks right. **Today it would delete nothing**: the droplet's
  backups are all from the last two days (about a dozen), so nothing is outside the newest 14 and older than 30 days for
  about a month; there is no urgency.
- Sizes: each backup is about the size of `commons.sqlite`, which is small (`players` has 1 row); the directory is nowhere
  near a disk concern yet. Retention is housekeeping, not an emergency.
- Not scheduled (no cron/systemd timer). Whether and when to schedule it is its own decision.
- Nothing else is left unbuilt from Grok's list except optional polish (`index.html` split, `/health` commit hash, the
  client's `netMint` status check).

## First real run: a dry run on the droplet (2026-09-19 ~16:10Z, Brandon: `deploy`)

Read-only: `prune-backups.sh` with no flags (a listing over ssh, nothing written). Result: **12 backups, keep 12, delete 0**
(the newest is `pre-a56eba3-20260919T152437Z`; the rest rank #2-#12, all from the last ~24h, so all are inside the newest 14 and
inside 30 days). The directory is 1.4M on a disk that is 57% used. Nothing was deleted; `--delete` was not passed.

**A real finding the fake-directory tests could not have shown: every backup has `-wal` and `-shm` sidecar files.** 24 of the 36
files in the directory are `commons.sqlite.pre-...Z-wal` (0 bytes) and `...Z-shm` (32 KB). A backup is a WAL-mode database, so
anything that opens it (the integrity check inside `backup_to`/`backup-db.sh`, and `verify.sh`'s read-only compare) creates them.
They are tiny (about 0.4 MB in total) and harmless, but the pruner **IGNOREs them by design (not a backup name), so when it
deletes an old backup its `-wal`/`-shm` would be left behind as orphans and accumulate**, two per deleted backup.

**Proposed follow-up (NOT built, needs Brandon to name it):** treat `<backup>-wal` and `<backup>-shm` as part of that backup:
delete them together with a DELETE'd backup (exact-name only, never a wildcard), and report (not delete, or delete only when
older than 30 days) sidecars whose main file is gone. Changing the policy's ignore rule is a decision, so it was not done here.
Cosmetic: the dry-run list is in directory order, not newest-first; sorting the printout by rank would read better.
