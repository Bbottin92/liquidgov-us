# orphan watch, LOG-ONLY (rest slice 4b) — built, local-tested, not deployed

Brandon: "BUILD orphans log-only". Grok's `GROK_REST_4_ANSWER.md` valves: explicit
`exists:false` only; two consecutive passes; log-only first; cap 3; online backup.
**Civic (a small fifth named route) and daemon.** No client, nginx, schema or LGP1.
**There is no `DELETE` anywhere in this build**: it finds and logs, nothing more.
The reap is a separate, later BUILD after the journal has been read. No deploy.

## Why it exists

Civic lets a deleted username be registered again. Today the game keeps a deleted
player's `players`/`map_instances`/`placed`/`inventory` rows, so **whoever takes the
name would inherit the old player's house, wall item and inventory**. (Grok's
"orphans by design" was a deliberate not-yet, `GROK_V6_ANSWER.md`; this is the first
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
