# nginx — reverse proxy for the sibling

Not a BUILD. Do not edit production nginx until Brandon says sibling.

Cookie is locked: **`lg_session`**, HttpOnly, Path=/, SameSite=Lax,
host-only, Secure when civic sees `X-Forwarded-Proto: https` (`cookie.md`,
`glue.md`). Path `/commons/` is the deploy shape. Subdomain needs a civic
`Domain=` it does not have.

Goal: `https://liquidgov.us/commons/` is same-origin so the browser
**sends** `lg_session`. Game port stays on loopback.

## Split of work

| URL | Who |
|---|---|
| `/` `/login` `/mail` `/proposals` `/u/…` `/canvas-frame/…` | civic — **unchanged** |
| `/commons/` `/commons/index.html` | nginx static (`alias`) |
| `/commons/presence/` | `proxy_pass` `http://127.0.0.1:8765/presence/` |
| Hall door | full URL to `/proposals`; not through the game |

Bind the daemon to **127.0.0.1 only**. If nginx dies, the game port is
not on the public NIC.

Do **not** `proxy_pass` `/canvas-frame/` to the daemon. The wall-slot
(sibling-era) hits civic on the same host. `/latest` is a civic path
(`frame-index.md`).

## Static `location` (trailing-slash trap)

```
location /commons/ {
    alias /opt/commons/;
    index index.html;
    add_header Cache-Control "no-cache" always;
}
```

`alias` **must** have a trailing slash to match `location`’s slash or
nginx concatenates `/opt/commonsindex.html`.

**Do not `try_files` under `alias`.** nginx `try_files` uses `$uri` as a
filesystem path and fights `alias` (wrong file or a redirect loop).
One `index.html` does not need an SPA fallback.

Prefer `alias` over `root` so files live beside `/opt/liquidgov`, not
inside the LGP1 store. Do not `proxy_pass` HTML through the daemon.

## Presence `location` (more specific, so it wins)

**Correction (`GROK_DEPLOY_REVIEW.md`, found staging `fb889c0`): the
shape below used to have no trailing slash on `location` while
`proxy_pass` had one.** Traced through nginx's own documented
replacement rule, that combination sends `/commons/presence/join`
upstream as `//join`, not `/presence/join` — the daemon's exact-match
routes (`http.cpp`) would 404 on every single presence call. Never
caught before because no nginx config for this project had actually
been installed anywhere yet. Trailing slash on **both** sides fixes it;
this is nginx's own documented safe pattern for this rewrite. Port is
**8765** (this daemon's actual coded default, `main.cpp`), not the
`8091` placeholder this doc used to carry.

```
location /commons/presence/ {
    proxy_pass         http://127.0.0.1:8765/presence/;
    proxy_http_version 1.1;
    proxy_set_header   Host $host;
    proxy_set_header   Cookie $http_cookie;
    proxy_set_header   Upgrade $http_upgrade;
    proxy_set_header   Connection $connection_upgrade;
    proxy_set_header   X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header   X-Forwarded-Proto $scheme;
    proxy_read_timeout 86400;
    proxy_buffering    off;     # SSE / 2 Hz stream
}
```

Map `$connection_upgrade` the usual way. Buffering off, long read
timeout, or nginx kills idle walkers.

**Cookie:** `$http_cookie` as-is. Daemon forwards that header to civic
localhost — it does not parse `lg_session` and open LGP1
(`GROK_GLUE_REVIEW.md`).

`location /commons/presence/` is longer than `/commons/`, so it wins
without `^~`. The client must call this as `/commons/presence/<rest>`
— the trailing slash after `presence` is load-bearing, not stylistic;
dropping it falls through to the static `/commons/` location instead
(a confusing 404-via-static-alias, not an obvious proxy failure).

## Civic `location /` must keep proto

Existing civic proxy already (must still) send `X-Forwarded-Proto
$scheme`. Drop that when adding `/commons/` and `g_https` goes false →
login cookies lose `Secure`. Add two locations; do not rewrite civic’s.

## What not to touch

- Civic TLS, `root`, `location /`.
- `client_max_body_size` globally (photo later: one location only).
- CORS on `/commons/` — same origin, none needed.
- Exposing `:8765` in `listen` / ufw.
- Browser talking to civic via the game proxy (friends is daemon →
  `127.0.0.1:<civic>`).

If `/commons` falls through to civic, civic 404s. That is a missing
`location`, not a reason to teach `http_server.cpp` the game.

## Subdomain (not this deploy)

Needs a second `server { }` **and** civic `Domain=.liquidgov.us`.
Path `/commons/` avoids both. Do not cut a cert for it.

## Reviewer fail

- Game daemon `listen 0.0.0.0`
- `alias /opt/commons` without the trailing slash
- `try_files` under `alias`
- Buffering left on a presence stream
- `commons.sqlite` under the static alias (downloadable DB)
- Proxying `/canvas-frame/` through the game
- Rewriting civic locations to “make room”
- Stripping Cookie and putting `lg_session` on the query string
