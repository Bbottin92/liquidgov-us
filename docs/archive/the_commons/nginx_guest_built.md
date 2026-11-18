# nginx `/guest` rate limit — built, CI-tested, DEPLOYED 2026-09-19 (~15:46Z)

Brandon: "BUILD nginx /guest" (`grok_next.md` #3, `grok_hardening.md` §2, `grok_reqjson_live.md`: "nginx also
needs its own **deploy**"). **Built without touching the droplet; deployed afterwards (see "Deployed" at the bottom).** Files:
`commons-daemon/deploy/nginx-commons-http.conf` (new, http level), `nginx-commons.conf` (one new location),
`tests/test_nginx_guest.sh`, two workflow edits.

## What it is

```
limit_req_zone $binary_remote_addr zone=commons_guest:1m rate=20r/m;          # http level (conf.d)
location = /commons/presence/guest {                                          # server level, EXACT match
    limit_req zone=commons_guest burst=30 nodelay;  limit_req_status 429;  ...proxy_pass .../presence/guest;
}
```

An exact-match location beats the `/commons/presence/` prefix, so ONLY the mint route is limited; join, move,
leave, lots, regions and `/placed/` keep the prefix location and are never limited. `/internal/*` is never named:
no `limit_req` on it or on any location that can match it (`civic_ratelimit.md`; it would 429 the daemon and look
like "civic down").

## The numbers are a decision, not a spec: no doc gave any. Chosen, with reasons (veto before deploy)

- **20 per minute per address, burst 30, nodelay.** Legitimate traffic is one mint per page load plus ONE per
  daemon restart (every client's first 403 re-mints). A shared address (a school, a household, an office) with 30
  players all re-minting after a restart must fit the burst and never be slowed. A script hammering the route gets
  429s after 30. The daemon already bounds memory (registry cap 256, never-used guests expire in 2 min and are
  evicted first), so this limit is about noise and CPU, not survival; it is deliberately loose.
- **Key `$binary_remote_addr`**: nginx is the edge here. If a CDN/proxy is ever put in front, every visitor shares
  one bucket and the key must change (the zone file says so). Confirm at deploy: no `cf-`/proxy headers in front.
- **429, not nginx's default 503** (503 reads as "the daemon is down").
- **No custom error body, on purpose.** The client's `netMint()` never checks the status: a JSON body would be
  read as credentials (`netReady=true`, `id` undefined) and loop on 403s. nginx's default HTML 429 makes
  `r.json()` throw, and the client stays local-only for the session (its existing failure path). The cost of a
  false positive is therefore "that visitor plays local-only until reload", which is why the burst is generous.
  **Follow-up, not built (a client change):** `netMint()` should check `r.ok` and retry after a delay.
- `limit_req_dry_run on;` is left commented out (nginx >= 1.17.1). It would log what WOULD be limited without
  limiting, if a first rollout should be cautious: `grep 'dry run' /var/log/nginx/error.log`.

## Tests (in CI; the workflow now installs nginx before the suite)

`tests/test_nginx_guest.sh`. **Static half (12 assertions, runs anywhere, mutation-checked with 8 breakages):**
exactly one `limit_req_zone` and one `limit_req`, inside exactly the guest location, keyed on the peer address,
429, no `error_page`, dry run off, burst >= 20, nothing active mentions `internal`. **Behavioural half (CI only):**
a REAL nginx (as the runner user, its own prefix/port, never the system config) in front of the real daemon:
`nginx -t` with the zone file; a mint through nginx returns a real credential (the exact-match `proxy_pass`
rewrite works); 61 rapid mints -> 30-34 accepted then 429s with nothing else (no 502/503) and the first 429 only
after the burst; a limited mint answers 429 with a non-JSON body and is logged; another address (127.0.0.2) is not
limited while 127.0.0.1 is; 30 lots polls, 30 region polls, join/move/leave with an earlier credential are all fine
while mint is limited; `/commons/presence/guests` is not the limited route; after ~3.5s a mint is accepted again
and the limit re-engages. In CI a missing nginx is a failure, not a skip. The workflow's `nginx -t` step now also
includes the zone file.

## Deploy (when Brandon says `deploy`; nginx is installed by hand, there is no script for it)

1. On the droplet, read-only first: `nginx -v` (>= 1.17.1 for dry run); `grep -n conf.d /etc/nginx/nginx.conf`
   (conf.d included in `http{}`); nothing in front of nginx (real client addresses); back up the live site file
   (`liquidgov.bak-pre-guestlimit-<utc>`) and confirm the live `/commons/*` blocks match the repo's fragment.
2. `cp nginx-commons-http.conf /etc/nginx/conf.d/commons-guest-zone.conf`; paste the new
   `location = /commons/presence/guest` block into the existing server{}; `nginx -t`; only then reload.
3. Verify without flooding production: one mint through `https://liquidgov.us/commons/presence/guest` is 200 with a
   real credential; five mints in a row are all 200; join/move/leave/lots unchanged; civic `/` 200 and `/internal/*`
   still 404 publicly; `nginx -T | grep -c 'limit_req '` is 1. The 429 itself is proven in CI, not by hammering the
   live site.
4. Rollback: remove the location and the conf.d file, `nginx -t`, reload (civic's blocks are never edited).

## Limits, said plainly

- The behavioural half ran nowhere before CI (no nginx binary in the dev environment). Its first CI run (`14278e4`,
  ubuntu-24.04 nginx) passed **33/33** (12 static + 21 behavioural), the whole suite 15 files / 229s, and the
  workflow's `nginx -t` step (now including the zone file) is green. It has still never run on the droplet's nginx.
- The droplet's nginx version and any front proxy are unchecked until the deploy (step 1).
- Not built here: backup retention (since built) and the client's `netMint` status check (since built: `netmint_built.md`).

## Deployed (2026-09-19, ~15:46Z, Brandon: `deploy`; Grok had accepted the rates and the HTML 429)

nginx only: no daemon, civic or client change. By hand, per the recipe above (there is no script for nginx):

1. **Read-only checks:** nginx **1.24.0** (dry run supported); `conf.d/*.conf` is included at http level and was
   empty; no existing `limit_req`/`commons_guest` anywhere; the public response is straight from nginx with no
   CDN/proxy headers in front (so the peer address is the real client). The live `location = /commons`,
   `/commons/`, `/commons/presence/` and `/commons/placed/` blocks were **identical** to the repo's
   (whitespace-insensitive); the guest block was not there.
2. **Backup:** `/etc/nginx/sites-available/liquidgov.bak-pre-guestlimit-20260919T154553Z`.
3. **Installed:** the zone as `/etc/nginx/conf.d/commons-guest-zone.conf`; the guest location inserted after the
   `/commons/placed/` block in `/etc/nginx/sites-available/liquidgov` (written beside, then moved into place,
   with an automatic rollback if `nginx -t` failed; it did not). `nginx -t` OK, `systemctl reload nginx`;
   nginx, liquidgov and commons-daemon all active.
4. **Verified without flooding:** exactly two active `limit_req*` lines in `nginx -T` (the zone, and the one
   `limit_req` in the guest location); five mints in a row through https all 200 with real credentials
   (`application/json`); `verify.sh --guest-name x` **22 passed, 0 failed** (civic `/` 200, `/commons/` 200, public
   `/internal/` still 404, lots 200, a real guest named `x` joined/moved/left, journal clean); zero "limiting
   requests" lines in the nginx error log; the live guest location and the three neighbouring blocks are identical
   to the repo's. The 429 itself was not exercised on production (proven in CI, per the plan).

**Rollback** (not needed): `cp` the `.bak-pre-guestlimit-*` file back over the site file, `rm` the conf.d file,
`nginx -t`, `systemctl reload nginx`.

**Side observation, unrelated to this change (already happening before it):** the nginx error log shows some
client polling `GET /commons/recipes` about every 6s, and 404ing (`open() "/opt/commons/recipes" failed`): that
route is deliberately not proxied (`deploy_plan.md`: the client never fetches catalogs from the daemon). Something
requests it anyway; nothing here acts on it.
