# Deploy scripts — built (files + fake-droplet tests; never run against the droplet)

Brandon: "BUILD deploy scripts" (`GROK_NEXT.md` #2: "drift / backup / sync / verify as files under
`commons-daemon/deploy/`"; `GROK_REQJSON_REVIEW.md`: "Drift check, manual sqlite copy, `-j1`, no
reconfigure"). **No droplet was contacted**, no nginx, no deploy: SSH still needs the literal `deploy` word.
The scripts encode the recipe the last several deploys actually followed (`hardening-built.md` "Deployed").

## What ships (`commons-daemon/deploy/`, runbook in its `README.md`)

| file | role |
|---|---|
| `lib.sh` | shared config (every knob an env var), the ONE exclusion list, the deployed-revision marker |
| `drift-check.sh` | read-only: does the droplet still hold what was last deployed? what will this deploy change? |
| `backup-db.sh [label]` | online SQLite backup (backup API, never `cp`), `integrity_check`, row counts, never overwrites |
| `sync-to-droplet.sh`, `sync-client-to-droplet.sh` | the two existing syncs, reworked onto `lib.sh`: shared excludes, refuse a dirty tree, write the marker, `-n` dry run |
| `build-on-droplet.sh` | `cmake --build -j1`; reconfigure only with `--reconfigure` |
| `verify.sh` | systemd, journal, loopback health, public routes, a real guest lifecycle, row counts vs the backup |
| `deploy-daemon.sh` | the sequence. **Without `--go` it only plans** (runs the read-only drift check) |

## The design choices worth knowing

- **Drift is measured against the last-deployed revision, not just the working tree.** The droplet keeps a
  marker (`/opt/commons-daemon/.deployed-rev`, and `.deployed-client-rev`, both outside the nginx-served
  directory). The check exports that revision from git and compares it to the droplet by checksum, so a hand
  edit on the box is reported instead of silently overwritten; then it prints what this deploy will change.
  First use (no marker yet): `--baseline 60f6745` (the hardening commit, live as of 2026-09-19).
- **A sync ships the working tree, so the tree must be HEAD:** dirty or untracked files in the synced paths
  refuse (`--allow-dirty`), and an unpushed HEAD refuses (`--allow-unpushed`). Deployed code is code git saw.
- **Exclusions are one list** shared by sync and drift (`build`, `.git`, `tests`, every `*.sqlite*`, `backups/`).
  The old daemon sync excluded only `data/*.sqlite*`; it now excludes sqlite anywhere, and `tests/`.
- **`deploy-daemon.sh --go` order:** drift -> backup (a failure here syncs nothing) -> sync -> build (`-j1`, a
  reconfigure only if `CMakeLists.txt` changed since the live revision) -> restart -> verify
  `--compare-backup` (fewer rows than the backup fails) -> client, last, with `--with-client`. Civic changed
  since the live revision? `--go` refuses until you have deployed civic and pass `--civic-done`.
- **Behaviour changes to the two existing sync scripts:** the positional `[user@]host[:path]` argument is gone
  (use `DEPLOY_HOST`), and they now refuse a dirty tree and write the marker.

## Tests (in CI via `run_all.sh`; plus a lint step in the workflow)

`tests/test_deploy.sh` (119 assertions) builds a throwaway git repo with a copy of the scripts and a FAKE
droplet: a temp directory behind a shim `ssh` (it runs the command locally) with stub `systemctl`,
`journalctl`, `cmake`, `chown`; `verify.sh` talks to the real test daemon over loopback. It pins: no marker ->
exit 2; the sync ships the real `schema.sql` and never tests/sqlite/.git; markers; drift on an edited AND a
deleted droplet file (droplet-only files are reported, not drift); dirty/untracked/unpushed refusals and their
overrides; backups are valid, integrity-checked, mode 600, create no `-wal`/`-shm` beside the live DB, refuse to
overwrite, and fail without a database; `-j1` and no configure unless asked; verify catches an error line, an
inactive service, civic down, no `listening` line, and lost rows (and accepts a guest named `x`); `--go` stops
before anything changes on drift, on a failed backup, and on an undeployed civic change; the full run does
backup -> sync -> build -> restart -> verify -> client in that order.

**Mutation-checked:** 10 deliberate breakages (drift never blocks, dirty/unpushed accepted, tests/ or sqlite
shipped, `-j2`, lost rows ignored, journal errors ignored, overwrite guard removed, civic guard removed) each
fail the test. The first run also caught two real bugs in my own scripts before they could reach the droplet:
`list_changed` passed its arguments swapped to the rsync helper, and a missing marker killed `drift-check.sh`
with exit 1 under `pipefail` instead of the intended "no baseline" exit 2.

The workflow gains a step: `bash -n` on every deploy/test script and `shellcheck -x -S warning` on
`deploy/*.sh` (no ssh, no secrets, no droplet key).

## Limits, said plainly

- **Real ssh, systemd, sudo-less root paths and the real nginx are not exercised.** The fake droplet proves the
  logic (ordering, refusals, markers, exit codes); the first real run is its own check. That first run is
  naturally the read-only one: `deploy-daemon.sh --baseline 60f6745` (plan only) is safe to run on `deploy`
  before `--go`.
- `backup-db.sh` opens the live DB read-only as root and runs `python3` on the droplet (it was there for the
  earlier manual copies). If `mode=ro` on a WAL database ever fails there, it fails loudly and takes no backup;
  nothing is synced.
- `verify.sh` mints one real guest through the public site (a RAM occupant for about a second), like any visitor.
- Not built: backup **retention** (backups accumulate; pruning is the last named item), nginx `limit_req`.
