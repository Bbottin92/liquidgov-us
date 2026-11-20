# Contributing to LiquidGov.US

Thanks for wanting to help. This file says how to build the site, run its checks, and get a change merged.
`README.md` is the map of the repo: what each folder is for and where a new file goes.

## Build and run (Linux, or WSL on Windows)

You need `g++` with C++17, CMake 3.16 or newer, `pkg-config` and libsodium:

```sh
sudo apt-get update                                   # a fresh machine (e.g. a cloud session) has no package list yet
sudo apt-get install -y g++ cmake pkg-config libsodium-dev
cd liquidgov-platform
cmake --preset dev && cmake --build --preset dev     # builds into ../build/dev
../build/dev/liquidgov_http                           # then open http://127.0.0.1:8080
```

Presets (`liquidgov-platform/CMakePresets.json`): `dev` (debug), `release` (the deploy binary, static C++ runtime),
`asan` (address and undefined-behavior sanitizers). A build folder is reused, so later builds only recompile what
changed; install `ccache` and switching branches stays fast too.

The server keeps its data in `./liquidgov_accounts.lgp` and `./liquidgov_blobs/` unless `LIQUIDGOV_STORE` and
`LIQUIDGOV_BLOBS` say otherwise. Run it from `liquidgov-platform/` so it finds `assets/`.

## Run the checks

- One end-to-end suite (each starts its own throwaway server and store, on its own port):
  `cd liquidgov-platform && bash tests/e2e/<suite>.sh ../build/dev/liquidgov_http ../build/dev/liquidgov_e2e_seed`
- Everything the site's CI runs: `bash scripts/lgcheck.sh <liquidgov-platform dir> <build dir> <previous build dir>`
- The repo's own checks (naming, hooks, deploy tooling): `python3 scripts/run_hook_checks.py .`

Judge a suite by its own last lines (`passes=N fails=M`, `EXIT=0`), not only by its exit code.

Turn on the repo's git hooks once per clone: `git config core.hooksPath .githooks`. They run the checks your change
can reach before each commit and push.

## Making a change

1. Branch from `master` as `wip/<number>-<words-with-hyphens>` (for example `wip/152-privacy-heading`).
2. A new behavior comes with a test that fails before your change and passes after it. Say both in the pull request.
3. Follow `naming.toml` (lowercase `snake_case` for code, tests and docs; no spaces in paths). The hooks check it.
4. A commit that changes site code needs three lines in its message, each on its own line:
   - `Read:` what you read line by line, and what rests only on tests
   - `Attacker:` whether someone attacking the site could use the change (or `n/a: <why>`)
   - `Undo:` how to undo it, and whether it is tied to another change
5. Open a pull request against `master`. CI must pass and a maintainer must approve before it merges.

## Things that need extra care

- **Stored data.** The store is a text format (LGP1). A new record type or field must keep older programs able to read
  the store (see `deploy/rollback_guard.py` and `tests/repo/lgp1_consistency.py`). Never change an existing record's
  shape.
- **Privacy.** Anything already public may show by default; everything else is opt-in. Never log passwords, session ids
  or message bodies.
- **The live site** is deployed only by a maintainer, after review.

## License

LiquidGov.US is licensed under the GNU Affero General Public License v3.0 (`LICENSE`). By sending a pull request you
agree that your contribution is licensed under the same terms.
