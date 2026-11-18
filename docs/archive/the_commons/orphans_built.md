# orphan watch, LOG-ONLY (rest slice 4b) — built, local-tested, not deployed

Brandon: "BUILD orphans log-only". Grok's `grok_rest_4_answer.md` valves: explicit
`exists:false` only; two consecutive passes; log-only first; cap 3; online backup.
**Civic (a small fifth named route) and daemon.** No client, nginx, schema or LGP1.
**There is no `DELETE` anywhere in this build**: it finds and logs, nothing more.
The reap is a separate, later BUILD after the journal has been read. No deploy.

## Why it exists

Civic lets a deleted username be registered again. Today the game keeps a deleted
player's `players`/`map_instances`/`placed`/`inventory` rows, so **whoever takes the
name would inherit the old player's house, wall item and inventory**. (Grok's
"orphans by design" was a deliberate not-yet, `grok_v6_answer.md`; this is the first
step toward closing it, and it deletes nothing.)

## What ships

- **Civic: `GET /internal/user-exists/<username>`** (one branch next to
  `/internal/blocked`). Same-box gate (plain 404 with `X-Forwarded-For` or a
  non-loopback peer). No cookie, no identity, read-only. `{"exists":true}` /
  `{"exists":false}` for a well-formed name; a **malformed name answers
  `{"exists":null}`, never `false`**, so nothing odd can read as "gone".
- **Daemon `civic::user_exists`:** tri-state `Exists | Gone | Unknown`. Only an
  explicit `exists:false` is `Gone`. Civic down/slow, `null`, a bad reply, a
  malformed name (checked before it goes into a request line) are all `Unknown`, and
  `Unknown` is never read as gone. (`civic.cpp` was refactored so the socket code is
  shared with the cookie-less call; the cookie routes behave the same.)
- **The pass** (`main.cpp`, on the existing accept-loop hook; no thread), every
  **~10 minutes**, first one an interval after start:
  1. every `players.lg_username` (guests have none), **skipping anyone currently in
     the game**, at most 50 checks;
  2. asks civic about each; **stops at the first `Unknown`** and forgets every streak
     (a dead civic costs one bounded wait, not one per player);
  3. **two consecutive passes** of `Gone` are required; anyone not gone this pass
     loses their streak;
  4. **at most 3** "would reap" per pass, the rest logged as deferred;
  5. **suspect guard:** if **every known account** (at least 2) is reported gone, the
     pass is treated as a civic problem, not a mass deletion, and counts nothing.
     (It is compared with every account in `players`, not just the ones asked about:
     I first compared against the *checked* set, and found before deploying that a
     living player who is online is skipped, so the four real test orphans would have
     read as "all checked accounts gone" and the watch would have logged nothing
     whenever Brandon was in the game. Fixed and retested with a living online
     player.)
- **What it logs** (journal, stderr): a startup line `orphan watch is LOG-ONLY ...`;
  `'<name>' is gone from civic (1st pass; needs 2 in a row)`; and from the second
  pass, `would reap '<name>': house instance N, X inventory rows (qty Q), Y placed
  rows, gone for K passes`, plus `deferred by the cap` and `SUSPECT`. The counts come
  from new **read-only** queries (`HouseStore::all_usernames`, `footprint_of`).
  While a name stays gone it repeats each pass (about six lines an hour per orphan)
  until the reap build; that is the price of a log-only phase.
- `COMMONS_TEST_TICK_MS` (tests only, >= 200) makes the hook and the pass run that
  often; unset in production, the interval is 600s.

## Tested (local, real civic, real deleted accounts, 1s test tick)

Five accounts each entered their house, picked up two lookups and hung chalk.
1. Route: alice `true`, unknown `false`, malformed `null`, with `X-Forwarded-For`
   404.
2. Everyone exists: several passes, **zero** gone/would-reap lines.
3. alice deletes her account through `/settings/delete`: `would reap 'alice': house
   instance 3, 2 inventory rows (qty 2), 1 placed rows` from the second pass on; **the DB
   counts are identical to the baseline** (players 5, map_instances 7, placed 5,
   inventory 10): nothing deleted.
4. **Civic frozen (SIGSTOP):** no would-reap lines during the freeze; after it, alice
   needs a fresh "1st pass" again (the streak was reset by "don't know").
5. alice's username **registered again**: exists, the streak clears, no more
   would-reap lines.
6. **Cap:** four gone (alice again, bob, carol, dave) and erin alive: exactly three
   would-reap lines and `1 more would-reap deferred by the cap of 3`.
7. **Suspect guard:** erin deleted too: `SUSPECT: civic reports all 5 known accounts
   gone`, and no would-reap. **And the reverse:** two deleted accounts plus one living
   player who is *online* (so never asked): both orphans are found, zero SUSPECT. Final DB counts still equal the baseline.
8. Regressions passing: the ~30s snapshot test (the hook I restructured), open
   houses (block probe, lots, fail-closed), session cache, rebind, visiting,
   persist-house.

## Deploy (when Brandon says `deploy`, after Grok's review)

**Civic first** (new route), then the daemon; **no client**. Drift-check the droplet's
`http_server.cpp` first (last time it equalled the previous git version). Online
SQLite copy first (the reap will need it anyway; it is cheap belt now). Live check:
public `/internal/user-exists/x` is **404**; on the box the route answers and refuses
`X-Forwarded-For`; the daemon journal shows `orphan watch is LOG-ONLY` at start.

**Then wait and read the journal.** The first pass runs about 10 minutes after the
restart and a name needs two in a row, so the first real "would reap" lines appear
about 20 minutes in. **Expect real ones:** the throwaway accounts deleted during earlier
live tests through `/settings/delete` left game rows on purpose ("orphans by design").
`journalctl -u commons-daemon | grep 'orphan watch'`. Those lines are the review
material for the reap BUILD; nothing is deleted meanwhile.

## The follow-up (not this build)

The reap BUILD adds one transaction per reaped name (inventory, that house's `placed`
rows, the house `map_instances` row, then the `players` row and anything pointing at
it), the same valves, an online SQLite backup first, and a per-pass cap of 3. It should
be named by Brandon after he has read the "would reap" lines and agrees with the list.

## Deployed (2026-09-19 10:17Z, Brandon: "deploy"; Grok accepted in `grok_rest_4b_review.md`)

Civic first, then the daemon. **No client, no nginx, no schema.**

1. **Civic drift check first:** the droplet's `http_server.cpp` md5 (`ad769b18...`)
   equalled the version before this round, so nothing was hand-edited live; the sync
   sent exactly that one civic file. Daemon: `civic.cpp/.hpp`, `house.cpp/.hpp`,
   `main.cpp`. Client dry-run: nothing.
2. **One fix before deploying (`81bc825`):** the suspect guard compared "gone" with
   the *checked* accounts, but online players are skipped, so with a living player in
   the game the real orphans would have read as "all checked accounts gone" and the
   watch would have logged nothing. It now compares with every known account
   (retested both ways). Found by reading the production `players` list before
   deploying: `persistverify9`, `vtowner`, `invverify1`, `Bbottin`, `ptvisitor`.
3. **Online game-DB backup first:**
   `/var/lib/commons/backups/commons.sqlite.pre-orphans-20260919T101314Z`, integrity ok
   (items 3, recipes 3, maps 14, map_instances 6, players 5, placed 3, inventory 4).
   Left on the box; the reap will want it.
4. **Civic** synced, `-j1` build (1m18s), `liquidgov` restarted. Public `/` and
   `/commons/` 200. **Public `/internal/user-exists/x` is 404** (as are
   `/internal/blocked/x` and `/internal/friends`). On the box: an unknown name answers
   `{"exists":false}`, a malformed one `{"exists":null}`, and `X-Forwarded-For` is a
   404. The accounts store's mtime is from before the deploy.
5. **Daemon** synced, `-j1` build, restarted at **10:17:21Z**; the journal shows
   `orphan watch is LOG-ONLY (every 600s, needs 2 consecutive explicit civic
   negatives, cap 3 per pass); nothing is deleted`. Live DB read-only: integrity ok,
   every count identical to the baseline. Through nginx: lots, `1,1`, house `/placed/`
   all 200; no failures; `NRestarts=0`.

**Timing:** the first pass runs at ~10:27Z and a name needs two in a row, so the first
`would reap` lines are due at ~10:37Z.

## The first real list (read 2026-09-19 10:37Z, two passes in)

Nothing deleted; DB counts identical to the pre-deploy baseline (map_instances 6,
players 5, placed 3, inventory 4). Journal:

| would reap | house instance | inventory rows (qty) | placed rows |
|---|---|---|---|
| `persistverify9` | 3 | 0 | 1 |
| `vtowner` | 4 | 0 | 1 |
| `invverify1` | 5 | 2 (2) | 1 |
| `ptvisitor` | 6 | 0 | 0 -- deferred by the cap of 3 this pass, due on the next (~10:47Z) |

`Bbottin` (Brandon's real account: 2 inventory rows, qty 3, no house instance) was
asked about, civic answered it exists, and it is **not** on the list. All four flagged
names are throwaway accounts deleted through `/settings/delete` during earlier live
tests. No SUSPECT line; `NRestarts=0`.

For the reap BUILD, Brandon should agree this list first. The reap is not built.


> Update 2026-09-19: the reap this doc deferred is now built -- see `orphans_reap_built.md`. Everything above describes the log-only phase that produced the first list.
