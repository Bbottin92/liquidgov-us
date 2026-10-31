# www -> apex — APPLIED 2026-09-20 (nginx only; verified live; renewal proven)

Brandon: `BUILD www apex` (the last leftover from Grok's list; `site-sense-built.md`: "`www.liquidgov.us` does not 301 to apex — nginx, named separately"),
then `deploy`. Built and tested first (read-only checks plus one throwaway loopback nginx that was removed), then applied. **nginx only: civic and the daemon were not restarted.**

## What was wrong (measured live, 2026-09-20)

- `www.liquidgov.us` resolves to the same droplet (`143.198.159.58`) and the ONE certificate (SAN `liquidgov.us` + `www.liquidgov.us`, valid to 2026-12-11) covers it.
- nginx named both hosts in the same server blocks, so `https://www.liquidgov.us/` (and `/laws`, `/commons/`, everything) returned **200: a second copy of the whole site on a second origin**: separate cookies (a login on www is not a login on the apex), separate localStorage/guest state, duplicate URLs for search and share cards.
- `http://www.liquidgov.us/...` redirected to `https://www.liquidgov.us/...` (it stayed on www).

## The change (two small edits; everything else untouched)

`deploy/nginx-www-apex.py` is a pure, idempotent, refuse-if-unsure edit of the live site config (`/etc/nginx/sites-available/liquidgov`, the file `nginx -T` reports as loaded):
1. In the 443 server, right after `server_name liquidgov.us www.liquidgov.us;`: `if ($host = www.liquidgov.us) { return 301 https://liquidgov.us$request_uri; }`. `return` inside `if` at server level is the safe use of `if`; it runs in the rewrite phase, before any location, so every path on www is redirected (`/internal/` included: the apex then answers as before). `$request_uri` keeps the path and the query string; `$host` is lower-cased by nginx, so `WWW.` and a Host with a port match too.
2. In certbot's port-80 block, its `if ($host = www.liquidgov.us) { return 301 https://$host$request_uri; }` now goes straight to `https://liquidgov.us$request_uri` (one hop instead of www -> https://www -> apex).

`server_name` keeps BOTH names, so the certificate match and certbot's own handling are unchanged; every `location`, `listen`, `ssl_*` and certbot line is byte-for-byte as it was (the test pins the diff to exactly 8 added lines and 2 removed). It refuses (exit 2) any config that is not the known shape: no port-80 www block, two 443 blocks, no block naming both hosts, or a half-applied result.
301 (permanent) as asked. A POST to www would become a GET on the apex; nobody posts to www (forms live on pages that now load from the apex), so 308 was not needed. No `Domain=.liquidgov.us` on the session cookie (Grok's note): the redirect is enough.

`deploy/nginx-www-apex.sh` applies it: **plan-only by default**; `--go` takes a backup (`/var/backups/nginx-www-apex-<time>/`), edits, `nginx -t`, graceful reload, then verifies 19 redirect/apex checks plus that every `/commons/` content type and the manifest types are unchanged, and **rolls back automatically** on any failure. The repo's older site-config copy (`deploy/nginx-liquidgov.us.conf`) shows the 443 half.

## Tests

`tests/test_nginx_www.sh` (34 assertions locally, 37 in CI) + `tests/nginx_www_behavior.sh` (17 checks) + `tests/fixtures/nginx-liquidgov.live.conf` (a byte-identical copy of the live file, md5 `92e63e28d649`; no secrets).
- **Transform** (python3): recognised as not applied / applied; exactly two `return 301 https://liquidgov.us$request_uri;`, none that keep the www host; certbot's apex redirect untouched; `server_name` still names both hosts; line counts for `location`/`listen`/`ssl_certificate`/`proxy_pass`/`limit_req`/`alias` unchanged; the diff is exactly 8 added and 2 removed; the 443 redirect is above the first location; idempotent, `--write` identical; **refuses** four unknown shapes (my first "two 443 blocks" fixture was malformed because the live file has no trailing newline, so it refused nothing; a precondition assertion now proves the fixture really has two).
  **Mutation-checked, 9 breakages**: 8 fail the test (redirect keeps the www host at 443 or at 80, not idempotent, no refusal for two 443 blocks, www dropped from `server_name`, a stray line, `--check` always true, redirect placed after the locations). The ninth (no explicit half-applied refusal) is an equivalent mutant: the transform's final sanity check still refuses it.
- **Behaviour on a real nginx** (its own prefix and loopback ports, a throwaway certificate, a stub upstream): the CONTROL (the untransformed live config) really does serve `https://www/` as 200 and keeps `http://www` on www; the transformed config redirects `www` `/`, a path with a query string, a deep path, `/commons/`, `/internal/...`, an upper-case Host and a Host with a port, all to the apex; `http://www/x?q=1` goes to `https://liquidgov.us/x?q=1` in ONE hop; `http://apex` is unchanged; an unknown host on port 80 is still 404; the apex is still proxied (`/`, `/laws`) and never redirects (no loop).
  It ran first on the droplet's own nginx 1.24 as a scratch loopback instance (ports 18443/18080/18191, removed afterwards; the live config md5 was identical before and after): 17 checks, 0 failed; and in CI on every push (CI installs nginx; a missing nginx there is a failure). WSL here has no nginx, so locally that half skips. CI on `290c56e`: green, `behavior: 17 checks, 0 failed`.

## Applied (2026-09-20, from 8925d8c; the nginx deploy files identical to 290c56e)

Order, with guards that abort before touching anything: the live config's md5 had to equal the fixture's; no certbot/apt/pip process on the droplet (a certbot dry-run started by another session was running when Brandon said `deploy`, so I waited ~150 s for it to finish rather than collide with the nginx plugin's temporary config edits);
**baseline `certbot renew --dry-run --no-random-sleep-on-renew`** (Let's Encrypt staging; validates `liquidgov.us` and `www.liquidgov.us` over HTTP-01) -> all simulated renewals succeeded, config md5 unchanged afterwards; then `deploy/nginx-www-apex.sh --go` (backup `/var/backups/nginx-www-apex-20260920T041810Z/liquidgov`, edit, `nginx -t`, graceful reload, all 19 verify checks ok);
then **the same dry-run against the NEW config** -> all simulated renewals succeeded again, and both redirects were still loaded afterwards. So renewal for both names is proven, not assumed (the authenticator is `nginx`; the plugin's challenge handling works with the redirects in place).

Live afterwards: `https://www.liquidgov.us/`, `/commons/`, `/login`, `/laws?x=1&y=2` -> `301 https://liquidgov.us/<same path and query>`; `http://www.../x?q=1` -> one hop to `https://liquidgov.us/x?q=1`; `http://liquidgov.us/` unchanged; apex `/` 200, `/commons/` 200, `/world` 303 to `/commons/`, public `/internal/*` 404; `/commons/` content types and the manifest types unchanged;
`nginx -t` OK, no error-log lines, `liquidgov` and `commons-daemon` NRestarts 0, `verify.sh` 23/23. Live config md5 after: `cd4488a32113`.
**One line of Grok's confirm list reads differently:** `http://www.liquidgov.us/.well-known/acme-challenge/ping` is a 301 to the apex (`https://liquidgov.us/.well-known/acme-challenge/ping`), which Grok's list said it should NOT be. That check assumed a `webroot` authenticator; here the authenticator is `nginx`, and the before/after dry-runs show the challenge for `www.liquidgov.us` still validates. (That path was already a redirect before this change, to `https://www.`.)

Rollback: `cat /var/backups/nginx-www-apex-20260920T041810Z/liquidgov > /etc/nginx/sites-available/liquidgov && nginx -t && systemctl reload nginx` (the script does this itself on any verify failure).

## Slips while doing it (no effect on the site)

- My first baseline dry-run used `timeout 300` and was killed during certbot's deliberate random delay for non-interactive renewals (`random delay of 327 seconds`); it left no residue (config md5 unchanged, no challenge includes, no process). A manual dry-run needs `--no-random-sleep-on-renew` (what the systemd unit passes). The guard aborted rather than applying on a baseline that had not passed.
- My first apply attempt aborted at once because a `git pull` had moved local HEAD (another session's webpush commit); the guard now checks that the deploy files are identical to `290c56e` instead of comparing the hash.
- My cleanup of the scratch nginx instance once used `pkill -f` on its own path and killed its own SSH command before removing the directory; I removed the scratch dir by exact path and nothing else was left.
- **This document was committed empty in `290c56e`** (an interrupted first run created the file, a later run's patch step failed silently on it, and I committed without checking it had content). Rewritten here.

## Consequences to know

- A visitor who was logged in on `www.` is logged out after the redirect (their cookie belonged to that host); they log in once on the apex. Bookmarks and shared `www` links keep working (301 to the same path).
- Search engines and share cards consolidate on the apex; civic's share-card URLs were already absolute `https://liquidgov.us/...`, so nothing in the app changed.

## Found on the way (its own task, now done by another session)

**certbot on the droplet was broken when I looked (2026-09-20).** `certbot.service` had FAILED on 2026-09-19 14:41 UTC and `certbot --version` / `certbot renew --dry-run` crashed with `AttributeError: module 'lib' has no attribute 'GEN_EMAIL'`. The certificate expires **2026-12-11** and automatic renewal starts around 2026-11-11, so unfixed it would have taken the whole site down.
I raised it as its own task; a separate session found the cause (a system-wide `pip install pywebpush` put `cryptography 50` in `/usr/local` ahead of the apt one), fixed and verified it (`certbot-fix-built.md`, commits `61eef87`, `8925d8c`, `2e19e2e`: a PYTHONPATH drop-in first, then webpush moved into its own venv and the drop-in removed). Both of my certbot dry-runs passed after that work.
Lesson for anyone applying nginx changes: a certbot run edits nginx config temporarily (nginx authenticator), so do not apply while one is in progress.
