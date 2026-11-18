# The ~30s snapshot — built, local-tested, not deployed

Brandon: "BUILD 30s snapshot". Spec is `presence.md`: "Periodic safety
snapshot, ~every 30s, of bound players only, for crash recovery ... not a
general persistence path." Deferred in `persist_house_built.md`; Grok's
`grok_v6_answer.md` had it as "No, leave/rekey cover real play" until named.
**Daemon only.** Client, civic, nginx, schema untouched. No deploy.

## What it does

Every ~30s the daemon writes the position of each **bound player who is
standing in their own house** onto their `players` row (`snapshot_leave`,
the same write leave and rekey-away already use). Before this, a crash or
kill while someone stood in their house resumed them from wherever they last
walked *out*; now it is at most ~30s stale.

Same guard as leave, nothing wider: owner's own house only, bound only. No
snapshot for guests, visitors in a friend's house, the shared demo/lab/mine,
or outdoors. No new column, no new table, no new endpoint.

## How (no thread)

The daemon is a single-threaded synchronous accept loop (deliberately, like
civic). Rather than add a thread and locks around `HouseStore`'s SQLite
handle:

- `HttpServer::set_periodic(interval_ms, fn)` (`http.hpp/.cpp`): `run()` now
  `poll()`s the listen socket with a timeout of "time until the next tick",
  and runs `fn` on the same thread **between** requests, never during one. The
  due-check runs on every pass, so a steady request stream can't starve it. An
  exception in `fn` is caught and logged, not fatal. When no hook is set the
  loop is the old blocking `accept()`.
- `main.cpp`: a 30 000 ms hook walks `presence.bound_usernames()`, and for
  each `u:<name>` occupant found in `instance:house:<name>` calls
  `house.snapshot_leave(...)`. `house.hpp`'s comment updated to say so.

`presence.occupants()` reaps idle rows (30s) as it always did, so someone who
has stopped sending moves is not snapshotted stale forever, and the write is
one `UPDATE` per bound player in their house per 30s (cap 96 players total).

## Tested (local, real civic + daemon, scratch store)

Signed up a bound account on the scratch civic; crashes are `kill -9` (no
leave, no rekey, nothing graceful).

1. **Control:** join own house, move to (3,9,f2), kill -9 within ~2s (before
   any tick), restart, rejoin: **no** `resume_*`. Nothing had been
   snapshotted, so the test can actually tell the difference.
2. **Real:** join, walk to (5,4,f1), keep moving ~35s with no leave, kill -9,
   restart, rejoin: **`resume_x:5, resume_y:4, resume_facing:1`.** Daemon
   was still serving requests throughout.
3. **Guard:** the same account standing in the shared `instance:demo` at
   (9,9,f3) for 35s, then kill -9: rejoining the house still resumes
   (5,4,f1), **not** (9,9,f3).
4. **Regression:** `persist_house_test.sh` (leave snapshot, wall slot across
   restart, intruder/guest 403, demo stays RAM) unchanged and passing. No
   stray processes left.

Not exercised: a real production tick (that is the deploy check), and two
bound players in their houses at once (the loop is per-name, no shared
state).

## Deploy (when Brandon says `deploy`, after Grok reviews)

**Daemon only, then nothing else** -- there is no client change. Rebuild +
restart the daemon on the droplet (`sync-to-droplet.sh`, `-j1`, chown,
`systemctl restart commons-daemon`). No schema change, so no backup needed,
but an online copy of the SQLite file is cheap insurance. Restart drops RAM
presence (everyone rejoins, house state is durable), same as every daemon
deploy. Live check: a bound player in their own house, `kill -9`-free proof
is not possible on prod, so the check is that requests stay healthy across a
tick window (`/presence/lots`, `/presence/<house>`) and the daemon journal
shows no `periodic task failed`.

## Not here

`open`/`locked`, `rebind()`, `lg_session` cache, orphan reaping, the
`sqlite3_step` result checks in `house.cpp`: all still parked.

## Deployed (2026-09-19, Brandon: "deploy"; Grok accepted in `grok_snapshot_review.md`)

Daemon only. Client, civic, nginx untouched (`liquidgov.service` never
restarted).

1. **Drift check:** a dry-run of the sync listed content changes for exactly
   the four files this round edited (`house.hpp`, `http.hpp`, `http.cpp`,
   `main.cpp`); everything else differed only in owner/group.
2. **Online backup first** (SQLite backup API, not a file copy):
   `/var/lib/commons/backups/commons.sqlite.pre-snapshot-20260919T041014Z`,
   `integrity_check` ok. Baseline counts: items 3, recipes 3, maps 14,
   map_instances 6, players 5, placed 3, inventory 4. Left on the box.
3. **Sync** (`sync-to-droplet.sh`), **build** on the droplet (`-j1`; only
   `main`, `http`, `house` recompiled), chown, `systemctl restart
   commons-daemon` (restart drops RAM presence; houses are durable).
4. **Live check across a 40s window (> one 30s tick), through nginx:**
   `/commons/presence/lots`, `/presence/1,1` and a `/placed/instance:house:...`
   GET all 200 every 5s; public `/internal/friends` 404; journal has no
   `periodic`/error/fail lines; `NRestarts=0`; live DB `integrity_check` ok and
   every row count identical to the baseline; daemon still bound to
   `127.0.0.1:8765` only.

Not observable from outside: a real tick writing a real bound player. That
needs someone bound standing in their own house at a tick, and the proof is
the resume position after a daemon restart. Nobody has done that on
production yet; it is the same code path the `kill -9` test exercised locally.
