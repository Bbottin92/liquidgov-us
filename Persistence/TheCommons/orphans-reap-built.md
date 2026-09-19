# orphan reap — built, local-tested, not deployed

Brandon: "BUILD orphans reap", after reading the first log-only list
(`orphans-built.md`: `persistverify9`, `vtowner`, `invverify1`, `ptvisitor`, all
throwaway accounts deleted through `/settings/delete`; his real `Bbottin` was not
on it). **Daemon only.** No civic, client, nginx or schema change. No deploy. This is
the first build that **deletes rows**, so the valves are the whole point.

## What it does

The existing ~10-minute orphan pass, when a name qualifies, now deletes that name's
game rows instead of only logging them.

**Still required before anything is deleted (all of the log-only rules, unchanged):**
civic must answer an explicit `{"exists":false}` (anything else, including civic
down or slow, is "don't know" and forgets the streak); the name must be gone on
**two consecutive passes**; **at most 3 per pass** (the rest wait for the next);
anyone currently in the game is skipped; and the suspect guard (every known
account "gone" counts nothing) still applies.

**New, before each delete:**
1. **An online backup first, automatically.** `Database::backup_to` (the SQLite backup
   API, never a file copy) writes `<db dir>/backups/commons.sqlite.pre-reap-<UTC
   stamp>` and verifies it with `integrity_check`. **No backup, no reap:** if it
   fails the pass logs `BACKUP FAILED; reaping nothing this pass` and touches nothing.
   (One backup per pass that reaps.)
2. **One last look:** just before deleting a name, civic is asked **again**; it must
   still say gone and the player must not have come online since the pass began,
   otherwise `NOT reaping ...` and the streak is dropped.
3. **One transaction per name** (`HouseStore::reap_orphan`, `sqlx::Tx`): that
   player's `inventory` rows, every `placed` row in their house, their `players`
   row, then their house's `map_instances` row **only if it is owned by them**;
   any stale `last_instance_id` pointing at that house is cleared first. It **rolls
   back whole** on any failure (logged, nothing changes for that name). Nothing else
   in any table is touched; demo/lab/mine are RAM-only and were never rows.
4. **Kill switch:** `COMMONS_ORPHAN_REAP=0` (or `off`/`false`) in the daemon's
   environment puts it back to log-only. Default is ON, as requested.

Logs: `orphan watch is REAPING (...)` at start; `backup taken before reaping: <path>`;
`REAPED '<name>': N inventory rows, M placed rows, house row removed, players row
removed (gone for K passes)`; `NOT reaping ...`; `BACKUP FAILED`; `reap of '<name>'
FAILED and was rolled back`; `deferred by the cap`.

## Tested (local, real civic, real deleted accounts, 1s test tick)

Seven accounts each with a house, two lookups and a wall item.
1. Everyone exists: passes run; **zero** reaps, **no** backup taken.
2. Four deleted (`usera`..`userd`), three alive (`keepr`, `usere`, `userf`): one
   backup, then `usera`/`userb`/`userc` reaped (2 inventory + 1 placed + house row +
   players row each), `1 more deferred by the cap`; the next pass takes a **second**
   backup and reaps `userd`. **The living three are untouched** (`keepr`'s rows are
   byte-for-byte the baseline). Totals 7/9/7/14 -> 3/5/3/6 players/instances/placed/
   inventory, `integrity_check` ok. **Each backup is a valid DB that still holds the
   pre-reap rows** (7 players/14 inventory, then 4/8).
3. **The inheritance hole is closed:** a new owner registers the name `usera`: their
   house wall is empty, their inventory `[]`, a fresh house instance.
4. **Civic frozen (SIGSTOP) the instant after a delete:** the row is still there after
   6s and nothing is reaped; unfrozen, the streak was forgotten and it is reaped only
   after two fresh passes. (A first version of this phase froze civic too late; the
   two 1-second passes had already run. Redone correctly.)
5. **No backup, no reap:** the backups dir made unwritable: `BACKUP FAILED; reaping
   nothing`, the row survives; dir writable again, it is reaped.
6. **Suspect:** every known account gone -> `SUSPECT`, nothing reaped.
7. **Kill switch:** `COMMONS_ORPHAN_REAP=0`: the deleted account's rows stay, only
   `would reap` is logged (`REAPED` count 0); switch removed, the same row is reaped.
8. A living player who is *online* while others are deleted: the deleted are reaped,
   no false SUSPECT.
Regressions passing: the ~30s snapshot test (kill -9), open houses (lots, blocks,
fail-closed), visiting, persist-house, rebind. (My first reap run used one-letter
usernames that civic rejects, so it exercised nothing; the count of `REAPED` lines was
0, no rows were harmed, and I redid it with valid names before trusting any result.)

## Deploy (when Brandon says `deploy`, after Grok's review)

**Daemon only.** Take the usual manual online SQLite copy first as well (belt on top of
the automatic per-pass backup). `-j1` build, no reconfigure, restart; the journal
should say `orphan watch is REAPING`.

**What will happen, and when:** the restart resets the RAM streaks, so the first pass
(~10 min after start) only logs `is gone (1st pass)` for the four; the second pass
(~20 min) backs up, then **reaps `persistverify9`, `vtowner` and `invverify1`** (cap 3),
and the next pass (~30 min) reaps **`ptvisitor`**. **`Bbottin` is not touched** (civic
says it exists). Watch with `journalctl -u commons-daemon | grep 'orphan watch'`;
`/var/lib/commons/backups/commons.sqlite.pre-reap-*` will hold the pre-reap copies.
After it: `players` should be exactly `Bbottin`. To stop it at any point, set
`COMMONS_ORPHAN_REAP=0` and restart.

## Not here

Any other cleanup (guest data, unused house rows for live accounts): unchanged.
