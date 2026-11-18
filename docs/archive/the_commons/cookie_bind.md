# Cookie bind — presence side of the contract

**2026-09-19: mid-session promotion built** (in-place `rebind()`, see
`rebind_built.md`). **2026-09-17: built.** Both blockers named below are closed --
`cookie_resolver_built.md` has the concrete change and what's still
open (mid-session promotion/rebind was NOT decided or built; see that
file's own "still NOT here" section). This file stays as the design
record; read it for the *why*, `cookie_resolver_built.md` for the
*what actually shipped*.

Originally: not a BUILD. Design/document only, by explicit request — no
code that round. Complements `cookie.md` (the raw parse algorithm) and
`glue.md` /
`grok_glue_review.md` (the civic-side facts and the already-parked
"daemon forwards `Cookie` to civic localhost" plan). This file fills the
one gap those don't cover: what binding actually does to `PresenceStore`
and this session's guest-auth work (`GuestRegistry`, id/secret,
`display_name` disambiguation), once it's ever wired.

## Why nothing here gets built yet

Two independent blockers, neither optional:

1. **Reachability.** `lg_session` is host-only (no `Domain=`, `cookie.md`)
   and the browser only sends it to whatever origin civic is actually
   served from. The commons daemon is a different origin today (a
   different port on the same loopback host in dev; a different
   host+port entirely once anything is deployed). No client-side change
   fixes this — same-origin `/commons/` behind nginx is the only fix
   (`nginx.md`), and that's the deploy round.
2. **Resolution.** The token → account chain (`account_from_req()`) is
   real, existing civic code, but it's not callable from outside
   `http_server.cpp` as-is. `grok_glue_review.md` already parks the plan
   as "daemon forwards `Cookie` to civic localhost... civic already has
   the chain" -- but *something* on civic's side still has to receive
   that forwarded header and answer with an account, and today nothing
   does. That's a civic change, however small, so it waits on Brandon.

Everything below assumes both are eventually true and describes what the
daemon does *once they are* -- it is not a plan to do either of them.

## The two auth paths never mix

This session built `GuestRegistry` (`guest.hpp`/`.cpp`): a client proves
it owns an occupant row with a server-issued `(id, secret)` pair, id
`g_...`. That path stays exactly as-is for guests and does not change.

A bound request is authenticated a completely different way: by the
forwarded `Cookie` header resolving to a real account, checked **on the
daemon, per request**, the same way `account_from_req()` does it in
civic today. Concretely:

- The daemon never trusts a client-supplied `id` as proof of a bound
  identity. It derives the effective id itself, server-side, as
  `"u:" + lg_username` (already the exact scheme `presence.hpp`'s
  `Occupant::id` comment names: `"ephemeral guest id, or \"u:<lg_username>\"
  once bound (post-glue)"` -- written into this project before this doc
  existed, so this isn't a new decision, just the first place it's spelled
  out for presence specifically).
- If the forwarded `Cookie` resolves to an account, that request is
  authenticated as that account, full stop -- no `secret` field is
  consulted or required for it. If it doesn't resolve (missing, expired,
  unknown token), the request falls back to being a guest request and
  the normal `GuestRegistry` check applies.
- A client cannot claim to be bound by sending `id: "u:someone"` without
  the matching cookie -- there is no code path that accepts that. This
  is the same shape as the row-hijack fix already shipped
  (`grok_daemon_review.md` should-fix #3): identity comes from something
  the client can't forge, never from a field it fills in.
- `display_name` for a bound occupant is presumably the real username,
  not free text -- but the existing region-local disambiguation
  (`disambiguate_name_locked`, this session's should-fix) still composes
  correctly with no changes: a collision can only ever be a guest's
  typed name coincidentally matching a real username, and the existing
  by-id exclusion already handles that the same as any other collision.

## Promotion, not a disconnect/rejoin

`presence.md`, already locked: "Once cookie glue exists ... the *same*
presence entry is relabeled with `lg_username` in place -- a promotion,
not a disconnect/rejoin. Position, facing, and any in-flight state carry
over untouched."

Today's `PresenceStore` has no primitive for this -- `join_locked` either
replaces a same-id/same-region row or inserts a new one; there's no
"rename this existing row's id and display_name without touching its
position, and without it ever costing a cap slot or a rejoin blip."
Whoever builds this needs one of:

- A new `PresenceStore::rebind(region, old_id, new_id, new_display_name)`
  that finds the occupant by `old_id` in `region` and updates `id` +
  `display_name` in place, touching nothing else (`x`/`y`/`facing`/
  `last_update_ms` untouched) -- the literal reading of "in place."
- Or, functionally equivalent from every other client's point of view:
  a `leave` + `join` pair at the *same* x/y/facing, timed so no GET poll
  from another client ever observes the gap. Cheaper to build on top of
  what already exists, but "no disconnect" becomes "no *observable*
  disconnect," a weaker guarantee than the literal one above.

Not deciding between these here -- flagging that a decision is needed,
since neither is built today.

## Restart/crash recovery becomes live, not dormant

`presence.md`'s snapshot section is already fully specified and today
entirely dormant, because V1 has no bound players to snapshot: "On clean
leave... if bound, last position is written onto that row
(`last_grid_*`/`last_instance_id` + `last_x`/`last_y`/`last_facing` in
`schema.sql`)" and "periodic safety snapshot, ~every 30s, of bound
players only." Binding is the thing that makes this path start actually
running. No new design needed here -- just noting that turning on
binding also turns on a piece of `presence.md` that's been sitting
unused since it was written, and to re-read it (not this file) as the
source of truth when that day comes.

## Fail if "fixed" by

- Accepting `id: "u:<anything>"` from a client without an independently
  resolved cookie proving it
- A client-supplied `display_name` being treated as a resolved username
- Checking both a guest `secret` *and* a cookie for the same request
  (pick one deterministically -- cookie present-and-valid wins,
  otherwise it's a guest request)
- A new presence/players table, or writing presence itself to SQLite --
  `schema.sql`'s "RAM only, never a table" rule doesn't change; only the
  *last position snapshot onto an existing `players` row* does, and only
  for bound players, exactly as `presence.md` already specifies
- Opening `liquidgov_accounts.lgp` from the daemon, ever, to shortcut the
  civic-forwarding plan
- Inventing a new civic endpoint here as a fait accompli instead of
  flagging it as the still-open civic-side dependency it is

No deploy. No nginx. Civic untouched. This file changes nothing about
what V1 actually does today -- guests still fake the whole thing, per
`presence.md` and `glue.md`.
