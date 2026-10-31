# Cookie resolver — built, local-tested, not yet deployed

**2026-09-17 update: must-fix ghost bug found and fixed before deploy.**
See "Must-fix: mid-session login ghost" below -- the original `effective_id()`
described in this doc was cookie-wins-unconditionally on every route, which
is what caused it. It's now split into three functions; `effective_id()`
survives only for `/placed/` POST, which never touches `PresenceStore` and
so was never at risk.

Brandon: "build" (in reply to the offer naming this exact increment --
"the first http_server.cpp edit this project has ever made" -- so this
doc treats that as BUILD + cookie resolver, `cookie-bind.md`'s standing
gate). Both blockers `cookie-bind.md` named are now closed:
reachability (same-origin `/commons/`, `deploy-plan.md`, already live)
and resolution (this round).

## What's actually new

**Civic (`http_server.cpp`), one new route, ~17 lines:**
`GET /internal/session-user`. Same-box only -- refuses (404, same shape
as `/logs`' non-operator refusal) any request that either arrives with
an `X-Forwarded-For` header (nginx's only path in always sets one, so
its presence means this came through the public proxy, not a direct
loopback call) or whose raw TCP peer isn't `127.0.0.1`/`::1`. Reuses
`account_from_req()` exactly as it already runs for every civic page --
no new session logic, no new store access. Returns
`{"username":"<name>"}` if the forwarded cookie resolves, `{}` if not.
Never touches anything else; every other route, every other file,
untouched.

**Daemon, new `civic.hpp`/`civic.cpp`:** `resolve_username(headers_raw)`.
Skips the network call entirely unless the raw `Cookie` header actually
contains `lg_session=` (civic sets `lg_theme` on every visitor
regardless of login -- gating on "any cookie at all" would fire this on
every guest's every request). When it does, opens a loopback socket to
civic's port, forwards the cookie, 1s timeout, parses the JSON reply
with the same tiny `field_str` helper request bodies already use.
Every failure path (no cookie, civic down, timeout, unresolved) returns
`nullopt` -- never blocks, never throws, always falls back to guest.

**Daemon, `main.cpp`:** one new `effective_id()` helper, used at all five
places that used to do `guests.verify(id, secret)` alone
(`/presence/join`, `/move`, `/rekey`, `/leave`, `/placed/` POST). Cookie
resolves -> effective id is server-derived `"u:" + username`, client's
own `id`/`secret`/`display_name` fields are not consulted at all for
identity (display_name becomes the real username). Cookie doesn't
resolve -> unchanged existing guest-secret path. The two never mix, per
`cookie-bind.md`.

## What's still NOT here (deliberately)

- **Deploy.** Local-only so far (WSL, scratch civic store, scratch
  daemon DB). Nginx, systemd, the droplet -- none of it touched. Needs
  its own explicit go, same as every prior deploy round this project.
- **Mid-session promotion.** A guest who's already walking around and
  then logs in, without leaving the game, does not get relabeled in
  place -- `cookie-bind.md` flagged this as needing a decision
  (`rebind()` primitive vs. leave+join) that was never made, and still
  isn't made here. What *is* built: binding happens at `join`/`rekey`
  time, checked fresh on every request after that (so a bound player
  who walks between rooms stays bound with zero extra code -- rekey
  re-derives the same id from the same cookie). A player who is bound
  *before* they ever join sees this working end to end. A player who
  logs in mid-visit stays a guest until their next join/rekey (i.e.
  their next region change) picks the cookie up -- not a crash, not a
  security gap, just not the seamless "relabeled in place" promotion
  `presence.md` describes. Worth its own round if it matters in
  practice.
- **Restart/crash snapshot for bound players.** `presence.md`'s
  snapshot-onto-`players`-row-on-leave logic is real and already
  written, but nothing here wires a leave/snapshot hook to it yet --
  today's `leave` still only touches `PresenceStore`, the RAM-only
  side. A bound player disconnecting does not yet persist last
  position. Separate, not attempted this round.

## Tested (local, both processes on loopback, scratch data only)

Fresh civic account signed up in a scratch `LIQUIDGOV_STORE`; its real
`lg_session` cookie forwarded to a fresh daemon instance with garbage
client-submitted `id`/`secret` on every call, to prove the cookie -- not
the client's claim -- decides identity:

- `POST /presence/join` with cookie + garbage credentials -> bound as
  `u:boundtest`, display_name forced to `"boundtest"` (client's attempted
  display_name silently ignored, as designed).
- `POST /presence/move` with the same garbage credentials, cookie
  present -> succeeds, position updates under `u:boundtest`.
- `POST /presence/leave` with cookie -> occupant row removed.
- Regression check, no cookie jar at all: guest register + join still
  works unchanged; a guest move with a *wrong* secret still 403s exactly
  as before cookie bind existed.

No deploy step run. Civic and the daemon were both scratch-store,
scratch-DB local processes, torn down after the test; nothing touched
`liquidgov_accounts.lgp`, `commons.sqlite`, or any real data.

## Must-fix: mid-session login ghost (found by Grok review, fixed)

`GROK_COOKIE_RESOLVER_REVIEW.md` / `CLAUDE_COOKIE_GHOST.md`: the
original `effective_id()` was cookie-wins-unconditionally on every
route, including `move`/`leave`/`rekey`. That breaks exactly the case
this doc's own "still NOT here" section described as safe -- a guest
who joins, then logs in without leaving:

1. Occupant row exists as `g_...`. Cookie now resolves.
2. Next `move`: old code looked up `u:<name>` in the region, found
   nothing (the row is still `g_...`), and returned `ok:false` --
   the client's own walk would desync.
3. Next `rekey`: old code inserted `u:<name>` into the new region but
   never erased `g_...` from the old one -- a ghost sprite, visible to
   everyone else in that room, that only ever cleared via the 30s idle
   reap.

Fixed, daemon only, no civic change:

- **`presence.hpp`/`.cpp`:** two new public methods. `forget(id)` --
  a global erase by id, for retiring a guest row that's a *different*
  id than the one about to be inserted (join_locked's own internal
  erase only ever erases the id it's currently inserting, never
  a different one). `contains(region, id)` -- whether an id currently
  has a row in a given region, used to tell if a bound id has actually
  been promoted into *this* room yet.
- **`main.cpp`:** `effective_id()` split into three. `bind_or_promote()`
  (join/rekey -- the only two places identity is established): cookie
  still wins, but if the client's original guest secret still verifies,
  its row is `forget()`-ten first, so the bound row replaces it instead
  of leaving it behind. `effective_id_in_region()` (move/leave): a
  resolved cookie only takes over once `contains(region, "u:<name>")`
  is true -- otherwise falls back to the still-live, still-verified
  guest row, so an ordinary move never 403s just because promotion
  hasn't happened yet. Plain `effective_id()` survives unchanged for
  `/placed/` POST, which never creates or looks up a `PresenceStore`
  row and so was never exposed to this bug.

**Verified locally** (scratch stores, fresh accounts, same four
scenarios `CLAUDE_COOKIE_GHOST.md` asked for):

- Guest joins region A, logs in on civic mid-session, moves again in
  the *same* region with the real guest id/secret still attached ->
  `ok:true`, row stays `g_...`, position updates. Not rejected.
- Same session then rekeys to region B -> region A ends up completely
  empty (no ghost), region B has exactly one row, `u:<name>`. A further
  move in region B with garbage id/secret succeeds directly under
  `u:<name>` (promotion has happened, `contains()` is now true).
- Regression: joining already-bound from the very first call (no prior
  guest row at all) still works exactly as before.
- Regression: a guest with no cookie and a bad secret still 403s.

Still not built: in-place `rebind()` (the row is still replaced, not
literally relabeled -- `cookie-bind.md` never decided between the two
and this fix doesn't either, it just stops the replace from leaking a
duplicate). Still no restart/crash snapshot for bound players.

## Deployed 2026-09-17

Brandon: "deploy." Civic binary rebuilt (`/opt/liquidgov`, rsync +
`cmake --build -j1` + `systemctl restart liquidgov`) and daemon rebuilt
(`/opt/commons-daemon`, same pattern, reconfigured first since
`civic.cpp` is a new source file) -- no nginx edit, Cookie was already
forwarded same-origin from the sibling-deploy round. Drift-checked both
source trees against the droplet before syncing (sha256 + diff): zero
unexpected changes, only this project's own commits.

Verified against production:

- `curl https://liquidgov.us/internal/session-user` -> `404`, plain
  `not found` body -- the public path is blocked exactly like `/logs`'s
  own refusal, not the JSON a same-box caller gets.
- Signed up a real throwaway account (`claudeverify17`) in an actual
  browser, typed a decoy guest name ("ShouldBeIgnored") into the
  Square's own name-entry modal, walked in. `GET /commons/presence/1,1`
  and the on-screen self-nameplate both show `u:claudeverify17` /
  `claudeverify17` -- the real account name, the decoy silently
  discarded, exactly as designed.
- Both `systemctl status` clean, `journalctl` for both services shows
  no errors, only the expected `GET /internal/session-user 200` lines
  from the daemon's own loopback calls while that test session walked.

Live. `liquidgov.us/commons/` cookie-binds a real account now.
