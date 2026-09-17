# Sibling deploy plan — staged, not applied

**Nothing in this file has been run against the droplet.** Per explicit
scope for this round: stage the systemd unit, nginx config, and sync
script as reviewable files; SSH and apply only after Brandon reviews and
says go. This doc is the staged plan itself.

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

- **Wiring `commons/index.html` to actually use the same-origin path.**
  The client today talks to `DAEMON` (`http://127.0.0.1:8765` by
  default, or a validated `?daemon=` override, both loopback-only —
  `4e9bfbd`). Switching it to default to the relative
  `/commons/presence/` path when served same-origin, while keeping the
  loopback default for local dev, is a separate, small client change
  not made this round — flagging it as the next piece once the droplet
  side below is actually applied and verified reachable.
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

## Sequence (SSH steps -- not run yet)

```bash
# 1. From the local repo, sync source (dereferences the schema.sql
#    symlink automatically -- see the script's own header comment):
commons-daemon/deploy/sync-to-droplet.sh

# 2. On the droplet, as a user that can sudo:
mkdir -p /opt/commons-daemon /opt/commons /var/lib/commons
chown -R liquidgov:liquidgov /opt/commons-daemon /opt/commons /var/lib/commons

# 3. Build (as liquidgov, or sudo -u liquidgov):
cmake -S /opt/commons-daemon -B /opt/commons-daemon/build
cmake --build /opt/commons-daemon/build -j1

# 4. Static files -- copy just the client, not the whole repo, into the
#    nginx alias target (nginx.md: alias serves EVERYTHING under it):
cp /opt/commons-daemon/commons/index.html /opt/commons/index.html
chown liquidgov:liquidgov /opt/commons/index.html

# 5. systemd:
cp /opt/commons-daemon/deploy/commons-daemon.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now commons-daemon
systemctl status commons-daemon   # expect "active (running)"

# 6. Verify the daemon itself, still loopback-only, before touching nginx:
curl -s http://127.0.0.1:8765/health
# expect {"status":"ok"} -- if this fails, stop, do not touch nginx yet.

# 7. nginx -- paste nginx-commons.conf's two location{} blocks into the
#    existing liquidgov.us server{} block (see that file's own header
#    for exactly where/how), then:
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

No deploy has happened. This is the plan for review.
