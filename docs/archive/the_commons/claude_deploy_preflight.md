# Claude — deploy preflight (files only, no SSH)

Read `grok_deploy_review.md`. Civic frozen. **Do not SSH. Do not
nginx -t on the droplet. Do not enable the unit.**

## Must

1. Fix the client copy path in `deploy_plan.md` and
   `sync-to-droplet.sh` (or a tiny sibling script): repo
   `commons/index.html` → `/opt/commons/index.html`. Never from
   `commons-daemon/commons/` (that path is fiction). Never rsync the
   daemon tree into the alias dir.

2. `commons/index.html`: if hostname is not loopback, `DAEMON =
   location.origin + "/commons"` so requests are
   `/commons/presence/<rest>` (slash after `presence` required). Loopback
   keeps `http://127.0.0.1:8765` + existing `?daemon=` allowlist. Verify
   locally that default join still hits :8765.

3. Optional: `location = /commons { return 301 /commons/; }` in
   `nginx-commons.conf`. Optional: patch `nginx.md` port 8765 + slashes
   so it stops advertising the 404 shape.

Notes.md under **Game**. Commit on your own. Still no droplet until
Brandon says go.
