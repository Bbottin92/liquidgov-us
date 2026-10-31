# SQLite WAL — already on; it changes the inventory backup

Explore, not a BUILD. Do not flip journal mode. Do not add a checkpoint
thread. Civic frozen.

## What the daemon already does

`commons-daemon/src/db.cpp` on open:

```
PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;
```

WAL is **persistent**. Once set, the file comes back in WAL even if a
later binary omitted the pragma. Production `commons.sqlite` is already
WAL from the first house persist.

Writers append frames to `commons.sqlite-wal`. Readers (this daemon is
single-threaded, so that's mostly "the next request") see committed
frames without waiting on a rollback journal. `-shm` is the wal-index;
it is **not** durable content. `-wal` **is**.

`PRAGMA journal_mode=WAL` does not replace `BEGIN/COMMIT`.
`house_instance_for` and `set_wall_slot` still need the wrap.

## Why this exists (and why not to touch it)

`sibling.md` wanted concurrent read/write. We have one thread and one
writer. WAL is still the right default: a `kill` mid-request does not
leave a hot rollback journal against the main file the same way DELETE
mode can, and a second process (`sqlite3` CLI, a backup) can read while
the daemon holds the write lock.

Do **not** switch back to DELETE to make `cp` easier.
Do **not** add `journal_mode=MEMORY` or `synchronous=OFF`.
Do **not** grow a checkpoint worker. Square-scale WAL stays small.

## The inventory deploy backup (this is the point)

`GROK_INVENTORY_REVIEW.md` said copy `commons.sqlite` then restart.
That instruction is incomplete.

- A **live** `cp` of the main file misses frames still in `-wal` (recent
  pickups, wall slots, `updated_at` ALTER is in the main file once
  checkpointed — uncheckpointed DML is not).
- `commons-daemon` `run()` has **no SIGTERM handler**. `systemctl stop`
  kills the accept loop without `sqlite3_close`. SQLite then **leaves
  `-wal` and `-shm` on disk**. After stop, the durable state is still
  three files, not one.
- A clean `sqlite3_close` (process exit via destructor, or a future
  signal path) checkpoints and unlinks `-wal`/`-shm`. Only then is the
  main file a complete copy.

**Deploy copy, in this order:**

1. `systemctl stop commons-daemon`.
2. Copy **all three** if they exist:
   `commons.sqlite`, `commons.sqlite-wal`, `commons.sqlite-shm`
   → `*.bak-inventory`.
   Do not copy only the main file and call it a backup.
3. Rebuild. `systemctl start` the new binary (migration runs on open).
4. Confirm `/health`. Confirm a second restart is clean.
5. Then sync the client.

Restore later: stop, delete live main+wal+shm, copy the three bak files
back (or the single file from a `VACUUM INTO` snapshot), start.

Better, if anyone is already in `sqlite3` on the box: `VACUUM INTO
'/var/lib/commons/commons.bak-inventory.sqlite'` against a **stopped**
or quiet DB — one self-contained file, no sidecars. Not required this
deploy if the three-file copy is done after stop.

`sqlite3_backup` / online backup is the always-safe live snapshot. Not
this week. Don't add it to `index.html` or civic.

## Fail if "explored" by

- `PRAGMA journal_mode=DELETE` to make cp simpler
- Checkpoint-every-request
- Copying only the main file while the daemon is running
- Enabling WAL a second time as a new feature
