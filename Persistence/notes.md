# Notes — ideas, decisions, parked work

Last updated: 2026-09-16

Shared working memory for **ideas and product decisions** (Grok + Claude).
Repo path: `Persistence/notes.md` on `Bbottin92/liquidgov-us` `master`.

Not bug classes. Not “what is live on the droplet.”

**How this file is used**
- Session start (both): read this file.
- During a turn: if Brandon decides, rejects, or parks something, append a dated
  entry *this turn*. Do not wait for “task done.”
- One heading per topic. Newest dated bullet at the top of that heading.
- Durable facts only. No transcript dumps. If it can be grepped from git, skip it.

---

## Shared pad

- **2026-09-16 — decided.** GitHub shares `Persistence/notes.md` (+ README) only.
  `context.md` and `state.md` exist in the *old* Grok sandbox and were never
  pushed. Do not invent fakes. Do not have Claude create them. A fresh Grok
  hard-gate that requires those two files will fail until we drop that read or
  Claude commits the *real* files from that sandbox. “Have Claude git pull”
  updates notes/README only.

## Game — atlas locked, no code until BUILD

- **2026-09-16 — P0 closed.** Patch 0957725 accepted: name field types
  "Brandon", rAF is the loop, interval only as watchdog, alpha:false,
  d-pad tabindex=-1, input.blur() on enter. Civic untouched. P1 not
  started. Parked (not a new patch): watchdog should no-op while
  `document.hidden` so a background tab doesn't start the 60Hz fallback.
  Next work waits on Brandon saying P1 / BUILD.
- **2026-09-16 — P0 patch shipped.** All three items from
  `CLAUDE_P0_PATCH.md`/`GROK_P0_REVIEW.md` fixed in `commons/index.html`:
  (1) keydown/keyup now ignore entirely (no `preventDefault`, no `keys[]`
  update) while `#nameOverlay` is visible or `document.activeElement`/
  `e.target` is an INPUT/TEXTAREA — typed "Brandon" via real keyboard
  input end-to-end and confirmed `nameInput.value === "Brandon"`, then
  confirmed movement worked immediately after submit (added `input.blur()`
  in `enter()` so the now-hidden input doesn't keep eating keys via stale
  focus). (2) `requestAnimationFrame` is the primary loop again
  (`rafLoop`); a `watchdogTick` every 300ms only starts a `setInterval`
  fallback if `lastTickAt` has gone stale, and tears it down if rAF
  resumes — confirmed rAF firing normally in this session's browser pane
  this time (72 callbacks/500ms measured directly) with `usingFallback`
  correctly false throughout. (3) `{alpha:false}` added on both the bake
  and display 2D contexts, `imageSmoothingEnabled=false` added on the bake
  context too (was only on display before), `tabindex="-1"` added to all
  four d-pad buttons. Re-verified after patching: A/D still not inverted,
  dual crop still switches at 390×844, water collision still blocks
  off-path movement, no console errors. Did not touch checker grass,
  Creek/Shore flood-fill, or start P1, per explicit instruction.
  `liquidgov-platform/` untouched.
- **2026-09-16 — Grok P0 review.** Skeleton accepted (bake/blit, dual crop,
  A/D, path ribbon, labels, civic untouched). Must-fix: name input vs WASD
  `preventDefault` ("Brandon" untypeable). Should-fix: rAF as the product
  loop, setInterval only as rAF-fallback; `alpha:false`. P1 not started.
  Paste `CLAUDE_P0_PATCH.md`. See `GROK_P0_REVIEW.md`.
- **2026-09-16 — blit pipeline.** `tilemap.md` expanded: display canvas
  is the logical crop only (not the 1152×1280 bake in the DOM). Floor
  camera, smoothing off, alpha:false, ignore DPR, no worker. Giant-canvas
  + CSS-translate is the fail.
- **2026-09-16 — P0 shipped.** `commons/index.html`: vanilla HTML/canvas,
  one file, no build step. Uint8Array(72×80) tile+collision, baked once to
  an offscreen canvas per `tilemap.md`, blitted per tick — no per-frame
  72×80 loop. Dual crop (240×160 @3× landscape / 160×240 @2× portrait,
  picked by `width >= height`) confirmed switching correctly at a real
  390×844 phone size. A/D and arrows confirmed not inverted (moved player
  in the correct direction, tested via dispatched key events, not just
  read from code). On-screen d-pad confirmed via real pointer events on
  the button elements. Water (Creek/Shore) confirmed solid; the
  Square→Homestead→Creek→Shore path ribbon confirmed walkable straight
  through that same water band. World edge confirmed blocking. All 12
  region names confirmed present and in the correct grid order as small
  world-space ink labels — no HUD chrome. No fountain, no other set piece,
  no doors, no passerby, no SQLite, no cookie, per P0 scope. One
  implementation deviation from `tilemap.md`'s default suggestion: the
  tick loop uses `setInterval`, not `requestAnimationFrame` — a bare rAF
  counter measured 0 callbacks over 500ms in the tool's own browser pane
  even fully visible/focused, so rAF isn't reliable in every environment
  this needs to run in; `setInterval` has no such dependency and the
  movement math was already delta-time based, not frame-count based, so
  nothing else changes. `liquidgov-platform/`, `liquidgov-core/`, LGP1
  untouched. Grok reviews next.
- **2026-09-16 — tilemap notes for P0.** `Persistence/TheCommons/tilemap.md`:
  one Uint8Array 72×80, bake supermap once, blit camera crop. No chunks,
  no WebGL, no 12 canvases. Claude should follow this in `commons/`.
- **2026-09-16 — P0 BUILD assigned to Claude.** Brandon: Grok guides /
  criticizes; Claude implements to save Grok tokens. Paste `CLAUDE_P0.md`.
  Sibling folder `commons/` (vanilla HTML/canvas), not `liquidgov_http`,
  not React. P0 only. Grok reviews after the commit. Civic frozen.
- **2026-09-16 — first BUILD = P0 only.** Brandon chose P0, not P0+P1.
  Empty walkable 72×80, dual crop, d-pad, A/D = left/right, throwaway name,
  spawn Square, region labels, no set pieces, no doors, no passerby.
  P1+ wait for a later BUILD. Still no code until Brandon says BUILD.
- **2026-09-16 — Grok build order.** Answer in
  `Persistence/TheCommons/BUILD_ORDER.md`. Greater map = empty walkable
  72×80 first (one array, not 12 maps). Chunk = a **system**, then a
  **spine** of adjacent regions, not one region. Dual crop + A/D + d-pad
  de-risked in P0. Passerby in P1 after the spine walks, before interiors.
  No V1 SQLite/P2P/glue. Recommended first BUILD: **P0+P1**. P0-only is
  the smaller phone-check slice. Spec not reopened. Still waiting on
  Brandon for BUILD.
- **2026-09-16 — Question posed to Grok: build order.** Spec pack fully
  locked, no open bets. Brandon wants the actual build scoped top-down:
  greater map first, then chunks, then smaller shippable pieces, so BUILD
  doesn't turn into one big-bang implementation. Asked Grok directly (he's
  the one building the client) in
  `Persistence/TheCommons/BUILD_ORDER_QUESTION.md` — what order he'd
  actually build in, what a "chunk" means in his build, where presence/
  `passerby` slots in, and what (if anything) he'd de-risk first. Not a new
  pushback round; spec isn't reopened. Still waiting on Brandon for BUILD.
- **2026-09-16 — Grok accepts passerby.** Ambient walker is locked as a
  **V1-only crutch**: nameplate exactly `passerby` (no unlabeled variant),
  `is_ambient` internally, hide while a real guest is in that region,
  Square → Homestead → Creek → Shore, trivial to delete later. Claude's
  round-2 rewrite stands. No remaining open bets. No game code. Civic
  frozen. BUILD still waits for Brandon.
- **2026-09-16 — Claude pushback round 2, ambient walker.** Grok's four spec
  patches (Square/Lab Yard neighbor fix, camera-overlap presence subscription,
  cookie-scoped friends query, `players` last-position columns) are correct,
  adopted, no further pushback. On Grok's new bet (an ambient walker so a
  solo preview isn't an empty Square): **rewrite, not accept-as-is** — ship
  only the nameplate-reading-"passerby" variant, reject the no-nameplate
  variant, because a real occupant always carries a name label
  (`art.md`) and an unlabeled look-alike would be indistinguishable from an
  actual second visitor — the opposite of this project's honesty pattern
  (boarded mine, closed shutter). Written up in `pushback.md` (Round 2) and
  `v1.md` (Pending, not yet locked). Not locked — awaiting Brandon/Grok to
  close it out. No game code. Civic frozen. BUILD still waits for Brandon.
- **2026-09-16 — Grok accepts Claude’s three overrides.** Now locked as
  implementation: regions **24×20** (supermap 72×80); camera **dual crop**
  240×160 @3× landscape / 160×240 @2× portrait (pick by viewport aspect);
  lab recipe #3 **chalk only, no lime**. Grok also patched (see
  `Persistence/TheCommons/GROK_ANSWER.md`): Square south = Homestead Row
  (atlas typo said Lab Yard); presence clients subscribe to
  camera-overlapping regions; friends query is cookie-scoped (no client
  username param); `players` last-position columns. Ambient walker on
  Square→Shore is a Grok *bet*, not locked. No game code. Civic frozen.
  BUILD still waits for Brandon.
- **2026-09-16 — Claude spec pack landed.** Wrote all seven files in
  `Persistence/TheCommons/`: `atlas.md`, `schema.sql`, `presence.md`,
  `art.md`, `glue.md`, `v1.md`, `pushback.md`. No lock changed — everything
  pinned in this heading (title, region names/order, 3×4 layout, Homestead
  gate, Hall/mine V1 scope) is unchanged. Two of Grok's *unlocked*
  implementation bets in `GROK_BET.md` are overridden in `pushback.md`, not
  silently: region grid 32×32→**24×20** (Grok's own bet flagged this risk
  and offered this exact fallback), and camera flat 240×160→**dual
  landscape/portrait crop** (240×160 desktop, 160×240 phone) so the phone
  case isn't letterboxed/non-integer-scaled. Lab recipe #3 also rewritten
  (drop "lime," keep only "chalk," per the safety-boundary reasoning in
  `pushback.md`) — the other two lookups, the palette, the Homestead cap,
  the Hall/mine V1 scope, and A/D=left/right are all accepted as Grok bet
  them. Ready for Grok to read `pushback.md` and answer.
- **2026-09-16 — Claude spec pack (no game code, civic frozen).** Paste
  `CLAUDE_THE_COMMONS.md` into Claude Code (not `CLAUDE_CODE_CONTINUE.md`).
  He fills `Persistence/TheCommons/` (atlas, schema, presence, art, glue, v1,
  pushback) from Grok bets in `GROK_BET.md`. V1 client stays the Grok preview
  after Brandon says BUILD. Droplet sibling later. Do not touch
  `liquidgov_http` / LGP1.
- **2026-09-16 — locked.** A + continuous outdoor + warp interiors + homestead
  gate. Title **The Commons**. Plaza region **the Square** (not “Commons”).
  Art: GBA / FireRed tiles. No code until Brandon says BUILD.
  - Outdoor 3×4, north up:
    North Woods | Lookout | Orchard
    Workshop Lane | **the Square** | Hall Lawn
    Lab Yard | Homestead Row | Garden
    Quarry Ridge | Creek | Shore
  - Interiors extra: per-player house, lab, hall (thin), mine (boarded / not present).
  - Homestead Row = gate (online / friends doors). Houses are instances, not 12 lots.
  - V1: all 12 lightly dressed; mine boarded; no workshop, crafting, farming,
    swimming, or XP. Lab output is placeable / wall-able, not “salt in a bag.”
  - **V1 runs in the Grok preview app.** Droplet sibling + SQLite later.
    `liquidgov_http` is never the game.
  - Presence: per-region at ~2 Hz even on one canvas. Droplet is 512 MB / 1 vCPU;
    do not flood.
  - Guests walk the 12; house binds only after LiquidGov cookie; no second
    password. Preview may use a throwaway name until glue exists.
  - Profile canvas from the site = one **wall slot**, not the floor.
  - Friends = civic **query**, short RAM TTL, never a second graph.
    Visit: open / friends / locked.
  - Photo → civic blob + feed only. Game does not grow posts, comments, or follows.
  - Touch: on-screen d-pad; A/D = left/right on screen.
- **2026-09-16 — parked, atlas not locked.** (superseded by lock above)
  Working title The Commons (not confirmed). Art lean GBA / FireRed cousin
  (not confirmed). Nothing built. Do not scaffold, sprite, or write game code
  until Brandon says BUILD. Do not start the game in the civic repo.
- **V1 proposed, not locked:** (superseded) walkable public ground, your house,
  lab, Hall door → live `liquidgov.us/proposals`, mine boarded. Guest play
  until cookie glue exists.
- **Atlas not locked.** (superseded) Brandon wants one greater map of about 12
  minor maps or instances. Options A/B and warp vs continuous were the choice;
  lock is A + continuous.

## Split: civic site vs game

- **2026-09-16 — decided.** Persistent FireRed/Sims/AC-style world is **not**
  grown inside `liquidgov_http` / LGP1. Two programs, one person.
  - LiquidGov keeps: login, friends, mail, votes, profile canvas, Juketube, civic pages.
  - Game keeps: tiles, positions, houses, inventory, lab, mining, visiting.
    Own process, own files. A game write must not take down Genesis.
  - Glue only: session cookie, friends as a *query* (not a copied graph),
    `/proposals` as an in-world door, blob+feed for photo mode. Game stores
    `lg_username` as a reference, never a second login.
  - Juketube-shaped hangout on this droplet (one screen, ~2 Hz, RAM presence)
    is a waiting room, not the concept. Don’t sell it as Animal Crossing.
- Chemistry: real *names and reaction classes* as a lookup table is fine.
  Real stoichiometry/safety is a different product. Mining without heat/grade/alloy
  is named fantasy tiers. Neither is a reason to pick this stack.
- Profile canvas = bitmap to hang on a wall. House layout =
  `{item, x, y, rot90}` rows. Don’t reuse the canvas as the house.
- **Do not start the game in this repo until Brandon explicitly says so.**
  When it starts, it is a sibling process + SQLite, not new LGP1 tags.

## Data: what lives where

- **2026-09-16 — decided.** Normalize catalogs and ledgers. Don’t NoSQL the civic site.
  - LGP1 stays the civic store. It is a tagged dump, not a database.
    Do not add game tags. Do not migrate LiquidGov to Mongo “to modernize.”
  - Game ledger (inventory, placed objects, recipes, trades): SQLite,
    ~3NF catalogs / 2NF instances. Trades are `BEGIN/COMMIT`.
    LGP1 cannot roll back a half-write.
  - Hot presence (position, who’s in the plaza): memory (or Redis later).
    Snapshot on timer/leave, not 10 Hz flushes.
  - House snapshot JSON is a *cache* derived from `placed` rows, not the only copy.
  - NoSQL that *does* fit: key–value for live presence; a document for a house
    blob you always load whole. Not the inventory ledger. Not a second friend graph.
  - If a column can be derived from LiquidGov or from `items`, don’t store it as truth.

## Messages home-screen app

- **2026-09-16 — confirmed live.** Checked droplet: `src/http_server.cpp` md5
  matches the WSL working copy exactly, `liquidgov_http` binary built the same
  day, service active. Curled production: `/mail` renders via
  `page_wrap("Messages", ...)` with `mail_install_hint()` in its own body (not
  site-wide), `manifest-mail.webmanifest` serves `start_url:"/mail"` live, and
  `/app/mail` already carries the Messages-specific `apple-mobile-web-app-title`
  / icon / manifest. Nothing left to implement — this item is closed.
  - Original ask, for context: iOS “Add to Home Screen” captures the
    **current URL**, so `/apps` → `/app/mail` (install copy) → icon would open
    the install page, not `/mail`. Fix was to nudge the install hint from
    inside live `/mail` itself with page-specific manifest/icon/title, which
    is what's now confirmed running.
  - Already-saved wrong icons from before the fix cannot be patched; those
    users delete and re-add from `/mail`.

## Standing product pins (copy, don’t reopen)

- Claude is sole droplet implementer of `/opt/liquidgov`. Grok: GitHub + prompts, no SSH.
- `lg-deploy` rsync as root left `/opt/liquidgov` `root:root` → store write SIGABRT.
  Directory must be `liquidgov:liquidgov` after rsync. Never rsync `*.lgp`.
- Friends are mutual accept. No age checkbox. Phone is signup binding, not login.
- Platform community ≠ AVND locality. No React/Vite/Node UI.
- Do not put game world state in LGP1 or `http_server.cpp`.
