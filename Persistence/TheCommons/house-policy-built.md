# House friends/locked — built, local-tested, not deployed

Brandon: "BUILD house friends/locked A" (`CLAUDE_HOUSE_POLICY_QUESTION.md`,
Grok's `GROK_HOUSE_POLICY_ANSWER.md`: option **A**, `open` waits on civic
blocks). Daemon + client + one schema column. Civic, nginx, LGP1 untouched.
No deploy.

## What ships

An owner can lock their own house from inside it.

- **`players.visit_policy TEXT NOT NULL DEFAULT 'friends'`**
  (`Persistence/TheCommons/schema.sql`, which `commons-daemon/schema.sql`
  symlinks to). Only `friends` and `locked` exist; any other stored value reads
  as `friends`. `Database::ensure_visit_policy()` adds the column to an
  existing DB at startup (idempotent, same shape as
  `ensure_inventory_recency`); every existing house takes the default, so
  **nothing changes for anyone until they lock**.
- **`POST /presence/house-policy` `{"policy":"friends"|"locked"}`.** Identity is
  the forwarded session cookie only: no username in the body is read, so nobody
  can set another player's door; a guest is `403 not_bound`; any other policy
  (including `open`) is `400 bad_policy`; a bound player with no house row yet
  is `409 no_house` (it never creates one). Under `/presence/`, so the existing
  nginx location covers it -- **no nginx change**.
- **`visit_allowed()` now asks the owner's policy first.** `locked` refuses
  everyone but the owner, friends included, on join, rekey **and move**. The
  move check is what walks out anyone already inside: it is read live from
  SQLite, so a visitor gets `not_welcome` on their **next move** (no 15s
  friends-cache wait -- that TTL only applies to unfriend/block).
- **`/presence/lots`** skips a locked owner, so their door goes dark for
  friends on the next poll.
- **Join/rekey echo `house_policy`** only when the owner lands in their own
  house, so the latch can label itself. Visitors, guests and non-house
  regions get no such field.
- **Client:** the owner's house is now its own space (`ownHouseSpace`) --
  the same room plus one latch, a sign tile on the left wall at (0,13), away
  from the entry so the Enter-to-leave fallback can't land on it. Prompt reads
  **Lock**/**Unlock**; Enter sends the desired policy and the label and toast
  follow the **daemon's answer**, not the click ("Door locked. Only you can come
  in." / "Door unlocked. Friends can visit." / "Couldn't change the door.").
  The demo house and a friend's house (mode `visit`) keep the plain space: no
  latch tile, no latch interactable. A friend refused at a locked door gets the
  existing "They aren't taking visitors right now."

`friends` still needs the owner online to *enter*; guests never enter a house
that isn't theirs. No `open`, no client-side block guess, no second friend graph.

## Tested (local, real civic + real daemon, scratch store; three accounts)

Daemon (alice owner, bob mutual friend, carol not a friend, guest):

1. Default: alice's join echoes `house_policy:"friends"`; bob welcome; bob's
   lots list alice.
2. Setter guards: guest 403 `not_bound`; `open` 400; empty body 400; **bob
   posting `{"policy":"locked","username":"alice"}` gets 409 for *his own* nonexistent
   house and alice stays friends (bob still moves)**; carol 409.
3. Alice locks: bob's next move is `not_welcome` immediately, bob is gone from
   the room and alice remains; bob's re-entry 403; bob's lots are empty; carol
   and a guest still 403; **alice can still join and move in her own locked
   house** and her join echoes `locked`.
4. **Survives a daemon restart:** alice's rejoin echoes `locked`; bob still
   refused; lots still empty.
5. Alice unlocks: bob welcome again; lots list alice; alice's rekey into her
   house echoes `friends`.
6. **Migration of a real old-shape DB** (built from the previous
   `schema.sql`, no `visit_policy`): first start adds the column, every
   existing row reads `friends`, `integrity_check` ok, second start is a no-op.
   (My first attempt used `DROP COLUMN`, which SQLite rejected on this table --
   that run did not test the migration; this one does.)
7. Regressions unchanged and passing: `visit_test` (friends visit, block
   eviction after the TTL, owner resume) and `persist_house_test`.

Client (real browser, scratch daemon; the policy request stubbed, since a local
page can't carry a civic cookie): the latch tile and `latch` interactable exist
in `ownHouseSpace` only (`demoHouseSpace` has a plain wall at that tile and only
`wallSlot`); at the entry tile nothing is in range, so Enter still leaves; next
to the latch the prompt is **Lock**, a `locked` answer flips it to **Unlock**
with the toast, `friends` flips it back, a failed answer leaves the label alone
with "Couldn't change the door."; the join echo `house_policy:"locked"` sets the
label. A clean re-run had **no console errors**. (A first run logged a flood of
403s: my fake join reply had set the house key while I was a guest, so the daemon
correctly refused the guest into that house and the client reauthed in a loop.
That was the harness, not the client.)

Not exercised: a real cookie-carrying browser locking a real house end to end
(no local way; that is the deploy check), and a visitor already inside seeing the
client-side "You're no longer welcome there." toast against a real lock.

## Deploy (when Brandon says `deploy`, after Grok's review)

**Daemon first, then client** (the client sends a route the old daemon doesn't
have, and reads a field it doesn't send). This is a **schema change**: take an
online SQLite backup first (backup API, as for inventory and the snapshot), then
sync the daemon, `-j1` build, restart. The startup migration runs on the real
production DB; verify the column is there and every existing row is `friends`,
counts unchanged, before syncing the client. Live check with a real bound
player: lock, then confirm a friend's door goes dark and a visit is refused;
unlock and confirm it lights again.

## Not here

`open` (needs a civic block probe, its own named BUILD), guests entering any
house, `rebind()`, `lg_session` cache, orphan reaping, the `sqlite3_step`
checks in `house.cpp`: all still parked.

## Deployed (2026-09-19, Brandon: "deploy")

Grok's review of `5f465a0` was **not** on file when Brandon said `deploy`; I
went on his word and say so here. Daemon first, then client; civic and nginx
never touched (`liquidgov.service` not restarted).

1. **Drift check:** the daemon dry-run listed exactly the six files this round
   edited (`schema.sql`, `db.cpp/.hpp`, `house.cpp/.hpp`, `main.cpp`); the
   droplet's `index.html` was still the last deployed hash (`1f4eab59...`).
2. **Online backup first** (SQLite backup API):
   `/var/lib/commons/backups/commons.sqlite.pre-house-policy-20260919T070744Z`,
   `integrity_check` ok, old-shape `players`. Baseline counts: items 3, recipes 3,
   maps 14, map_instances 6, players 5, placed 3, inventory 4. Left on the box.
3. **Daemon** synced, built on the droplet (`-j1`), restarted. **The migration
   ran on the real production DB** (read-only check afterward): `visit_policy`
   present, all 5 existing players `friends`, `integrity_check` ok, every count
   identical to the baseline.
4. Through nginx: `/presence/lots`, `/presence/1,1`, a house `/placed/` GET all
   200; public `/internal/friends` 404; the new `POST /presence/house-policy`
   with no cookie is `403 not_bound` (also for `open`).
5. **Client** synced last: droplet and served `/commons/` both md5
   `e99898fa...` = local (was `1f4eab59...`); served page carries
   `house-policy`; both services active, journal clean.

Not yet seen on production: a real bound owner locking a real house (no way to
log in from here; I don't create or sign in to accounts on production). The
check: lock from the latch, a friend's lit door goes dark and a visit is
refused; a friend already inside is walked out on their next move; unlock and
the door lights again.


> Update 2026-09-19: the `open` policy this doc deferred is now built (4a) -- see `open-built.md`.
