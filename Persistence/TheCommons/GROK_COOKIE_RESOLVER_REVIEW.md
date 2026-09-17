# Grok review of c263e8f — not deploy yet

Civic `GET /internal/session-user` (loopback, XFF → 404) + daemon
forward is the right shape. Cookie wins identity. Guest path unchanged
without `lg_session=`. Scratch test (join already bound) is the easy
case. **Do not SSH.**

Same-origin `/commons/` already sends `Cookie` (fetch default
`same-origin`). Client `credentials` does not need a change for prod.

## Must-fix (real, local)

Cookie-wins on **every** verb, but `join_locked` only `erase_id`s
`occ.id`. Mid-session login (the case they deferred as "until rekey"):

1. Occupant is `g_…`. Cookie appears.
2. Next **move** looks up `u:name` → miss → `ok:false`. Local walk
   desyncs; guest row idles out in 30s.
3. Next **rekey** inserts `u:name` and does **not** erase `g_…` →
   **ghost** until reap. One person, two sprites.

Claimed "stays a guest until rekey" is **not** what the code does:
`effective_id` never falls back to `g_` once a cookie resolves.

Fix, daemon only:

- **join / rekey:** if `eff_id != claimed_id`, `erase` claimed_id
  globally *then* insert `u:name` (no ghost).
- **move / leave:** if cookie resolved but `u:name` is not in that
  region, fall back to verified `claimed_id` (keep moving as guest
  until the next join/rekey promotes). Do not `ok:false` a live walker.

That matches the written "promote at room change" without breaking
walk or leaking a sprite. Still not in-place `rebind()`. Still no
snapshot-on-leave.

## Should-fix (not this patch)

- Cache `lg_session` → username ~5–30s. Every 2 Hz move is a blocking
  loopback GET to civic (1s timeout stalls the **whole** accept loop).
- `find("Cookie:")` vs `X-Cookie:` (same class as Origin).

## Fail if “fixed” by

- Deploying civic/daemon first
- Opening LGP1 from the daemon
- Trusting client `id: "u:…"` without the cookie
- `Allow-Origin: *` coming back

Paste `CLAUDE_COOKIE_GHOST.md`. Then Brandon says **deploy** — civic
binary + daemon rebuild + no nginx change (Cookie already forwarded).
