# Grok review of fb889c0 — staged only, do not SSH yet

Civic `location /` untouched. Unit uncoupled. Loopback bind + systemd
`IPAddressAllow=127.0.0.0/8 ::1`. `-L` on schema.sql. `-j1`. SQLite
under `/var/lib/commons` with `ReadWritePaths`. Proxy port **8765** (not
nginx.md's 8091). **No apply.**

## Accept

Trailing slashes on **both** `location /commons/presence/` and
`proxy_pass http://127.0.0.1:8765/presence/`.

`/commons/presence/join` → remainder `join` → upstream `/presence/join`.
nginx.md's no-slash location + slash `proxy_pass` would have been
`//join`. That catch is real. Do not revert it.

No second `map $http_upgrade`. No `try_files` on `alias`. Cookie
forwarded, unread — fine until glue.

## Must-fix before apply (files, not droplet)

1. **`index.html` is not inside `commons-daemon/`.** Sync copies
   `commons-daemon/` → `/opt/commons-daemon`. Plan step 4
   `cp /opt/commons-daemon/commons/index.html` is a path that **does not
   exist**. Copy from repo `commons/index.html` → `/opt/commons/index.html`
   (second rsync or an explicit `cp` from the working tree). Alias serves
   that directory only — do not dump the daemon tree under `/opt/commons/`.

2. **Do not call `/commons/` "live" until the client uses a relative
   daemon.** Shipped `DAEMON` is still `http://127.0.0.1:8765` (or
   `?daemon=`). A visitor to `https://liquidgov.us/commons/` would talk
   to **their** loopback, not the droplet. Curl on the droplet can prove
   nginx; browsers cannot until `index.html` does:

   ```
   var DAEMON = (location.hostname === "127.0.0.1" || location.hostname === "localhost")
     ? (validated ?daemon= or "http://127.0.0.1:8765")
     : (location.origin + "/commons");
   ```
   and fetch `/commons/presence/...` — **keep** the slash after
   `presence`. Loopback keeps today's cross-origin :8765 + CORS echo.

Apply order after those two land: health on :8765 → nginx -t → reload →
curl `/commons/` and `POST /commons/presence/guest` → **then** a real
browser. If unit fails on `IPAddressAllow` (missing bpf), drop the
IPAddress* pair and keep the 127.0.0.1 bind; do not open `0.0.0.0`.

## Should-fix (not blocking review)

- `location = /commons { return 301 /commons/; }` (no-slash URL falls
  through to civic `/`)
- Update `nginx.md` to the slashed 8765 shape so the next paste is not
  the 404 version
- FetchContent: if droplet `curl -I https://sqlite.org` fails, tarball
  / `SOURCE_DIR` before cmake, not apt

## Fail if “fixed” by

- Pasting into civic `location /`
- Second `map`
- rsync of `*.sqlite` or anything under `/opt/liquidgov`
- `listen 0.0.0.0`
- Opening LGP1

Paste `CLAUDE_DEPLOY_PREFLIGHT.md`. Still no SSH until Brandon says go
**after** that patch.
