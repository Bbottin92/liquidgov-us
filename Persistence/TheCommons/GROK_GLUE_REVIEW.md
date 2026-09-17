# Grok review of glue (`glue.md` 245d181)

Accept. Civic `http_server.cpp` was not edited. `commons/` was not edited.

## Facts that are now locked

- Cookie **`lg_session`**: HttpOnly, Path=/, SameSite=Lax, Max-Age=30d,
  **`; Secure` when `g_https`** (from `X-Forwarded-Proto`). No `Domain=`.
- Path `/commons/` needs zero civic cookie change. Subdomain needs one.
- Token → `account_from_req()` → username. Never treat sid as a name.
- Wall-slot source of truth is `GET /canvas-frame/<username>/<i>`
  (public PNG). There is no single bitmap. Missing: a latest/count
  address. Do not 404-probe.

The first-pass “no Secure” miss was real; the correction is right.
Checksum vs droplet is Claude’s job (Grok does not SSH). Point-in-time.

## Parked for sibling BUILD (not this week)

1. **Daemon forwards `Cookie` to civic localhost.** It does not open
   `liquidgov_accounts.lgp` or reimplement `cookie_of`. Civic already
   has the chain. 401/empty = guest.
2. **nginx must keep `X-Forwarded-Proto`.** Drop it and `Secure` falls
   off (`glue.md`). Already implied by `nginx.md`.
3. **Wall-slot fetch forwards the viewer cookie** so `is_blocked`
   applies. Anonymous GET skips that check. `/latest` alias is civic,
   one small route, not a game parser.

## Fail if “fixed” by

- `document.cookie` in `commons/index.html`
- A second session cookie
- `Domain=.liquidgov.us` “just in case”
- Game probing `/canvas-frame/u/0`, `/1`, … until 404
- Opening LGP1 from the game process

V1 still fakes cookie/friends/wall-slot. Sibling waits on Brandon.
