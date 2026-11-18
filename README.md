# LiquidGov.US

The civic platform behind https://liquidgov.us: a C++ web server, its page scripts, and the tools that build, test
and deploy it. This file is the map: what each folder is for, and where a new file goes.

## Layout

| Folder | What it holds | A new file of this kind goes here |
|---|---|---|
| `liquidgov-platform/` | The site program that runs on the server | |
| `liquidgov-platform/src/site/` | The site's code, one folder per feature (`features/<feature>/`), shared code in `core/` and `ui/` | a route, a page, a feature |
| `liquidgov-platform/libs/` | Libraries the site is built from (store, accounts, mail, images, ...) | a reusable library |
| `liquidgov-platform/assets/` | Page scripts, styles, icons and media, served as static files | JavaScript, CSS, images |
| `liquidgov-platform/tests/` | Unit tests (`*_test.cpp`), end-to-end suites (`e2e/*.sh`), browser checks (`browser/*_check.py`), undo drills (`rollback/`) | a test for the site |
| `liquidgov-core/` | The governance core library (delegation, identity, land, judgment), with its own tests; not part of the site program | core civic logic |
| `extension/` | The browser extension and its check | extension code |
| `deploy/` | The deploy tooling (`sync-to-droplet.sh`, `make-go.py`, `ci-gate.sh`), server config (nginx, systemd), undo scripts, and one generated `<name>-go.sh` / `<name>-remote.sh` pair per deploy | deploy tooling or server config |
| `scripts/` | Developer tools: local checks (`lgcheck.sh`, `local_ci.py`), the affected-check runner, worktree tidy, release build | a tool run on a developer machine |
| `tests/repo/` | Checks on the repo and its tooling: naming, deploy gate, store-format consistency, git hooks | a check about the repo itself |
| `.githooks/` | The repo's git hooks (pre-commit, commit-msg, pre-push) | |
| `docs/` | Design notes; `docs/archive/` holds finished or retired work (`docs/archive/the_commons/` is the retired game) | a design note or decision record |
| `Persistence/` | `notes.md`, the shared, dated decision log (newest first under each heading) | a dated decision bullet, not a file |

## Naming

`naming.toml` is the standard and `tests/repo/naming_check.py` enforces it on every commit:

- code, tests and docs: lowercase `snake_case` (`feed_routes.cpp`, `unreachable_routes.sh`, `spatial_evolution.md`);
  `README.md` is the one exception
- deploy scripts: lowercase words joined by hyphens (`assets1-go.sh`), because the deploy tooling matches that pattern
- branches: `wip/<number>-<words-with-hyphens>`
- dates in text and file names: `YYYY-MM-DD`
- no spaces anywhere in a path

## Where files do NOT go

Scratch work, logs, builds and one-off outputs stay out of this repo. They go in
`C:\Users\Brandon\scratch\<YYYY-MM-DD>-<agent>-<task>\` (builds in WSL `~/lg-<name>-check`).
