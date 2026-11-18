# rebind — built, local-tested, not deployed

Brandon: "BUILD rebind" (`grok_the_rest.md` slice **3**: "Mid-session login
promotes `g_...` -> `u:` in the current region without waiting for a door. No
ghost."; the design record is `cookie_bind.md` "Promotion, not a
disconnect/rejoin", which named the in-place `PresenceStore::rebind` as the
literal reading and left the choice open). Daemon + a small client change. Civic,
nginx, schema, LGP1 untouched. No deploy.

## The gap this closes

A guest who logs in on LiquidGov **without leaving the game** kept a `g_...` row
until their next join/rekey (their next door). It worked (no ghost, no reject),
but they were still a guest to everyone else: no house door, no bound nameplate,
and once they did cross a door the row was replaced rather than relabelled.

## What changed

- **`PresenceStore::rebind(region, old_id, new_id, display_name)`**: relabels the
  row **in place**. id and display_name change; `x`, `y`, `facing` and
  `last_update_ms` are untouched, no cap slot is spent, nobody sees a gap. The
  display name is disambiguated within the region against everyone else ("alice
  (2)" if a guest already shows as "alice"). If `new_id` already has a row
  somewhere (a second tab of the same account) that older row is superseded, as
  `join()` already does: newer wins, one id one row.
- **`POST /presence/move`** promotes. If the cookie now resolves, the bound id has
  no row in this region, **and the client's guest secret still verifies** (the
  same proof `bind_or_promote` asks for at a join), the row is rebound and the
  move proceeds as the bound player. It never happens in a house region (a guest
  can't be in one, and promotion must not become a way around the owner/visitor
  checks join and rekey make). The answer carries `promoted:true`, `bound:true`,
  `username`, `display_name`.
- **Client (`commons/index.html`):** the join-answer bookkeeping is now one
  function, `netApplyJoinJson(j)`, also applied to a move answer that says
  `promoted`. It sets `isBound`, the bound id (so the client's own new `u:` row is
  filtered out of "other players", i.e. the self-ghost fix keeps working), the
  accepted nameplate, the house key, and loads saved pickups once.

## Tested (local, real civic + real daemon, scratch stores)

Daemon:
1. A guest "wanderer" joins 1,1 at (5,5); alice logs in; the same tab's next move
   to (9,7) with her cookie + the guest credential: `promoted:true`, and region
   1,1 is **one row `u:alice` at (9,7)**, zero `g_` rows.
2. The next move needs no guest secret at all (garbage id/secret + cookie: ok).
3. Guards: the cookie with a **wrong** guest secret (no proof of the row) is `403
   bad_credential` and the guest row is untouched; no cookie + bad secret is still
   403; no cookie + right secret still moves as a guest.
4. **Second tab:** alice is bound in 1,1; a guest tab in 3,3 logs in: `u:alice`
   ends up in 3,3 only; 1,1 is empty (superseded, no duplicate).
5. **Name collision:** another guest already shows as "alice": the promoted row is
   "alice (2)".
6. A move naming a house region with a guest secret is refused (`not_welcome`),
   not promoted.
7. Regressions passing: `cookie_bind_test`, `cookie_ghost_test` (see note below),
   `visit_test`, `policy_test`, `persist_house_test`.

Note on `cookie_ghost_test` TEST 1: its printed expectation ("exactly one row,
still g_...") describes the **old** deferred behaviour, which is what this round
changes. It now shows one row, `u:ghosttest`, at the new position, promoted on
that very move; TEST 2 (rekey, no ghost) and TESTs 3-4 are unchanged.

Client (real browser, scratch daemon; the move answer stubbed, since a local page
can't carry a civic cookie): before the answer the client is a guest ("wanderer",
`isBound` false); after a `promoted` answer it is bound ("tester", `netBoundId`
`u:tester`, house key `instance:house:tester`, pickups load once), and a poll that
lists `u:tester` as if it were another player draws **nobody** (`othersDrawn`
empty). Console clean.

Not exercised: a real cookie-carrying browser logging in mid-walk (no local way;
the deploy check), and a real civic session expiring mid-session (out of scope,
unchanged).

## Deploy (when Brandon says `deploy`, after Grok's review)

**Daemon, then client** (Grok's order). No schema change, so no migration; an
online SQLite copy first is still cheap insurance. `-j1` build, no reconfigure
(`presence.cpp` is an existing source). Restart drops RAM presence. Live check
with a real logged-in player: open the game as a guest (private window or logged
out), log in on LiquidGov in another tab, walk: within a move the nameplate
should become your username and the friend-lot doors should start lighting, with
no second sprite. Order matters only mildly: a new client against the old daemon
just never sees `promoted`.

## Not here

`open` + orphans (slice 4, the only civic one). Cookie expiry mid-session and
logout mid-session (a bound row whose cookie stops resolving) are unchanged.

## Deployed (2026-09-19, Brandon: "deploy")

Grok's review of `8e65bac` was **not** on file when Brandon said `deploy`; I went
on his word and say so here. Daemon first, then client; civic and nginx never
touched (`liquidgov.service` not restarted).

1. **Drift check:** the daemon dry-run listed exactly `main.cpp`, `presence.cpp`,
   `presence.hpp`; the client dry-run exactly `index.html`; the droplet's client
   files were still the last deployed hashes.
2. **Online backup first** (SQLite backup API):
   `/var/lib/commons/backups/commons.sqlite.pre-rebind-20260919T085640Z`,
   `integrity_check` ok. Baseline: items 3, recipes 3, maps 14, map_instances 6,
   players 5, placed 3, inventory 4. Left on the box.
3. **Daemon** synced, `-j1` build with no reconfigure (only `main` and `presence`
   recompiled), restart. Live DB read-only afterward: integrity ok, every count
   identical to the baseline (no schema change). Through nginx: lots, `1,1`, a
   house `/placed/` GET all 200; public `/internal/friends` 404; journal has no
   failures; `NRestarts=0`.
4. **Guest path unchanged on production:** a real guest minted, joined `1,1`,
   moved (200), a bad-secret move still `403 bad_credential`, then left.
5. **Client** synced last: droplet, served and local `index.html` all
   `9a8ac18c...` (was `1e924625...`; `tiles.png`/`sprites.png` unchanged); the
   served page contains `netApplyJoinJson`. A guest walk on production: the
   refactored join handling works (nameplate, region, sheet loaded), console clean.

**Not seen on production:** the promotion itself. It needs someone who starts as a
guest and logs in mid-walk, and I don't sign in to production. The check for you:
open the game logged out (or in a private window), log in on LiquidGov in another
tab, then walk in the game tab. Within about one move the nameplate should turn
into your username and the friend-lot doors near Homestead Row should start
lighting, with no second sprite trailing you. If you get a second sprite or the
name doesn't change, say so.
