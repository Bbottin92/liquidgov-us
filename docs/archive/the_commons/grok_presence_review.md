# Grok review of presence wiring (70bc5b8)

Accept the **scope**. Client had no daemon. Full join/move/rekey/leave
+ server-issued (id, secret) is the right slice. Civic frozen. Loopback.
Two-tab live test is real. sendBeacon → `fetch(...,{keepalive:true})` is
the correct lesson.

Do **not** deploy. Do **not** nginx.

## Accept

- `GuestRegistry`: public `g_` id, secret once, constant-time compare
- Mutations 403 without secret; GET `/presence/<region>` stays public
- ~2 Hz move+poll, rekey on AABB, leave on interior, remint-not-required
  on first outdoor tick
- CORS `*` + OPTIONS 204 with `Content-Type` — needed for :8090 → :8765;
  guest secret is JSON, not a cookie, so `*` is legal **until glue**
- Unreachable daemon → local passerby, no throw
- Passerby hides when a **real** second occupant is in-region

## Must-fix (real, local)

**Guest credential reaps on the same 30s clock as presence, but interiors
do not heartbeat.**

`netLeave()` on house/lab. `GuestRegistry::verify` is then not called.
After 30s `reap_locked` drops the secret. Walk out → join with the JS
secret → **403**, player never reappears. Presence row is already gone;
that is correct. The **credential** should have survived the interior.

Fix (either, not both):

1. **Daemon:** guest idle ≥ 30 **minutes** (or no reap except process
   death). Presence 30s stays. A crashed tab still frees the plaza slot.
2. **Client:** on 403 `bad_credential`, `POST /presence/guest` again and
   join. Also covers a daemon restart mid-session.

(1) is less chatty. (2) is more honest after a daemon restart. Doing
**both** is fine.

Do not “fix” by networking interiors this round.

## Should-fix (not this patch unless cheap)

- Poll **camera-overlapping** regions (`presence.md`), not only the
  current AABB. Stated V1 simplification; neighbors vanish at the seam.
- `display_name` still client-chosen. Row hijack is closed; name spoof
  is not. Fine until cookie bind.
- `NET` host is loopback `:8765`. Grok preview and `liquidgov.us` will
  not see WSL. Relative `/commons/` proxy is the nginx round.
- CORS `*` must die before `lg_session` is forwarded (credentialed
  fetches cannot use `*`).
- Mint: no retry if `g_`+hex collides (2^64; ignore).

## Fail if “fixed” by

- systemd / SSH / `0.0.0.0` / civic / LGP1 / cookie forward
- sendBeacon again
- Networking interiors / instance ids this round

Paste `claude_presence_patch.md`.
