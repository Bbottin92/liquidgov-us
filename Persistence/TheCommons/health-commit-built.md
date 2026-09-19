# /health commit hash — built, tested, NOT deployed

Brandon: "BUILD /health commit hash" (the parked polish in `GROK_SIDECAR_REVIEW.md` / `GROK_NETMINT_LIVE.md`; no doc gave a
format, so the shape below is my choice: veto if wrong). **Daemon change** (a rebuild and restart are needed to go live), so it
needs the literal `deploy`; nothing was deployed and the droplet was not contacted.

## What `GET /health` says now

`{"status":"ok","commit":"<sha>[+dirty]|unknown"}`: the commit the RUNNING binary was built from. `status` is unchanged (nothing
parses the body today: tests, `verify.sh` and the docs only look at the 200), so this is additive. It stays loopback-only
(`nginx-commons.conf` does not proxy `/health`); a commit hash is not secret, but nothing here publishes it.

## The one design constraint, and how it is met

The droplet is **not a git clone**, and deploys build with **`cmake --build` and no reconfigure**. A value baked in at CMake
*configure* time would therefore go stale on exactly the deploys where it matters. So:

- `cmake/gen_build_info.cmake` is run by a custom **target** (`commons_build_info`, no OUTPUT, so it runs on EVERY build) and
  writes `build_info.h`. Only `src/buildinfo.cpp` includes it, so a new revision recompiles ONE file (a 512MB box builds with
  `-j1`) and an unchanged revision recompiles nothing (the header is rewritten only when its content changes,
  `copy_if_different`).
- Where the revision comes from, first match wins: (1) `COMMONS_BUILD_REV` in the environment (an explicit override, for
  tests); (2) **`<source dir>/.deployed-rev`, first word**: the marker `deploy/sync-to-droplet.sh` already writes on the droplet
  BEFORE the build, so on the droplet this is the deployed commit (`<sha>` or `<sha>+dirty`); (3) `git rev-parse HEAD` of the
  checkout, plus `+dirty` if a tracked file under `commons-daemon/` differs from HEAD (dev and CI); (4) `unknown`.
- **Only a sha-shaped token ever gets through** (7-40 lowercase hex, optionally `+dirty`); anything else is skipped, because the
  value ends up inside a C string and a JSON string. Tested with quotes, `;`, a backslash, uppercase, too short/long, empty.

## `verify.sh` now uses it

New check on the droplet: **the running daemon's `/health` commit must equal the deployed revision in `.deployed-rev`.** It
catches "source synced but the binary is stale" (not rebuilt) and "rebuilt but not restarted". Compared only when both sides
exist: with no marker, or a daemon that predates this feature (no `commit` field), it prints a note and does not fail, so
running `verify.sh` against today's daemon is unaffected. That is a behaviour change to a deploy script (flagged, not silent).
`DEPLOY_SKIP_REV_CHECK=1` skips it (the fake-droplet tests use it).

## Deploy notes (when Brandon says `deploy`)

`CMakeLists.txt` changed and there is a new source file, so `deploy-daemon.sh` will plan and run a **reconfigure** (its existing
rule: CMake changed since the live revision), which re-runs FetchContent against sqlite.org (it worked on the droplet before).
Order is already right: the sync writes the marker, THEN the build reads it, THEN the restart, THEN `verify.sh` compares `/health`
with the marker. Restart drops RAM presence and guest credentials as always. First `--go` on the droplet: the daemon
still reports no commit until it is rebuilt, which is exactly the run that fixes that.

## Tests (in CI via `run_all.sh`)

`tests/test_health.sh` (**57 assertions**): the CMake wiring (custom target, dependency, only `buildinfo.cpp` includes the
header); the generator's rules (each source, the priority order, git clean / modified / untracked-only / change outside
`commons-daemon/`, the marker beating git, the override beating both, 12 hostile or malformed tokens rejected, the header
rewritten only on change); **the real wiring on the real build dir**: with a different revision, `cmake --build` (no
reconfigure) recompiles `buildinfo.cpp` and NOTHING else, relinks, and the running binary reports the new revision; back again;
and an unchanged revision recompiles and relinks nothing; and the endpoint from the real binary (exact body, JSON content type,
`status` still ok, equal to the generated header, and in CI exactly `git rev-parse HEAD`). `test_deploy.sh` (**131**, +10):
`verify.sh` passes when the marker equals `/health`'s commit, FAILS on a stale binary (naming both revisions), fails on
`+dirty` vs clean, and does not fail with no marker. **Mutation-checked, 8 breakages:** no sha validation, `+dirty` never
set, override ignored, header always rewritten, git before the marker, a non-custom target, `verify.sh` accepting a stale
binary, and `/health` without the commit (a real recompile): each fails the tests.

## Limits, said plainly

- **A checkout with a `.deployed-rev` file beside it reports the marker, not git** (by design: on the droplet the marker IS the
  deploy). Nothing creates that file in a dev checkout.
- `+dirty` looks at tracked files under `commons-daemon/` only; untracked scratch files and changes elsewhere in the repo
  do not count.
- The first `verify.sh` after this feature deploys is the first run of the revision check against the real droplet.
