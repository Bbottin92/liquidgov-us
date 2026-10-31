# Grok on CLAUDE_SPLIT_QUESTION.md — **A**

Classic `<script src>` files, same globals, no modules, no build step.
Cut on the existing section headers. Two-phase client sync. Per-file
`"use strict";`. Do not `defer` or `async` the tags (order must match
today's inline script).

## The hoist trap (must-prove, concat is not enough)

Today one `<script>` hoists **every** `function` to the top of the whole
file. After a split, a function only exists after its own file runs.
A top-level call in `world.js` to something declared in `main.js` works
now and **throws** after the cut.

Byte-equality of the concatenated sources does **not** prove that.
Add a load-order check: evaluate the files **in tag order** in node (or
the real browser) and fail on `ReferenceError`. The existing IIFEs and
`paintBake` / `applyCrop` / `loadSheets` / `assertSpawnIsFree` must still
run. If a call sits above its declaration across a cut, **move the cut**,
do not add a stub.

Keep the `netMint:begin` / `end` markers inside `js/net.js`. Repoint the
extract tests.

## Cut (load order)

`style.css` then `js/world.js` · `interiors.js` · `sprites.js` · `actors.js`
· `net.js` · `input.js` · `interact.js` · `main.js`.

Do not merge, do not rename to "better" modules, do not introduce `let`
at the top level as part of this BUILD. `index.html` is markup plus the
link and the eight tags.

## Deploy

Two-phase: all of `js/`, `style.css`, images first; `index.html` last.
`no-cache` stays. Client-only, literal **deploy** after review. CI gate 5
becomes `^commons/`.

B is a rewrite. C is half. D if Brandon would rather leave it.

Build A locally, prove hoist + concat + browser, **hold for review**.
Do not deploy from this answer.
