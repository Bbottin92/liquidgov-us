# commons-daemon deploy scripts

The daemon deploy as files: **drift / backup / sync / build / restart / verify**. Nothing here runs by itself,
and CI never runs it against the droplet (CI drives the same scripts against a fake droplet in
`tests/test_deploy.sh`). Brandon's literal **`deploy`** word is still what starts a real one.

```
deploy-daemon.sh                      # PLAN ONLY: the read-only drift check + what would happen
deploy-daemon.sh --go --baseline 60f6745      # first use (no marker yet on the droplet); 60f6745 is live as of 2026-09-19
deploy-daemon.sh --go                 # afterwards: the droplet's own marker says what is live
deploy-daemon.sh --go --with-client   # daemon, then the client LAST
deploy-daemon.sh --go --guest-name x  # verify joins the real Square as a guest named x (the reqjson check)
```

| script | does | writes to the droplet? |
|---|---|---|
| `drift-check.sh` | Is the droplet still what was last deployed, and what will this deploy change? | never |
| `backup-db.sh [label]` | Online copy of `commons.sqlite` (SQLite backup API, never `cp`), `integrity_check`, row counts | a new file in `/var/lib/commons/backups/` |
| `sync-to-droplet.sh [-n]` | `commons-daemon/` -> `/opt/commons-daemon` (+ `.deployed-rev` marker) | source files |
| `sync-client-to-droplet.sh [-n]` | `commons/` -> `/opt/commons` (+ `.deployed-client-rev`, kept OUTSIDE the served dir) | client files |
| `build-on-droplet.sh [--reconfigure]` | `cmake --build -j1`; reconfigure only when asked | the build dir |
| `verify.sh [--guest-name N] [--compare-backup P]` | systemd, journal, loopback, public routes, a real guest, row counts | one throwaway guest (RAM) |
| `prune-backups.sh [--delete]` | Retention for `/var/lib/commons/backups`: delete only if outside the newest 14 AND older than 30 days (never the newest, never < 24h); a backup's `-wal`/`-shm` go WITH it (exact names; orphans are reported, never deleted); DRY RUN unless `--delete`; not run by `deploy-daemon.sh` | only with `--delete`: removes old backup files |
| `deploy-daemon.sh` | runs them in order; refuses to continue past a failed step | all of the above |
| `lib.sh` | shared config, the exclusion lists, the marker | (sourced) |

## The order, and why

1. **Drift check.** The droplet is the runtime truth (a hand-patch on the box once forked civic three ways).
   The check exports the *last deployed revision* from git (named by the droplet's marker) and compares it to
   the droplet by checksum: anything different was edited on the box, and a sync would silently overwrite it.
   It also prints what this deploy will change, for you to read against the change you meant to ship, and it
   refuses a working tree that differs from HEAD (a sync ships the tree, not a commit) or a HEAD that is not
   pushed (`--allow-dirty` / `--allow-unpushed` are explicit overrides).
2. **Backup**, before anything changes. If it fails nothing is synced.
3. **Sync**, then 4. **build** (`-j1` on a 512MB box, no reconfigure unless `CMakeLists.txt` changed since the
   live revision), 5. **restart**. A restart drops all in-RAM presence and every guest credential (guests re-mint
   through the existing 403 path); houses are durable.
6. **Verify** (`--compare-backup`): a table with *fewer* rows than the backup fails the deploy.
7. **Client last**, once the daemon it talks to is live.

**Civic goes first when it changed.** These scripts never touch civic. If `liquidgov-platform/` changed since
the live revision, `deploy-daemon.sh --go` refuses until you have deployed civic with the repo-root
`deploy/sync-to-droplet.sh` and say so with `--civic-done`.

## What is never synced

`build/`, `.git`, `tests/`, every `*.sqlite*` file, `backups/` (the list is `DAEMON_EXCLUDES` in `lib.sh`,
shared by the sync and the drift check so they cannot disagree). Live data must never be in the synced tree
at all (the 2026-09-12 civic incident: a stray local `.lgp` overwrote production accounts).

## When something fails

- **Drift found:** do not sync. `scp` the droplet's file down, diff it against git, decide which is right, and
  commit the droplet's change (or discard it on the box) before deploying.
- **Verify failed:** the new code is live. Roll back by checking out the previous revision and deploying it
  (`--baseline <the failed one>`). Restore the backup only if data is actually wrong, and only with the service
  stopped: `systemctl stop commons-daemon`, copy the backup over `commons.sqlite` (and remove any `-wal`/`-shm`),
  `systemctl start commons-daemon`.
- **A sync that died half way** leaves the marker at the *previous* revision (it is written only after a
  complete sync), so the next drift check will report the half-copied files. That is the intended outcome.

## Knobs (all environment variables; defaults are production)

`DEPLOY_HOST` (root@143.198.159.58), `LIQUIDGOV_DEPLOY_KEY`, `DEPLOY_DAEMON_DIR`, `DEPLOY_CLIENT_DIR`,
`DEPLOY_DATA_DIR`, `DEPLOY_SERVICE`, `DEPLOY_CIVIC_SERVICE`, `DEPLOY_SERVICE_USER`, `DEPLOY_PUBLIC_BASE`,
`DEPLOY_PREFIX`; `DEPLOY_SSH` (the whole ssh command) and `DEPLOY_SKIP_SITE=1` exist for the tests.

## Not here

nginx (`nginx-commons.conf` and `nginx-commons-http.conf` are installed by hand, their own deploy word; see `nginx-guest-built.md`), scheduling retention (a separate decision; see `backup-retention-built.md`), and `commons-daemon.service`
installation (a one-time step in `Persistence/TheCommons/deploy-plan.md`).
