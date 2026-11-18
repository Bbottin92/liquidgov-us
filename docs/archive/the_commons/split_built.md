# index.html split — built, proven, DEPLOYED 2026-09-19 ~18:17Z

Brandon: "BUILD A. Prove load order in CI. Two-phase sync. Do not deploy." (`claude_split_question.md`, answered by
`grok_split_answer.md`: A, classic `<script src>` files in one global scope; no modules, no `let`, no rename, no
`defer`/`async`; cut on the existing headers.) **Client only.** Deployed afterwards (see "Deployed" at the bottom); the daemon was not restarted.

## What it is

`commons/index.html` (2,244 lines, 103 KB) is now a **42-line shell**: the markup, one `<link rel="stylesheet" href="style.css">`,
and eight plain `<script src>` tags, in this order (a comment in the page says why the order matters):

`js/world.js` (549 lines: constants, tile ids, painter, tileset, tile arrays, bake) · `js/interiors.js` (170) ·
`js/sprites.js` (92) · `js/actors.js` (160: camera/crop, player, passerby) · `js/net.js` (576: presence networking incl. the
`netMint` block and its test markers) · `js/input.js` (56) · `js/interact.js` (131) · `js/main.js` (395: main loop + name prompt).
Plus `style.css` (the old inline `<style>`, 90 lines). Every JS file starts with `"use strict";` (strict mode is per script:
the old single directive would have covered only the first file). No code line was changed, added or moved.

## The proof (this is the part that matters)

1. **Byte-equivalence, mechanical:** `style.css` equals the old `<style>` content, and the eight JS files with their added
   `"use strict";` lines removed join back to the old script body **exactly**. (Run once, against the committed pre-split page at
   `59eb784`/`HEAD`; it is deliberately NOT a permanent test, because the next legitimate client edit would break it.)
2. **Hoisting, not just concatenation.** Today one `<script>` hoists every function to the top of the whole file; after the split
   a file can only see what already loaded. `tests/client_files_unit.js` (48 checks) loads the REAL files **in tag order**, each as
   its own classic script in one shared global scope under a stubbed DOM, and fails on any `ReferenceError`. It also checks that the
   load-time code really RAN, by its effects (a silent skip would look like success): `bakeTileIds` (the Creek is water), `paintBake`
   (the `baked` canvas got at least one draw per world cell: 5,760), `paintSpriteAtlas`, `loadSheets` (both sheets requested),
   `applyCrop` (the `#game` canvas sized 240x160), `assertSpawnIsFree` (no error), plus the key functions exist. **Controls:** three
   wrong load orders and a missing file must fail, so the check can fail. **No cut needed moving.**
3. **Structure:** exactly the eight tags in the documented order, no inline script or style left, no `defer`/`async`/`type=module`,
   every file referenced and present, each starts with `"use strict";`, no top-level name declared in two files (the original had
   none), the scripts come after the markup they use.
4. **Mutation-checked, 11 breakages** of the client (two tags swapped, a lost `"use strict"`, `defer`, a top-level use of a name
   from a later file, a missing file, an unreferenced file, a duplicate declaration, an inline script, and the bake / `applyCrop` /
   `loadSheets` / sprite atlas never running): each fails the test. Two of them (the bake and `applyCrop`) first slipped past a weak
   version of the test (any canvas draw counted; `crop` has a default), and the effect checks were tightened until both were caught.
5. **Real browser, old vs new, same daemon:** the committed inline page (port 8091) against the split page (8090). Identical:
   227 globals (same name hash), the source text of all 68 functions (same hash), the value of all 159 non-function globals (same
   hash), the baked world (1152x1280, same pixel hash) and the sprite atlas (same pixel hash), the applied CSS, the title. Only the
   intended difference: 1 inline script vs 8 external and 0 inline. Then entering the game on both: overlay dismissed, mint OK, joined
   and listed by the daemon, same spawn (576, 480), same crop, no uncaught errors, console clean; every file loads 200, JS as
   `text/javascript`, CSS as `text/css`, byte sizes equal the repo's. The two main-canvas hashes differ (each frame contains the
   wandering passerby and, with two tabs on one daemon, the other tab's guest), so the deterministic layers and state were compared
   instead of claiming pixel parity for a live frame.

## Two-phase client sync (`deploy/lib.sh`, `sync-client-to-droplet.sh`)

A multi-file page cannot be swapped atomically. The client now goes in TWO PHASES: everything except `index.html` first
(`--exclude /index.html`: scripts, stylesheet, images), `index.html` LAST. A visitor never loads a NEW `index.html` that points at
scripts the droplet does not have yet; and while phase 1 runs, the old self-contained page keeps working. If phase 1 fails,
`index.html` is untouched and the `.deployed-client-rev` marker is not written. The daemon sync is unchanged. `rsync` still never
deletes, so a removed or renamed file would linger on the droplet (harmless). `test_deploy.sh` (148, +17): exactly two rsync runs, phase 1
first and excluding `index.html`, phase 2 last and `index.html` alone; on a second fake droplet a failing phase 1 fails the sync, no
`index.html` lands and no marker is written; a dry run shows both phases and transfers nothing; the real run lands everything and writes
the marker. Mutation-checked (3 breakages: `index.html` not excluded from phase 1, the phases swapped, a failed phase 1 ignored).

## Also changed

`tests/test_netmint.sh` and `netmint_unit.js` now read `commons/js/net.js` (88 unit checks unchanged; the two-call-site check
counts across `commons/js/*.js`). CI gate 5 now matches any `commons/` file, not just `commons/index.html`: a civic change
plus any client file in one commit needs "civic" or "deploy" in the subject. New test `test_client_files.sh` runs in CI (node
is there; a missing node in CI is a failure). No daemon, civic or nginx change; nginx already maps `.css` and `.js` (checked on the box).

## Deploy (when Brandon says `deploy`, after review): client only, two-word protocol as before

Step 1 read-only: `deploy-daemon.sh` is NOT the tool; use `drift-check.sh --tree client` (the marker `.deployed-client-rev` exists,
so no `--baseline`): it will list `index.html` (changed) plus `style.css` and `js/*.js` (new), and nothing else. Step 2:
`sync-client-to-droplet.sh` (two phases, then the chown and the marker). Then verify: `/commons/` serves the shell, every
script and the stylesheet answer 200 with the right MIME types, the served bytes equal the repo's, `verify.sh` passes, a mint and
join work from a real browser, and the daemon uptime is unchanged (no restart). Visitors mid-session keep the old inline page until
they reload (`Cache-Control: no-cache` is already set). **Rollback:** re-sync the previous `index.html` (it is self-contained): the
new files can stay, nothing references them.

## Limits, said plainly

- CI runs the load-order test under a stubbed DOM in node, not in a real browser: the stub proves order and effects, the manual
  real-browser comparison above proves rendering, and the CI test cannot see a rendering regression.
- `"use strict"` per file, the tag order and the file names are pinned by the test on purpose: a future rename or reorder must
  change the test in the same commit.
- Not done, by instruction: turning globals into modules, `let`/`const` at the top level, merging or renaming files.

## Deployed (2026-09-19 ~18:17Z, Brandon: `deploy` twice after review; CI green on `bf377c7`)

Client only: no daemon, civic or nginx change. **Step 1 (read-only):** `drift-check.sh --tree client`: no drift (the droplet held exactly
what `b04970e` shipped) and exactly the ten expected files to change (`index.html`, `style.css`, the eight `js/` files). **Step 2** was
delayed by a Windows host error, not by anything on the droplet: WSL could not start (`0x800705aa`, insufficient system resources, host
memory pressure) for a few attempts, so the sync script never ran and nothing reached the droplet; once WSL came back, the script
re-ran the read-only drift check first (it stops unless the plan is unchanged: it was) and then `sync-client-to-droplet.sh` sent the
client in TWO PHASES (phase 1: scripts, stylesheet, images; phase 2: `index.html` last), chowned it, and wrote the
`.deployed-client-rev` marker (`cb982a4`, kept outside the served directory: a 404 over https).

Verified: every one of the twelve client files is byte-identical to the repo's on disk AND as served through https (`index.html` 200
`text/html`, `style.css` 200 `text/css`, the eight scripts 200 `application/javascript`; nginx maps `.js` to that, the local dev server
sends `text/javascript`, both fine); `Cache-Control: no-cache`; the live page has 0 inline script blocks and 8 external script tags
(`/commons/js/world.js` was a 404 before); the daemon was NOT restarted (`ActiveEnterTimestamp` 17:23:28 UTC and `NRestarts=0`, before
and after; the running daemon is still `d9363b5d7aa5`); `verify.sh --guest-name x` **23 passed, 0 failed**; and a real browser on the LIVE
page: 8 scripts, tiles loaded, entered as `x`, minted, joined and listed by the daemon at the same spawn (576, 480), console clean,
screenshot showing the game rendering. Visitors mid-session keep the old inline page until they reload.

**Rollback (not needed):** re-sync the previous `index.html` (`b04970e`, self-contained); the new files can stay.
