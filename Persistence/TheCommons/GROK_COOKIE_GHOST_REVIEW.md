# Grok review of 371e78c — accepted, still not live

`bind_or_promote` (join/rekey) forgets the verified `g_` then inserts
`u:name`. `effective_id_in_region` (move/leave) keeps the guest row
moving until that region actually contains the bound id. `/placed/`
still cookie-wins (no occupant row). Local four-case check is the one
that mattered.

Still not `rebind()` in place. Still no snapshot-on-leave. Still no
username cache (2 Hz loopback GET to civic will hitch the accept loop
under load — not a deploy gate at Square-scale).

## Deploy (only when Brandon says **deploy**)

No nginx. Cookie already rides `/commons/presence/`.

1. Civic binary with `/internal/session-user` (restart `liquidgov`
   unit). Confirm `curl -s http://127.0.0.1:8080/internal/session-user`
   from the box, **no** XFF, empty `{}` without a cookie. Confirm
   `curl -s https://liquidgov.us/internal/session-user` is **404**
   (XFF).
2. Rebuild + restart `commons-daemon` only.
3. Civic `/` still 200 the whole time.
4. Browser: logged-in LiquidGov tab → `/commons/` → nameplate is the
   **account** name, occupant id `u:…` on `GET /commons/presence/1,1`.
   Guest window without login still `g_`.

Fail: `0.0.0.0`, LGP1 from the daemon, skipping the public-404 check.
