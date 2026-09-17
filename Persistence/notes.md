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

- **2026-09-16 — WebGPU is not this game.** `Persistence/TheCommons/webgpu.md`:
  240×160 blit does not want a compute shader. Canvas 2D stays. Sprites /
  glue / sibling still wait on Brandon.
- **2026-09-16 — WASM is not V1.** `Persistence/TheCommons/wasm.md`: no
  compile step in commons/. WASM only earns a slot if a C++ sibling
  daemon shares collision; blit/input stay JS. Sprites/glue/sibling still
  wait on Brandon.
- **2026-09-16 — nginx reverse proxy notes.** `Persistence/TheCommons/nginx.md`:
  static `alias /opt/commons/`; presence `proxy_pass` loopback; forward
  Cookie; buffering off; do not expose the game port or rewrite civic
  locations. Not a BUILD. Sibling still not this week.
- **2026-09-16 — two parked P3 nits closed.** Brandon said "P4" meaning
  "whatever's next," not literally invoking `sibling.md`'s deploy work --
  that doc itself says the sibling is "a new BUILD, not a P4," and "not
  this week," so held off on it rather than guess into a production
  change. Instead closed the two items already parked after P3 review, in
  `commons/index.html`: (1) the interact button now shows
  `nearestInteractable.prompt` (Try/Read/Look/Hall/Enter) instead of
  always saying "Enter" — falls back to "Enter" when nothing's in range
  (the interior-exit case). (2) Lookout's railing moved from y=10 to
  y=16 (plateau y=10-15): the landscape crop is only 10 tiles tall, so a
  camera centered on a player *at* the railing was topping out at y=15 and
  never actually reaching the Square (y=20) — confirmed by computing the
  visible tile range directly (was 9-19, now 11.5-21.5) and by screenshot,
  the Square's notice-post is now visibly in frame from the railing.
  Verified in-browser: both fixes work as intended, full regression set
  still holds (A/D, water, spawn free, no console errors). Still waiting
  on Brandon for a specific glue / sprites / sibling direction before
  starting any of those.
- **2026-09-16 — sibling mechanics.** `Persistence/TheCommons/sibling.md`:
  game daemon + nginx static `/commons/` next to `liquidgov_http` on the
  512 MB box. Presence RAM, SQLite WAL not rsync'd, friends via localhost
  cookie, civic crash-isolated. Path vs subdomain waits on a real
  Set-Cookie read. Not a BUILD. P3 still closed.
- **2026-09-16 — P3 closed.** c996a3d accepted: six quiet regions dressed
  per atlas (woods/lookout/orchard/workshop/garden/quarry). Shutter and
  quarry sign toast only. No new interiors. Civic untouched. Outdoor V1
  DoD met. Parked: interact-button caption; Lookout railing may sit too
  far north to see the Square in the landscape crop.
- **2026-09-16 — IndexedDB is a blob cache.** `Persistence/TheCommons/indexeddb.md`:
  later, tileset/house-snapshot/photo-staging. Not V1 inventory, not
  trades, not friends, not the 5.6 MB bake. Do not open IDB in commons/
  during P0–P3. P2 still closed. P3 waits.
- **2026-09-16 — P3 shipped.** `commons/index.html`: all 12 regions now
  dressed, matching `v1.md`'s full outdoor definition of done. North Woods
  (tree line on the world's north edge, scattered solitary trees, one
  cluster + sit-log), Lookout (a walkable plateau, a solid railing along
  its south edge facing the Square, a bench, one tree off the plateau),
  Orchard (six trees in loose rows), Workshop Lane (closed-shutter facade
  + bench + two crates — shutter interact toasts "Closed for now.", no
  interior, ever, without Brandon reopening that pin), Garden (flower-bed
  cluster + a low sitting wall), Quarry Ridge (boarded adit + sit-rock +
  a sign — only the **sign** is interactable, "Read" → "Closed.", the adit
  itself stays decorative, no ore, no interior, matches the locked pin).
  Same tileset throughout — new tile ids (tree, railing, plateau,
  flower-bed, low-wall, crate, adit) reused across regions per art.md,
  not one-off per-region art. No new doors, no new interiors, no state
  changes beyond two toasts. Verified in-browser: all six regions' set
  pieces render and collide correctly (adit/flowerbed/lowwall/crate/tree
  all solid, plateau walkable, railing solid), shutter and sign toast
  correctly, the adit itself confirmed *not* interactable, and the full
  P0-P2 regression set still holds (A/D, water/bridge, Homestead's
  boarded-door toast, Hall confirm/open, spawn still free, dual crop at
  390×844, no console errors). `liquidgov-platform/` untouched. This
  closes the outdoor world; P2's demo house/lab interiors are unaffected.
- **2026-09-16 — inventory is RAM.** `Persistence/TheCommons/inventory.md`:
  P2 `inventory = {}` vanishing on refresh is the spec, not a bug. Do not
  sessionStorage/localStorage/SQLite in this preview. Write-only object
  may toast a count later; placing items is glue. P2 still closed.
- **2026-09-16 — P2 closed.** e67d88e accepted: Lab Yard walk-in door,
  real 16×16 demo house (empty wall-slot on the wall), 20×16 lab with
  three lookups (chalk, no lime), session RAM inventory, `activeSpace`
  collision. Station interact wins over Enter-to-exit. Civic untouched.
  P3 (six quiet outdoor regions) waits on Brandon. Parked: interact
  button caption stays "Enter" until P3.
- **2026-09-16 — P2 shipped.** `commons/index.html` extended per
  `BUILD_ORDER.md`'s P2 list. Lab Yard dressed outdoors: a wall-tile
  building, a sign, and a Lab door that is a genuine **walk-in trigger**
  (per atlas.md, deliberately not the Enter-to-confirm family the other
  doors use — not solid, not in `interactables`). Demo house's P1 black
  stub replaced with a real 16×16 interior: table, two chairs, rug, and
  an empty wall-slot frame embedded in the wall itself (still empty —
  no glue, per `glue.md`). New 20×16 Lab interior with a workbench visual
  and three lookup stations (copper acetate, iron-gall ink, chalk-only —
  pushback.md's fix, no lime), each showing the real recipe name in a
  toast and recording one pickup in a plain in-memory `inventory` object
  (v1.md: session RAM, vanishes on refresh, no SQLite in this preview).
  Collision/movement/rendering generalized behind a single `activeSpace`
  (outdoor / demoHouse / lab) rather than copy-pasting the outdoor logic
  twice — same 12×12-box/4-corner/axis-split rules from collision.md
  apply unchanged inside either interior, only which tile/solid array and
  pixel bounds count as "the world" changes. Demo house keeps its P1
  Enter-to-enter/Enter-to-exit pattern exactly as shipped; the Lab is
  walk-in both ways, with Enter also working as a fallback exit. Real bug
  caught by testing, not review: `tryInteract()` originally checked
  `mode === "lab"` before checking `nearestInteractable`, so standing
  right next to a lab station and pressing Enter always exited the lab
  instead of triggering the station — no toast ever fired, at any
  distance. Fixed by checking the interactable first; the interior-exit
  fallback only applies when nothing is in range. Also fixed on sight
  (not asked, but the same class of bug as the P0 fountain trap): the
  wall-slot frame was placed one tile off the actual wall, floating on
  the open floor instead of embedded in it. Verified in-browser: Lab door
  auto-warps in and back out (both via walking onto the interior door and
  via the Enter fallback, landing at the intended spot outside each time,
  no oscillation), demo house enter/exit round-trips to the exact saved
  outdoor position, both entry tiles confirmed unblocked
  (`boxBlockedAt` false), all three lab stations produce the correct
  recipe text and inventory entry, and the full P0/P1 regression set
  still holds (A/D not inverted, water solid off the path, bridge still
  crosses it, boarded-door toast, Hall's `confirm()`/`window.open()`
  stubbed and verified, dual crop switches at 390×844, no console
  errors). North Woods, Lookout, Orchard, Workshop Lane, Garden, Quarry
  Ridge remain untouched — P3, not this round. `liquidgov-platform/`
  untouched.
- **2026-09-16 — dt clamp notes.** `Persistence/TheCommons/dt.md`: keep
  1/20; that is what stops tab-return tunneling through 1-tile doors.
  Do not add CCD to satisfy a dt=1 test. P1 still closed. P2 waits.
- **2026-09-16 — P1 closed.** Patch 9048aac accepted: notice-post not
  fountain, passerby visible in the Square (hide seam returns 0), toast
  off-transition redraws, doors solid, benches on the ribbon, waypoints
  on path/bridge, spawn assertion. Civic untouched. P2 waits on Brandon.
- **2026-09-16 — P1 patch shipped.** All items from
  `CLAUDE_P1_PATCH.md`/`GROK_P1_REVIEW.md`/`collision.md` fixed in
  `commons/index.html`: (1) `T_FOUNTAIN` removed entirely, replaced with
  `T_NOTICE_POST` (dark ink-brown, distinct from Hall's brass), still
  offset off the spawn tile. (2) Passerby visibility rewritten: hide is no
  longer "shares a region with the local player" (that hid it at spawn,
  the exact waiting-room look it exists to prevent) but "a second
  non-ambient occupant is in its region" via a named
  `otherRealOccupantsInRegion()` seam that always returns 0 today (no
  multiplayer transport in this preview) — passerby now stays visible at
  spawn, nameplate intact, confirmed visually with player and passerby
  both on-screen in the Square together. (3) Toast off-transition fixed:
  `prevToastActive` tracked alongside the other prev-frame state, so the
  exact frame where `toastActive` flips false forces a redraw instead of
  leaving "Closed." on the bitmap until something else happens to move.
  Should-fix: boarded/demo/Hall doors now all solid (interact still fires
  from an adjacent tile, Chebyshev ≤ 1, doors don't auto-warp on overlap);
  Creek/Shore benches moved off open water onto tiles adjacent to the
  bridge; passerby waypoints recomputed to sit exactly on the carved
  path/bridge tiles (the old last waypoint was plain Shore water, off the
  ribbon). Added a startup assertion (`assertSpawnIsFree`) that
  console.errors if a future prop ever lands back on the spawn tile,
  per `collision.md`'s explicit invariant. Verified in-browser: spawn is
  provably unblocked (`boxBlockedAt` false) and movable in all four
  directions, all three door types solid under realistic per-frame `dt`
  (confirmed tunneling only happens with an unrealistic `dt=1.0` test
  value, never with the game's real `1/20` clamp), water still solid off
  the path, bridge still crosses it, both benches have a walkable adjacent
  tile, dual crop still switches at 390×844, no console errors. No P2.
  `liquidgov-platform/` untouched.
- **2026-09-16 — collision notes.** `Persistence/TheCommons/collision.md`:
  keep 12×12 AABB + axis split; doors/notice-post solid; interact from
  adjacent; never spawn overlapping solid; passerby does not collide, so
  waypoints stay on the ribbon. For the open P1 patch, not a new phase.
- **2026-09-16 — Grok P1 review.** Spine accepted (Hall, 7 doors, bake
  labels, idle skip, hidden watchdog). Must-fix: notice-post not fountain;
  passerby must stay visible in the Square (hide only for a second real
  occupant); toast off-transition must redraw. Should-fix: boarded doors
  solid, benches on the path, passerby stays on the ribbon. Paste
  `CLAUDE_P1_PATCH.md`. P2 not started. Civic frozen.
- **2026-09-16 — P1 shipped.** `commons/index.html` extended per
  `BUILD_ORDER.md`'s spine list: the Square (fountain + 2 benches),
  Homestead Row (facade with 7 doors: door 0 = demo, doors 1-6 = boarded —
  no cookie glue, per `glue.md`), Creek/Shore (bench + recolored
  path-over-water as a "bridge" tile, cosmetic only), Hall Lawn (door +
  2 hedges, opens `liquidgov.us/proposals` in a new tab after a
  `confirm()`). North Woods/Lookout/Orchard/Workshop Lane/Lab Yard/Garden/
  Quarry Ridge untouched — P2/P3, not this round. Passerby implemented per
  the round-2 lock: nameplate exactly `passerby`, `isAmbient` flag
  internal, patrols Square→Homestead→Creek→Shore and back, hides whenever
  its current region (computed from AABB, never stored per-tile) matches
  the player's. One prompt at doors (`Enter`/tap a new on-screen button):
  Hall confirms+opens, the demo door stubs to a black placeholder interior
  ("[Enter] to step back outside"), a boarded door shows a transient
  "Closed." toast. `canvas.md`'s three P1 asks done: region labels are now
  baked into the offscreen canvas once (no more per-frame `fillText`),
  frame() skips the actual blit when nothing changed (player/passerby
  didn't move, crop unchanged, no toast) though rAF keeps ticking, and the
  P0-parked nit is fixed — the watchdog no-ops entirely while
  `document.hidden`, tearing down an already-running fallback too.
  Real bug caught by testing, not review: the fountain's placement formula
  put it on the exact same tile as the player's spawn point, so a fresh
  arrival's own bounding box already overlapped a "solid" tile and every
  direction of movement was silently rejected. Fixed by offsetting the
  fountain 3 tiles south of center. Verified in-browser: A/D still not
  inverted, water still solid off the path, the path/bridge still crosses
  it, dual crop still switches at 390×844, `document.hidden` watchdog
  guard confirmed both for "don't start while hidden" and "tear down an
  active fallback when hidden", demo door enter/exit round-trips, boarded
  door toasts without changing mode, Hall's `confirm()`/`window.open()`
  call verified with both accept and decline (stubbed, so production
  liquidgov.us was never actually opened during the test). No console
  errors. `liquidgov-platform/` untouched.
- **2026-09-16 — canvas 2D notes.** `Persistence/TheCommons/canvas.md`:
  P0 blit is the big win. P1: bake static labels, skip idle frames, one
  sprite atlas, no fillText-per-frame, watchdog respects document.hidden.
  Not a patch to commons/ yet. P1 still waits on Brandon.
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
