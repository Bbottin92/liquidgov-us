# Grok review of commons-daemon

Local sibling process. Civic frozen. Do **not** deploy.

## Closed

Must-fixes (92e379d, d2b5bf4, 9f7d609): duplicate join, rekey `total_`, 413 + truncate.

Should-fixes in **b4bf924** — accepted:

- `move` stamps `last_update_ms`
- Lazy idle reap 30s on every public PresenceStore method (no timer thread)
- One schema: git symlink `commons-daemon/schema.sql` → `Persistence/TheCommons/schema.sql`
- 9.5 MB blob gone; FetchContent amalgamation 3.53.4, `URL_HASH SHA256=`
- Server-issued ids correctly **parked** until `commons/index.html` asks

## Still a deploy gate (not this week)

- **FetchContent needs network at cmake.** Droplet configure without
  sqlite.org will fail. Offline: `FETCHCONTENT_SOURCE_DIR_...` or a
  tarball next to the tree. Do not rediscover apt/`-dev`.
- **CMake `SHA256` ≠ sqlite.org `SHA3-256`.** The download page hash
  (`628a44cf…`) is SHA3. The pin (`1e71ddf9…`) is SHA256 of the zip.
  Both can be right. Do not “fix” the pin to the page hash; CMake 3.16
  `URL_HASH` has no SHA3.
- **Git symlink.** WSL follows it. A Windows clone without
  `core.symlinks` or `rsync` without `-L` turns schema.sql into a path
  string and `exec_file` runs garbage. Sibling install should copy the
  real file (or CMake `configure_file`).
- Client-chosen `id` until an explicit client + daemon round.
- systemd / nginx / `listen 0.0.0.0` still wait on Brandon.

## Fail if “fixed” by

- Opening LGP1, wiring cookie, npm, civic edits, UDP/h2/h3 in the daemon
