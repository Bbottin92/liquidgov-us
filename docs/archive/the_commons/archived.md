# The Commons is archived (2026-11-15)

The game (the daemon, its web client and its tests) was removed from this repo on 2026-11-15. It was live on the
droplet but unused: one player (Brandon), no visitors in the logs, nothing on the site linked to it, and the public
`/commons/` address only ever showed a static "The Commons is closed" notice (`deploy/commons-closed.html`), which
stays as a tombstone for old bookmarks (`/world` still redirects there).

What is in this folder: the design notes only. Nothing here is built from.

Getting the code back:

- the last commit that had it: branch `archive/commons-final` (and the local tag `commons-final`) on
  `Bbottin92/liquidgov-us`, at master `a006386`: `commons-daemon/` (SQLite ledger, presence, catalog, deploy scripts),
  `commons/` (the web client), `.github/workflows/commons.yml`, and the site's four `/internal/*` routes and
  `/canvas-frame/<user>/latest` in `liquidgov-platform/`
- `git checkout archive/commons-final -- commons-daemon commons` brings the folders back into a working tree

What was kept because the rest of the repo uses it: the repo's own tooling tests, which lived in
`commons-daemon/tests/`, are now in `tests/repo/` (ship, the deploy gate, naming, the git-hook checks, make-go and
the rest), run by `scripts/run_hook_checks.py` and `tests/repo/run_all.sh`.

The decision and the evidence are in `Persistence/notes.md` (2026-11-15, "The Commons ... found live but unused").
