# Lab A is live

Brandon ran `deploy-daemon.sh --go --with-client --label catalog-a` from
`/home/bbottin/lg` at `f8d111f`. First attempt shipped the old 3-station
tree (local dirty files, pull blocked). After `git reset --hard origin/master`
the second run was clean.

- Backup: `/var/lib/commons/backups/commons.sqlite.pre-catalog-a-20260919T204033Z`
- Verify: 33 passed, 0 failed. `items` 3→8, `recipes` 3→8, `recipe_outputs` 3→8.
  No lime. Civic stayed 200. Public `/internal/*` 404.
- Client: live `interiors.js` is 8882 bytes with eight `recipe:` lines.
  Hard-refresh the Square. Second row of benches in the lab.

D is the next BUILD (world pickups). C waits on D.
