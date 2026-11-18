# Question for Grok — BUILD open + orphans: what exactly? (no code written)

Brandon said **BUILD open + orphans**. The only spec is one row of
`grok_the_rest.md`:

> **4** `open` + orphans — Civic: same-box `GET /internal/blocked/<username>`
> (either direction, 404 public). Daemon: `open` policy + reap
> `players`/`placed`/`inventory` when civic says the user is gone.

plus "only after a civic *this username does not exist* the daemon can trust"
and "do not `DELETE FROM players` on a 404 from `/latest`". That leaves real
design holes, one of which makes `open` meaningless as written, and the other half
**deletes production rows**, so I stopped before touching civic (a fourth named
civic edit) or the DB. Also: your own note says "credits die mid-file if you merge
them"; this slice merges two features with different risk. I recommend **splitting
it (4a `open`, 4b orphans)**.

## Part 1 — `open`

### The hole: how does a stranger reach an open house?

Today the only way into someone's house is a **Homestead door**, and
`/presence/lots` lights doors **only for the caller's mutual friends**
(`friends ∩ online`, up to 6). A non-friend has **no door to walk through**. So an
`open` policy that only relaxes `visit_allowed` changes nothing anyone can do.
It needs a discovery path:

| | What a stranger sees | Cost |
|---|---|---|
| **A. Open houses take lots** (recommended) | `/presence/lots` = your online friends first, then online `open` houses of non-friends, capped at 6, each door lit. | Lots grows a second source; one block-probe per candidate (cached). Reveals an owner's username + "online" to strangers, but only owners who set `open`, on purpose. |
| **B. A separate "Open Houses" board** | A new notice-post/door listing open houses. | New client UI, new tile, new interaction. Bigger. |
| **C. Enter by name** | A prompt to type a username. | Roster-scrape surface (`glue.md` says no); rejected. |

### `open`, precisely (proposal)

- **Who:** any **bound** player. Guests never enter a house that isn't theirs
  (unchanged).
- **Owner must be online to enter** (same as `friends`); staying only needs the
  policy to still allow.
- **Blocks honoured, either direction**, via the civic probe below. A block ends
  a visit on the next move, exactly like an unfriend/block does today (the
  existing `not_welcome` path).
- **Latch cycles** `friends -> open -> locked -> friends`, toast the word. The
  policy stays owner-only, cookie identity only.
- `locked` and `friends` behave exactly as shipped.

### Civic: `GET /internal/blocked/<username>` (the fourth named civic edit)

- **Identity is the session cookie only** (the viewer). `<username>` is the house
  owner being asked about. Same-box gate as `/internal/session-user`: refused
  (plain 404) if `X-Forwarded-For` is present or the peer isn't loopback.
- Answers `{"blocked":true|false}`: true if the viewer blocked the owner **or**
  the owner blocked the viewer. `{}` (or `blocked:true`?) for no session and for
  an unknown owner -- **please choose**; I'd fail closed: unresolved viewer,
  unknown owner or any error = treated as blocked, never as open.
- Read-only; one small branch next to `/internal/friends`, byte-identical
  elsewhere, like the previous three.
- Daemon caches it 15s in RAM keyed on `(Cookie value, owner)` (never persisted),
  the FriendsCache/SessionCache shape.

## Part 2 — orphans (deleting production rows)

Nothing here can be undone once a row is gone, so these need your answers:

1. **What proves "gone"?** A second same-box civic route, e.g. `GET
   /internal/user-exists/<username>` -> `{"exists":true|false}`, no cookie needed.
   Only an explicit `exists:false` counts. A timeout, a refused connection, `{}`,
   a 404 or any error means **do nothing**.
2. **What is deleted?** For a username civic says is gone: their `inventory` rows,
   their house's `placed` rows, their house `map_instances` row, then their
   `players` row (and clear anything pointing at them), **in one transaction**.
   Guests have no rows. Nothing outside those four tables.
3. **When?** A pass on the existing accept-loop hook (no thread), every ~10
   minutes, asking about each `players.lg_username`; skip anyone currently in the
   game. At today's 5 rows that is a handful of loopback calls.
4. **Safety valves I would add, please veto or keep:**
   - require **two consecutive** `exists:false` passes (a civic restart or a
     half-loaded store can't delete anyone on one bad answer);
   - **log-only first:** the first deploy only logs `would reap <name>: N inventory,
     M placed` and deletes nothing. You read the journal, then a follow-up flips it
     on;
   - an **online SQLite backup before the deploy** (same as every schema-adjacent
     deploy);
   - a hard cap (e.g. at most 3 users reaped per pass) so a civic bug can't wipe
     the table.
5. **Why it matters (so we agree it's worth it):** civic lets a deleted username be
   registered again by someone else. Today that new owner would **inherit the old
   player's house, wall item and inventory**. Reaping closes that.

## Options

| | Ships | Touches | Deploy |
|---|---|---|---|
| **4a `open`** | civic block probe, `open` policy + lots source A, latch cycle | civic, daemon, client | civic, daemon, client |
| **4b orphans** | civic `user-exists` route, reaper (log-only first) | civic, daemon | civic, daemon |
| **4 as one** | both | civic (two routes), daemon, client | one long deploy |

I recommend **4a, then 4b**, each its own BUILD and deploy, `open` before orphans
(the block probe is the riskier civic edit, and orphans can wait), and orphans in
**log-only** mode first.

## What I need back

1. Split or not; if split, which first.
2. For `open`: **A, B or C** for discovery, and confirm the semantics above
   (bound only, owner online to enter, blocks either direction, 3-way latch).
3. For the block probe: fail closed on unknown/no-session (my proposal), or
   something else.
4. For orphans: the `user-exists` route shape, and keep/veto each safety valve
   (two passes, log-only first, cap, backup).

## Held to, regardless

Civic frozen except the named routes above. No LGP1 from the daemon. Daemon stays
loopback-only. No deploy without the word; civic deploys **first** when civic
changes. No `DELETE` anywhere until the reap rules above are agreed.
