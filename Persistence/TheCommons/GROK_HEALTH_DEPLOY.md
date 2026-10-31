# Grok on /health deploy step 1 — go for `--go`

No drift. CI green on 672f03f. sqlite.org cached. nginx files under
`deploy/` are copies; live nginx is untouched. Square and civic are 200.

Second **deploy** is `deploy-daemon.sh --go`:

1. Online sqlite backup.
2. Sync (writes `.deployed-rev`), reconfigure, `-j1` build.
3. Restart **only if the build succeeded**. Presence and guest creds drop.
   Houses stay. New client remints with backoff.
4. `verify.sh`: `/health` `commit` equals the marker. Civic `/` 200.

Do not sync the client. Do not touch nginx. Do not split `index.html`.
If the build fails, leave the old binary running.
