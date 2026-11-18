# Question — BUILD index.html split: how, exactly? (no code written)

Brandon said **BUILD index.html split**. The only specs in the docs are "`commons/` sibling, one vanilla file, no React,
no npm, no C++" (`grok_p0_review.md`) and "one vanilla HTML file until Brandon picks a next slice" (`wasm.md`), plus Grok's
repeated "do not split `index.html`" while other work shipped. Nothing says *how*. This is a refactor of the whole client
that visitors load on the live site, with several ways to break it silently, so I stopped before touching it. One answer
unblocks the build.

## Facts (measured on `commons/index.html` at `59eb784`)

- **103 KB, 2,243 lines**: 90 lines of inline CSS, 20 of HTML, **2,123 of JS in one `<script>`**, one `"use strict"` (line 119).
- **Everything is shared global state**: 106 top-level `var`, 68 top-level `function`, 0 top-level `let`/`const`, no
  `import`/`export`, no inline `on*=` handlers, 4 IIFEs. `player` is referenced 75 times, `mode` 33, `crop` 25, `netReady` 21,
  across the whole file. Sections call each other freely.
- **Order matters**: code runs at load time and depends on what came earlier: IIFEs at lines 403 (`bakeTileIds`), 848
  (`paintSpriteAtlas`), 895 (`loadSheets`), 972 (`assertSpawnIsFree`); `paintBake()` at 665; `applyCrop()` at 953; event
  listeners; and the name prompt at the very end starts the game.
- **What else touches `index.html`**: `tests/netmint_unit.js`/`test_netmint.sh` extract the `netMint:begin`..`netMint:end` block from
  it; CI gate 5 matches the exact path `^commons/index.html$`; `sync-client-to-droplet.sh` rsyncs the whole `commons/` directory
  (so new files ship) but never deletes; nginx serves the directory with `Cache-Control: no-cache`; `tiles.png?v=1` is hand-versioned.
- Natural cut lines already exist as `// ---- Section ----` headers (about 535 lines world/tiles/bake, 169 interiors, 91 sprites,
  159 camera/player/passerby, 575 networking incl. `netMint`, 185 input+interact, 394 main loop + name prompt).

## The decisions

### 1. Mechanism (the big one)

| | What | Cost / risk |
|---|---|---|
| **A. Classic `<script src>` files, same global scope** (recommended) | Cut along the existing headers; `index.html` becomes a shell that loads `style.css` and the scripts **in the current order**. | Behaviour is identical by construction (same code, same order, same globals). Works over `file://` and http, no build step, no rewrite. Needs `"use strict"` at the top of EVERY file (see 4). |
| **B. ES modules** (`import`/`export`) | Real encapsulation. | 106 vars + 68 functions of shared mutable state means rewriting most of the file's references and untangling cycles; high regression risk for no player-visible gain; modules do not load over `file://`. |
| **C. Only extract the CSS and the networking block** | 3 files, minimal. | Safe, but leaves a 1,500-line script; it is a partial split. |
| **D. Don't split** | | Fine, if the cost/risk is judged not worth it. |

### 2. Cut lines, if A (proposal, in load order)

`style.css` (90 lines) · `js/world.js` (constants, tile ids, painter, tileset, tile arrays, bake: ~535) · `js/interiors.js`
(~169) · `js/sprites.js` (~91) · `js/actors.js` (camera/crop, player, passerby: ~159) · `js/net.js` (presence networking
+ the `netMint` block: ~575, keeps the test markers) · `js/input.js` (keyboard + d-pad: ~55) · `js/interact.js` (~130) ·
`js/main.js` (main loop + name prompt: ~394). `index.html` keeps the 20 lines of markup and 9 `<link>`/`<script>` tags. Layout:
`commons/style.css`, `commons/js/*.js`, images unchanged.

### 3. Not a choice, but a trap: `"use strict"` is per file

Today one `"use strict"` covers the whole script. Split into separate scripts, it would cover only the first file and the rest
would silently run sloppy (an assignment to an undeclared name creates a global instead of throwing). Fix: `"use strict";` as the
first line of every JS file.

### 4. Deploy atomicity

A multi-file page is not swapped atomically: a visitor loading during the sync could get the new `index.html` with an old script.
Options: **(i) two-phase client sync: assets and scripts first, `index.html` last** (recommended: a small change to
`sync-client-to-droplet.sh` plus a test; `no-cache` already makes browsers revalidate every file); (ii) do nothing (a window of
seconds); (iii) content-hashed file names (that is a build step; rejected). `rsync` never deletes, so a renamed/removed file would
linger on the droplet, harmless but worth knowing.

### 5. How I would prove "no behaviour change" (the part that matters)

1. **Byte-equivalence**: concatenating the JS files in load order equals the original `<script>` body, except for the added
   `"use strict";` lines and the file separators. Checked mechanically and recorded in the commit, so no code line changed.
2. `node --check` on every file.
3. A permanent CI test (`test_client_files.sh`): every `<script src>` exists and is loaded once, in the documented order; no JS
   file is unreferenced; each begins with `"use strict"`; the `netMint` block is present (the netMint tests are repointed at
   `js/net.js`); no missing file can slip in (a 404 there is a blank game).
4. **Real-browser parity, before and after**: the old and new page against a real daemon: same canvas pixel hash after the bake,
   a scripted walk, a real mint + join.
5. After a deploy: the page loads, a mint and join work, the served files equal the repo's.

### 6. Also changes

CI gate 5 becomes `^commons/` (a civic change plus any client file still needs "civic" or "deploy" in the subject); the netMint
tests' paths; docs and the README; `deploy-scripts` docs mention the two-phase sync. No daemon, civic or nginx change. A deploy
is client-only and needs the literal `deploy` (`drift-check.sh --tree client`, then the sync).

## Recommendation

**A, with the 9-file cut above, the per-file `"use strict"`, the two-phase sync, and the proof in 5.** It is the only option that
changes structure without changing a single line of game code, and every step is mechanically checkable. B is a rewrite, C is
half a job, D is also legitimate if this is only tidiness.

**Reply with one letter: A (whole recommendation) / B / C / D, or A with named changes** (e.g. different file grouping, or
skip the two-phase sync). I build on that.
