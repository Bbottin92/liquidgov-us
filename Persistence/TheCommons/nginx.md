# nginx — reverse proxy for the sibling

Not a BUILD. Do not edit production nginx until Brandon says sibling.
Do not invent the civic cookie name — read `Set-Cookie` in
`http_server.cpp` when glue starts (`glue.md`).

Goal: `https://liquidgov.us/commons/` is same-origin with civic so the
existing session cookie is sent. Game port stays on loopback.

## Split of work

| URL | Who |
|---|---|
| `/` `/login` `/mail` `/proposals` `/u/…` | civic (`liquidgov_http`) — **unchanged** |
| `/commons/` `/commons/index.html` | nginx static (`alias`, not the daemon) |
| `/commons/presence` (name TBD) | `proxy_pass` `http://127.0.0.1:<gameport>/` |
| Hall door | still a full URL to `/proposals`; not proxied through the game |

Bind the daemon to **127.0.0.1 only**. If nginx dies, the game port is
not on the public NIC.

## Static `location` (trailing-slash trap)

```
location /commons/ {
    alias /opt/commons/;
    try_files $uri $uri/ /commons/index.html;
}
```

`alias` **must** have a trailing slash to match `location`’s slash or
nginx concatenates wrong (`/opt/commonsindex.html`). Prefer `alias`
over `root` so files live beside `/opt/liquidgov`, not inside the LGP1
store.

`index.html`: `Cache-Control: no-cache`. Hashed tilesets later can be
immutable. Do not `proxy_pass` HTML through the daemon.

## Presence `location` (more specific, so it wins)

```
location /commons/presence {
    proxy_pass         http://127.0.0.1:8091/;   # port TBD; loopback only
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

Map `$connection_upgrade` the usual way (`upgrade` if `$http_upgrade`).
2 Hz presence is either WebSocket or SSE; both need buffering off and a
long read timeout or nginx kills idle walkers.

**Cookie:** forward `$http_cookie` as-is. The daemon may then call civic
on localhost with that same header. Do not strip `HttpOnly` cookies in
JS and re-send them as query params.

`location /commons/presence` is longer than `/commons/`, so it wins
without `^~` tricks. Do not `proxy_pass` the static tree “just in case.”

## What not to touch

- Civic `server { }` TLS, `root`, existing `location /`.
- `client_max_body_size` until photo POST exists (then raise **only**
  the photo location, not globally).
- Adding CORS headers on `/commons/` — same origin, none needed.
- Exposing `:8091` in a `listen` / ufw hole.
- `proxy_pass` to civic from the game location (friends query is
  daemon → `127.0.0.1:<civic>` , not the browser).

## Civic still in front

Today nginx already reverse-proxies to `liquidgov_http`. Add two
`location` blocks; do not rebuild that server. If a `/commons` request
falls through to civic, civic will 404 — that’s a missing `location`,
not a reason to teach `http_server.cpp` the game.

## Subdomain (not assumed)

`commons.liquidgov.us` needs a second `server { }` and a cookie
`Domain=` change on civic login. More blast radius. Path `/commons/`
avoids it. Do not cut a cert for the subdomain until that cookie read
says we must.

## Reviewer fail

- Game daemon `listen 0.0.0.0`
- `alias /opt/commons` without the trailing slash
- Buffering left on a presence stream
- Putting `commons.sqlite` under the static alias (downloadable DB)
- Rewriting civic locations to “make room”
