# Sibling deploy plan — applied 2026-09-17, live

**Applied.** Brandon said go after `GROK_DEPLOY_REVIEW.md`/
`CLAUDE_DEPLOY_PREFLIGHT.md`'s two must-fixes landed
(`c024c27`). Every step below was actually run against
`143.198.159.58` on 2026-09-17 and verified end-to-end, including a
real browser loading `https://liquidgov.us/commons/` for the first
time. See the "Applied 2026-09-17" section at the bottom for exactly
what was verified and the one new thing found only by actually
deploying (`/opt/liquidgov-us`, a second unrelated git checkout on the
droplet — not the running app, not touched, flagged for Brandon).
The sequence below is kept as the accurate record of what happened,
not a hypothetical anymore.

Reference docs this plan implements, unchanged: `sibling.md` (what
"sibling" means, memory budget, deploy split), `nginx.md` (exact
location shapes), `glue.md`/`cookie.md`/`cookie-bind.md` (why the
`/commons/` path shape exists at all — same-origin so `lg_session` gets
sent). This file doesn't re-decide anything those already decided; it's
the concrete sequence for applying them, plus one real bug found while
deriving that sequence (below).

## Files staged this round

- `commons-daemon/deploy/commons-daemon.service` — systemd unit
- `commons-daemon/deploy/nginx-commons.conf` — the two new `location`
  blocks to add to the existing civic server block
- `commons-daemon/deploy/sync-to-droplet.sh` — rsync script, modeled on
  `deploy/sync-to-droplet.sh` (civic's own), targeting a sibling path

## Correction found while staging: nginx.md's proxy_pass shape 404s

`nginx.md`'s example (`location /commons/presence` without a trailing
slash, `proxy_pass http://127.0.0.1:8091/;` with one) sends
`/commons/presence/join` upstream as `//join`, not `/presence/join` —
traced through nginx's own documented rewrite rule, not guessed. This
daemon's routes are exact-match on `/presence/join` etc (`http.cpp`), so
every presence call would have 404'd the first time this was actually
deployed and exercised. Never caught before now because no nginx config
for this project has ever been installed anywhere. Fixed in
`nginx-commons.conf` by matching trailing slashes on both sides
(`location /commons/presence/` + `proxy_pass .../presence/`) — nginx's
own documented safe pattern for this exact kind of prefix rewrite.
Full derivation is in that file's own header comment.

**Consequence:** once the client is switched to call the daemon
same-origin (a later step, see below), it must build presence URLs as
`/commons/presence/<rest>` with that trailing slash after `presence`
intact — `/commons/presencejoin` or a client that strips the slash
would not match this location at all and would fall through to the
static-file location instead (a confusing 404-via-static-alias, not an
obvious proxy failure).

## What this plan does NOT include

- ~~Wiring `commons/index.html` to actually use the same-origin path~~
  — **done and live**, `c024c27`: off loopback, `DAEMON` is
  `location.origin + "/commons"`. Confirmed in a real browser at
  `https://liquidgov.us/commons/`.
- **Cookie bind itself.** `cookie-bind.md` already covers why: this
  plan makes the *transport* same-origin (so `lg_session` would be
  sent), but nothing on civic resolves that cookie into an account yet.
  That's still a civic-side change, still not done here.
- **Photo/wall-slot/friends routes.** Not built, not proxied. Only
  `/commons/` (static) and `/commons/presence/` (the presence API that
  actually exists today) are covered.
- **`/health`, `/items`, `/recipes`, `/maps`.** The daemon has these
  routes but `commons/index.html` never calls any of them over the
  network today (catalog data is baked into the client's own JS). Not
  proxied by `nginx-commons.conf` for that reason. A future round that
  has the client fetch catalogs from the daemon instead of hardcoding
  them would need a matching `location` added at that time.

## Pre-flight checks (before running anything below)

1. **Does the droplet have outbound internet access?** `CMakeLists.txt`
   now `FetchContent`s the SQLite amalgamation from sqlite.org at
   configure time (`b4bf924`) — this is new since the daemon was last
   only test-built locally. Civic's own TLS renewal (certbot) implies
   *some* outbound access exists, but that's not the same guarantee.
   Check `curl -I https://sqlite.org` on the droplet before assuming the
   plain `cmake -S . -B build` step below will succeed unattended; if it
   can't reach sqlite.org, use `FETCHCONTENT_SOURCE_DIR_SQLITE3_AMALGAMATION`
   pointing at a copy uploaded alongside the sync, or vendor the tarball
   next to the tree — both already discussed in
   `fetchcontent-alternatives.md`, neither built, pick one if this check
   fails.
2. **Disk space and RAM headroom** for a `cmake --build` on a 512MB box
   — civic's own build already uses `-j1` for exactly this reason
   (memory, not just avoiding a GCC ICE per the existing deploy notes);
   the sync script's own output already says to build the same way.
3. **`liquidgov` user exists and can write `/opt/commons-daemon` and
   `/var/lib/commons`** once those directories are created (see below) —
   they don't exist yet on the droplet.

## Sequence (SSH steps -- all run, 2026-09-17, exactly as below)

```bash
# 1. From the local repo, sync the DAEMON's source (dereferences the
#    schema.sql symlink automatically -- see the script's own header):
commons-daemon/deploy/sync-to-droplet.sh

# 2. On the droplet, as a user that can sudo:
mkdir -p /opt/commons-daemon /opt/commons /var/lib/commons
chown -R liquidgov:liquidgov /opt/commons-daemon /opt/commons /var/lib/commons

# 3. Build (as liquidgov, or sudo -u liquidgov):
cmake -S /opt/commons-daemon -B /opt/commons-daemon/build
cmake --build /opt/commons-daemon/build -j1

# 4. Static files -- sync the CLIENT separately (GROK_DEPLOY_REVIEW.md
#    must-fix #1: `commons/index.html` is a sibling of `commons-daemon/`
#    in this repo, not inside it -- the earlier
#    `cp /opt/commons-daemon/commons/index.html ...` step here named a
#    path that does not exist). nginx's alias serves EVERYTHING under
#    /opt/commons/, so this must stay client-only, never the daemon tree:
commons-daemon/deploy/sync-client-to-droplet.sh
ssh -i "${LIQUIDGOV_DEPLOY_KEY:-$HOME/.ssh/liquidgov_deploy_key}" root@143.198.159.58 \
  chown -R liquidgov:liquidgov /opt/commons

# 5. systemd:
cp /opt/commons-daemon/deploy/commons-daemon.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now commons-daemon
systemctl status commons-daemon   # expect "active (running)"

# 6. Verify the daemon itself, still loopback-only, before touching nginx:
curl -s http://127.0.0.1:8765/health
# expect {"status":"ok"} -- if this fails, stop, do not touch nginx yet.

# 7. nginx -- backed up the live /etc/nginx/sites-available/liquidgov
#    first (liquidgov.bak-pre-commons-<timestamp>), pasted
#    nginx-commons.conf's three location{} blocks into the existing
#    server{} block, then tested on disk BEFORE reloading (nginx -t
#    does not affect the running process -- safe to fail):
nginx -t
systemctl reload nginx

# 8. Verify same-origin reachability (still guest-only -- no cookie bind,
#    no client change yet, this just proves the proxy path itself works):
curl -s https://liquidgov.us/commons/           # expect index.html
curl -s -X POST https://liquidgov.us/commons/presence/guest
# expect {"id":"g_...","secret":"..."} -- NOT a 404, NOT civic's own
# 404 page. A civic 404 here means the location didn't match at all
# (falls through to civic's `location /`) -- check the trailing slashes
# from the correction above before anything else.
```

## Rollback

`systemctl disable --now commons-daemon` stops and disables the game
process; civic is untouched and keeps serving `/` regardless (separate
unit, no `Requires=`/`After=` coupling — see the service file's own
comment on this). Removing the two pasted `location` blocks and
reloading nginx removes `/commons/` entirely with no civic-side edit to
undo, since civic's own `location /` was never touched.

## Reviewer-fail carried forward from every prior review doc

- `listen 0.0.0.0` on the daemon, or removing the systemd unit's
  `IPAddressAllow=127.0.0.0/8 ::1` / `IPAddressDeny=any` pair
- Touching civic's `location /`, its TLS block, or its `X-Forwarded-Proto`
  header
- A second `map $http_upgrade $connection_upgrade` block
- Adding CORS headers in nginx on top of the daemon's own (same-origin
  needs none; the daemon's existing CORS is for the still-cross-origin
  local-dev path only)
- rsyncing `commons.sqlite` (the sync script already excludes it, same
  class of bug as the civic `.lgp` incident)
- Opening `liquidgov_accounts.lgp` from the daemon, ever
- Building with `-j` (unqualified, i.e. all cores) on a 512MB box

## Applied 2026-09-17

Every step above ran for real against `143.198.159.58`. Result:

- `cmake -S`/`--build -j1` succeeded on the actual 512MB/1vCPU box,
  `FetchContent` reached sqlite.org fine (pre-flight check #1 passed,
  no tarball fallback needed) — first time this daemon has ever been
  built anywhere but a dev machine.
- `commons-daemon.service`: `active (running)`, ~1-2MB RSS (well under
  `MemoryMax=64M`), `IPAddressAllow`/`Deny` accepted by the kernel (no
  eBPF fallback needed).
- nginx reload: civic stayed `200` before, during, and after: never
  interrupted. `/commons/` served real `index.html`; `/commons`
  (no slash) 301'd correctly; `POST /commons/presence/guest` and the
  full join/move/leave/wrong-secret-403 lifecycle all verified over
  real HTTPS through the proxy.
- **A real browser** loaded `https://liquidgov.us/commons/` for the
  first time ever, minted a credential, joined presence, and the
  daemon's own `GET /presence/1,1` showed the exact same occupant --
  the full client-to-daemon path proven end-to-end, not just curl.
- **New, unexpected finding, not a blocker:** `/opt/liquidgov-us`
  exists on the droplet -- a full git clone of `Bbottin92/liquidgov-us`
  (`origin` remote, at `4d38c92` at the time of this deploy), separate
  from `/opt/liquidgov` (confirmed via the systemd unit's `ExecStart`
  and the live process's own `cwd` to be the actual running app).
  Not the running civic instance, not touched, not part of this plan --
  flagged here because it was genuinely unknown before this deploy and
  is worth Brandon knowing about (possibly a staging/migration
  experiment from another session). Investigated read-only only.

No rollback needed -- nothing failed. The rollback section above still
describes how, if this ever needs undoing.
