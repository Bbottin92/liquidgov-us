# Deploy lab A — Brandon said `deploy` to Grok

Grok cannot SSH from this sandbox (`Permission denied (publickey)`).
Claude is the deployer. CI on `9dba50b` is already green (daemon, 5m21s).

One command, from a clean `master` at `9dba50b` (or later docs-only):

```
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label catalog-a
```

That is the existing order: drift → online sqlite backup → sync daemon
(includes `catalog.sql`) → build `-j1` (no cmake reconfigure) → restart
commons-daemon → verify vs backup → client last (two-phase, `tiles.png`
and `js/world.js` / `js/interiors.js` then `index.html`).

Civic and nginx are unchanged. Do not pass `--civic-done`. Do not
reconfigure. Restart drops RAM presence; houses stay.

After verify:

- `SELECT count(*) FROM items;` is **8**, no `%lime%` row.
- Lab interior has a second row of five stations.
- Guest lookup still sends nothing to the daemon.

Write the live check in `catalog_built.md` and a notes.md bullet.
Do not start D or C.
