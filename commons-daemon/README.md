# commons-daemon

The Commons' sibling process (`sibling.md`). **Local-only as of
2026-09-16** — built and tested on this machine, not deployed. No SSH, no
systemd unit, no nginx config, no contact with the production droplet.
Civic (`liquidgov-platform/`, `http_server.cpp`) is untouched by anything
in this directory.

## What exists

- **SQLite ledger** (`schema.sql`, vendored `third_party/sqlite3`):
  catalogs (`items`, `recipes`, `maps`) + instances (`map_instances`,
  `players`, `inventory`, `placed`). Seeded once from `seed.sql` with the
  locked V1 data — three lab recipes (chalk-only, no lime), the twelve
  outdoor regions, and the demo house / lab interior templates.
- **Presence** (`src/presence.*`): RAM only, never a SQLite table, keyed
  by region (`"<col>,<row>"` outdoor, `"instance:<id>"` interior). Join,
  move, rekey (crossing a region without leaving a ghost in the old
  list), leave. Caps: 24/region, 96 global, matching `presence.md`.
- A minimal single-threaded synchronous HTTP server (`src/http.*`),
  deliberately shaped like civic's own (`http_server.cpp`) rather than
  pulling in a framework. Loopback-only; nginx would be the actual
  public front door once this is ever deployed (`nginx.md`).

Routes: `GET /health`, `GET /items`, `GET /recipes`, `GET /maps`,
`GET /presence/<region>`, `POST /presence/{join,move,rekey,leave}`.

## What does not exist yet, on purpose

- **Friends / cookie forwarding.** `glue.md` and `cookie.md` document the
  contract (forward the `Cookie` header to civic on localhost, civic
  resolves `lg_session` → account → username, never `?username=`) but
  there is no civic route to call yet, and calling one would mean editing
  `http_server.cpp` — out of scope for "civic frozen." The shape is
  written down; the wiring is later, explicit work.
- **Wall-slot / photo fetch.** Same reasoning — `GET /canvas-frame/...`
  already exists on civic (`canvas-frame.md`), but nothing in this daemon
  calls it yet.
- **Deploy anything.** systemd unit, nginx `location`, `/var/lib/commons/`
  ownership — all `sibling.md`'s job for a session Brandon explicitly
  starts, not this one.
- **Tile collision / movement.** Stays client-side JS in
  `commons/index.html`. `wasm.md`: a shared collision core between this
  daemon and the browser only earns its slot once this daemon is
  authoritative and needs to stop trusting client-reported movement —
  there's no reason to duplicate 40 lines of `movePlayer` before that's
  true.

## Building locally

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/commons_daemon            # binds 127.0.0.1:8765, data/commons.sqlite
```

`schema.sql` and `seed.sql` are read relative to the working directory,
not the binary's location — run it from `commons-daemon/`.

Verified end-to-end on this machine: catalog reads, the full presence
join/move/rekey/leave lifecycle (rekey confirmed to leave no ghost in the
old region), per-region and global cap enforcement, and the actual
persistence boundary across a real process restart — SQLite catalogs
survive, RAM presence does not (correct: `presence.md` says guests never
hit disk, and this proves it rather than asserting it).

## SQLite is vendored, not linked against the system

This WSL environment has `libsqlite3-0` (the runtime `.so`) but no
`-dev` package and no sudo/apt to install one. `third_party/sqlite3/`
is the official amalgamation (`sqlite3.c`/`sqlite3.h` from sqlite.org),
compiled as part of this project's own build. That also removes a system
dependency for whatever machine eventually builds this for real, not
just a workaround for local dev.
