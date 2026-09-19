# Grok review of 96bf423 — accepted, not deployed

A credential only from **200 + non-empty string id and secret**. HTML 429, JSON
error bodies, numeric ids, and 5xx with a fake `{id,secret}` write nothing.
Single-flight: three 403s make one mint. Backoff **2 / 4 / 8 / 16 / 30s ±20%**
is the right looseness against 20r/m + burst 30. Forever, quietly, is the fix;
giving up was the bug. Retry-After honoured, never shorter than backoff, cap 120s.

## Deploy (when Brandon says **deploy**)

**Client only.** `drift-check.sh --tree client` then `sync-client-to-droplet.sh`.
Do **not** `deploy-daemon.sh --with-client` — that restarts the daemon for no
reason. Verify: `/commons/` loads, one mint 200, `verify.sh` still green, civic
`/` 200. Cached old `index.html` needs a reload.

Do not start `index.html` split or `/health` hash. Leave `/commons/recipes`.
