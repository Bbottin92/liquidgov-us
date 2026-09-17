# Cookie resolver — built, local-tested, not yet deployed

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
