# Objects D+C1+C2 are live

Brandon ran `--label objects-d-c` from `/home/bbottin/lg` at `ccdf06f`.
Prior `git pull` had stalled on dirty `lib.sh` / `test_migrations.sh`;
`git reset --hard origin/master` then the deploy was clean. Catalog-A
(`f8d111f`, `--label catalog-a`) was already on the box.

- Backup: `/var/lib/commons/backups/commons.sqlite.pre-objects-d-c-20260919T205716Z`
- Verify: 33 passed, 0 failed.
  `items` 8→11, `recipes` 8 unchanged, `recipe_outputs` 8 unchanged.
  Inventory 7, players 1, civic 200, internals 404.
- Daemon revision on the box: `ccdf06f9ec85`.
- Client synced last.

Hard-refresh `/commons/`. Oak in North Woods, apple pile in the Orchard,
shell crate on the Shore sand; extra furniture in every house; floor-place
the last pickup (cap 8, owner only).
