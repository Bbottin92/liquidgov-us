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

- **2026-09-17 — commons: guest credentials survive an interior stay.**
  Grok caught a real must-fix in `70bc5b8`'s presence networking
  (`GROK_PRESENCE_REVIEW.md`/`CLAUDE_PRESENCE_PATCH.md`): `GuestRegistry`
  reaped credentials on the same 30s clock as `PresenceStore`'s plaza-slot
  reap, but a guest in a house/lab stops heartbeating presence entirely
  (`netLeave()` on entry, nothing calls `verify()` again until they walk
  back out). Any interior stay over 30s meant the credential died while
  the guest was still very much active, and exiting got a silent 403 with
  no way back in. Fixed both ways the patch asked for:
  1. **Daemon**: `GuestRegistry::kIdleMs` raised from 30s to 30 minutes
     (`guest.hpp`) -- `PresenceStore`'s own 30s plaza-slot reap is
     untouched, so a crashed/closed tab still frees its region slot fast;
     only the credential's own lifetime changed.
  2. **Client**: `join`/`move`/`rekey` now check for a 403 response and
     call a new `netHandleAuthFailure()` -- mints a fresh credential and
     rejoins from the player's current outdoor position, guarded by
     `netReauthInFlight` so one bad response can't fire overlapping
     remints. This also covers a daemon restart mid-session, which the
     30-minute window alone can't (RAM state is gone either way).
  Verified live: entered the demo house, waited 32s (past the *old* 30s
  window), walked out -- same guest id, clean `200` on rejoin (not a
  remint), daemon confirms the occupant reappeared. Separately killed and
  restarted the daemon while a tab sat outdoors (wipes `GuestRegistry`
  even with the 30-minute window) -- the next throttled move/poll hit a
  403, `netId` changed (real remint), rejoin succeeded, no thrown error,
  two-tab presence still worked afterward. Full regression set (catalogs,
  413 cap, missing-field 400, wrong-secret 403) still holds. Should-fix
  items explicitly left for later per the review: camera-neighbor
  polling, `display_name` spoofing, CORS `*` (fine until `lg_session` is
  ever forwarded), hardcoded loopback host. Civic untouched, no SSH/
  systemd/nginx, no deploy.
- **2026-09-16 — commons real presence networking + server-issued guest ids.**
  Brandon asked for a client-side round; found first that `commons/index.html`
  had zero daemon wiring at all (no `fetch` calls anywhere -- presence was a
  pure local simulation, `otherRealOccupantsInRegion` was a hardcoded 0).
  Rescoped with Brandon to full presence wiring, id-first, rather than
  building an inert guest-id endpoint nothing would call. Landed both sides:
  - **Daemon**: new `GuestRegistry` (`src/guest.hpp/.cpp`) mints a fresh,
    unguessable `(id, secret)` pair via `POST /presence/guest` --
    `id` is the public occupant label already broadcast by every
    `/presence/<region>` GET; `secret` is a private bearer credential
    returned only once and never echoed back. `join/move/rekey/leave` now
    all require a matching `secret`, constant-time compared, closing a real
    (not just hypothetical) hijack: any client in a region can already see
    every occupant's `id` from the GET feed, so before this a client could
    replay someone else's `id` in a `join` call and steal/rename their row
    outright -- confirmed live (wrong secret -> 403, occupant row
    untouched; unknown id -> 403; missing secret -> 400 same as any other
    missing field). Unclaimed credentials expire after 30s
    (`GuestRegistry`'s own idle window, same shape as `PresenceStore`'s).
    Also added CORS (`Access-Control-Allow-Origin: *` + an `OPTIONS`
    preflight handler in `http.cpp`) -- needed the moment any browser page
    calls this loopback daemon cross-origin, which nothing did before now.
  - **Client**: `commons/index.html` now really talks to the daemon.
    `enter()` mints a credential; the outdoor frame loop joins on first
    tick, rekeys on crossing an outdoor AABB (no raw leave+join, matching
    presence.md's "no ghost in the old list"), and throttles move+poll to
    ~2Hz (presence.md's tick rate) rather than every render frame.
    `otherRealOccupantsInRegion` now reflects a real poll instead of a
    hardcoded 0, so the passerby's hide-rule reacts to actual other
    players for the first time. Entering a house/lab calls a clean
    `netLeave()` (interiors use `instance:<id>` keys presence.md says V1
    doesn't wire for guests); exiting rejoins automatically via the same
    join-on-null-region path used at login, no separate code path needed.
    `x,y` sent as fractional region-local tile coordinates (not integers,
    not world-absolute) -- satisfies presence.md's literal "tile
    coordinates local to that region" while keeping sub-tile smoothness.
    **Known V1 simplification, stated not hidden**: only the player's own
    current region is polled/rendered, not every region whose AABB
    intersects the camera (presence.md's actual tick-rate spec) -- proves
    real multiplayer works, but a neighbor just past the camera edge won't
    show yet. Remote occupants render at a static standing frame (no
    animFrame in the occupant feed); smoothing between polls is a later
    nice-to-have.
  - **Bug found and fixed during live testing, not by inspection**:
    `netLeave()` first used `navigator.sendBeacon` with a JSON `Blob` body.
    That's cross-origin-preflighted, and the actual POST failed
    (`net::ERR_FAILED`) against this daemon even though the `OPTIONS`
    preflight itself succeeded -- and `sendBeacon()` still returned `true`
    regardless (that return value means "queued," not "delivered"), so the
    `if (!sent) fetch(...)` fallback never fired and a house/lab entry
    silently never told the daemon to leave. Switched to plain
    `fetch(..., {keepalive:true})`, already proven working for every other
    call; interior-entry leave now verified reaching the daemon (200 OK).
    A genuinely undelivered leave (e.g. an abruptly closed tab, tested via
    closing the tab outright) still isn't guaranteed -- no browser
    guarantees that -- but `PresenceStore`'s existing 30s idle reap is the
    correct backstop and was reconfirmed cleaning up that exact case live.
  Verified end-to-end in two real browser tabs against the live daemon
  (not just curl): two guests join, see each other by name (not the
  passerby), region-crossing rekeys with no ghost left in the old region,
  interior entry/exit leave-and-rejoin correctly, an unreachable daemon
  degrades to the pre-existing local-only simulation with no thrown
  errors, and the full prior regression set (catalogs, 413 cap, missing-
  field 400, restart boundary) still holds. Added `commons-client` to the
  shared (global, not project-local) `.claude/launch.json` alongside the
  existing `liquidgov-http` entry -- a static `python3 -m http.server` for
  `commons/`, port 8090, `liquidgov-http`'s own entry untouched.
- **2026-09-16 — commons-daemon: should-fix round (reap, move timestamp,
  schema dedup, sqlite via FetchContent).** Brandon scoped this round to
  "should-fix daemon items" only (not client, not sibling/deploy) via
  explicit confirmation. Four of `GROK_DAEMON_REVIEW.md`'s five should-fix
  items landed:
  1. `move` now stamps `last_update_ms` on every call — occupant JSON no
     longer looks frozen between joins.
  2. Idle reap: `PresenceStore::reap_locked` runs under `mu_` at the top
     of every public method (join/move/rekey/leave/occupants/global_count)
     and drops any occupant whose `last_update_ms` is more than 30s
     (`kIdleMs`) stale. A crashed/closed tab no longer holds a cap slot
     until process restart. Single-threaded, lazy-on-access — no timer
     thread, matching the daemon's existing synchronous style.
  3. `schema.sql` dedup: `commons-daemon/schema.sql` is now a symlink to
     `Persistence/TheCommons/schema.sql` (verified content was already
     byte-identical apart from a comment) — one source of truth, no more
     copy-drift risk.
  4. Vendored `sqlite3.c`/`.h` (9.5MB) dropped from git; CMake now
     `FetchContent`s the same 3.53.4 amalgamation from sqlite.org, pinned
     by SHA256 (downloaded once with Brandon's explicit permission to
     compute the hash, content verified byte-identical to the prior
     vendored copy before switching). Real tradeoff accepted: the build
     now needs network access at configure time; it was previously fully
     offline. `db.hpp`'s include path updated to match (`"sqlite3.h"` via
     the target's public include dir, not a relative `third_party/` path).
  **Deliberately not done:** server-issued guest ids (should-fix #3,
  impersonation resistance). This can't be meaningfully half-done
  daemon-side — the server minting an id nobody's client asks for or
  stores achieves nothing, and `commons/index.html` still generates its
  own guest id today. Stays parked for an explicit client-side round
  before nginx is ever in front of this daemon. Verified live: full
  regression set, `move` timestamp advances on move, an untouched
  occupant is gone after >30s while one refreshed via `move` survives,
  clean build from a bare `rm -rf build` (proving FetchContent actually
  works, not just "worked once"), and the restart boundary (SQLite
  catalogs survive, RAM presence + reap state do not). Civic untouched,
  no SSH/systemd/nginx, no deploy.
- **2026-09-16 — commons-daemon: body-cap nit, truncate to `Content-Length`
  after the split.** Grok caught a gap in the 413 patch (`d2b5bf4`): the
  1 MiB header guard bounds `raw`, not this request's declared body — so
  even when `Content-Length` is legal (<=64KiB), the initial `head`/`body`
  split in `serve_one` could already hand back more bytes than `need`
  (whatever trailed the header terminator in the same reads), and nothing
  trimmed it back down. Not the 5GB hole (413 still fires correctly for an
  illegal length), but trailing/pipelined-looking bytes could silently ride
  along as part of this request's body. Fixed: after the read loop,
  `body.resize(need)` whenever `body.size() > need`. Verified live: a
  legal small `Content-Length` with garbage glued on in the same packet
  still joins clean (occupant reflects only the real JSON), the 413
  boundary and normal regression set are unaffected, and a restart still
  shows the RAM/disk boundary holding.
- **2026-09-16 — decided.** GitHub shares `Persistence/notes.md` (+ README) only.
  `context.md` and `state.md` exist in the *old* Grok sandbox and were never
  pushed. Do not invent fakes. Do not have Claude create them. A fresh Grok
  hard-gate that requires those two files will fail until we drop that read or
  Claude commits the *real* files from that sandbox. “Have Claude git pull”
  updates notes/README only.

- **2026-09-16 — commons-daemon: body-cap must-fix patched (`http.cpp`), presence
  bugs already fixed upstream.** Grok's review (`GROK_DAEMON_REVIEW.md`,
  `CLAUDE_DAEMON_PATCH.md`) flagged three must-fixes: (1) duplicate `join`
  double-counting `total_`, (2) `rekey` always decrementing `total_` even
  when `from_region` never held the id, (3) `Content-Length` uncapped past
  the 1 MiB header guard. Before patching, `git pull` surfaced that (1) and
  (2) were already fixed upstream by a newer id-index redesign of
  `presence.cpp`/`presence.hpp` (`92e379d`, `63792ea`: an id-lookup replaces
  the old scan-and-decrement pattern, `rekey`'s `from_region` is now
  advisory only since `join_locked` de-dupes globally regardless) — pulled
  that in rather than reapplying an older, now-superseded fix and clobbering
  it. Only (3) still needed work: `serve_one` now rejects `need < 0` or
  `need > 65536` with a 413 and closes the connection before the body-read
  loop, so a client-declared multi-GB `Content-Length` can never reach
  `body.append` on the 512 MB box. Verified live: duplicate join (same
  region and cross-region) collapses to one occupant; rekey from a region
  the id was never in leaves `global_count()` correct; a 5 GB, a negative,
  and a 65537-byte declared length each get a 413 and the daemon stays up
  and keeps serving normal requests; a full process restart still shows
  SQLite catalogs surviving and RAM presence clearing. No should-fix items
  touched (idle reap, server-issued ids, FetchContent, schema dedup, cookie,
  deploy) — those stay parked. Civic untouched, no SSH/systemd/nginx, no
  deploy.
- **2026-09-16 — Sibling: daemon written and tested locally, not deployed.**
  Brandon confirmed the scope explicitly before any droplet contact:
  write the daemon locally first, zero SSH/systemd/nginx this round.
  New `commons-daemon/` (C++, own CMake build, separate from
  `liquidgov-platform/`): a real SQLite ledger (vendored `sqlite3`
  amalgamation — WSL has the runtime `.so` but no `-dev` headers and no
  sudo to install them; downloaded the official amalgamation from
  sqlite.org with Brandon's explicit permission first, per the file-
  download rule) running `schema.sql`/`seed.sql` (the locked V1 catalog:
  3 lab recipes chalk-only, 12 outdoor regions, house/lab interior
  templates), RAM-only presence (`presence.md`'s shape — keyed by region,
  join/move/rekey/leave, 24/region and 96 global caps), and a minimal
  single-threaded synchronous HTTP server deliberately shaped like
  civic's own rather than a framework. Friends/cookie-forwarding and
  wall-slot fetch are documented (`glue.md`, `cookie.md`,
  `canvas-frame.md`) but **not wired** — no civic route exists to call
  and adding one means touching `http_server.cpp`, out of scope. Tile
  collision/movement stays client-side JS (`wasm.md`) — not duplicated
  here. Verified end-to-end, not just "it compiles": full presence
  lifecycle (join/move/rekey confirmed to leave no ghost/leave), per-
  region and global cap enforcement, a missing-field 400, and — the
  actual point of an integration test, not just a happy path — a real
  process restart: SQLite catalogs survived, RAM presence did not,
  proving the RAM/disk boundary `presence.md` describes rather than
  asserting it. `liquidgov-platform/`/`http_server.cpp`/civic untouched,
  no SSH, no systemd unit, no nginx change, nothing on the droplet.
- **2026-09-16 — Correction: `lg_session` does get `Secure`, conditionally.**
  My own first pass on the cookie (same session, bullet below) grepped
  only the literal `Set-Cookie:` string in `set_cookie()` and reported
  "no `Secure`" as flat fact — missed the very next line,
  `if (g_https) o += "; Secure";` (line 1112), and `clear_cookie()`'s
  matching one. `g_https` is set per-request (line 6824) from
  `X-Forwarded-Proto`, so on the real site behind nginx it's true for
  real traffic and the cookie the browser actually gets **does** carry
  `Secure`. Caught by Grok independently citing
  `deploy/ux-on-droplet.patch` and writing `cookie.md` concurrently — a
  real overlap this time, not just a doc-only commit alongside — merged
  by rechecking the source directly rather than taking either version on
  trust, confirming Grok's finding first-hand before writing it into
  `glue.md`. Lesson for next time reading this file cold: a `+= "..."` or
  a conditional two lines past the literal string you grepped for is easy
  to miss with a narrow grep; read the whole function, not just the match
  line. `glue.md`'s host-only/no-`Domain=` finding was independently
  correct and unaffected.
- **2026-09-16 — Glue: cookie and wall-slot are now verified facts.**
  Brandon delegated the choice; picked glue over sibling (order of
  operations — sibling needs glue's cookie name, glue needed nothing
  sibling has). Read the real `http_server.cpp` (WSL copy) and filled in
  `glue.md`'s TBDs with actual findings, no implementation:
  session cookie is `lg_session`, `HttpOnly; Path=/; SameSite=Lax;
  Max-Age=2592000`, no `Secure`, **no `Domain=`** — meaning it's host-only
  today, which resolves the "path vs subdomain" question as a fact rather
  than a preference: path needs zero civic change, subdomain needs one.
  Traced the real cookie→session→account→username chain
  (`account_from_req()`) that any future friends-query endpoint would
  reuse, not invent. Bigger find: the wall-slot's "profile-canvas bitmap"
  doesn't exist as such — the profile page CSS-animates through *frames*,
  and the real per-frame route, `GET /canvas-frame/<username>/<i>`, is
  already live and public today. Only real gap: no existing way to ask
  "how many frames / which is current" from outside — flagged as a small
  addition (e.g. a `/latest` alias), not a blocker. Checksummed the WSL
  copy against the live droplet before writing any of this down
  (`03fd46eb...`, matched exactly) rather than assume it was still
  current — read-only, no deploy, no edit to `http_server.cpp`. Sibling
  still waits on Brandon; nothing in `commons/index.html` changed this
  round.
- **2026-09-16 — Sprites patch: idle-skip sees facing now.** Fixed the
  must-fix from `GROK_SPRITES_REVIEW.md`: `updateFacingAnim` could change
  `player.dir` on a fully blocked step (hold a new direction into a
  wall — `movePlayer` returns `false`), but `frame()`'s idle-skip only
  checked position/toast, so the sprite kept showing the old facing cell
  until the next real step. Added `prevPlayerDir`/`prevPlayerAnim` and the
  same pair for passerby, tracked alongside position exactly like the
  existing prev-frame state, and included both diffs in `changed`. Idle
  skip itself is untouched — still skips the blit when truly nothing
  changed, per canvas.md; it just now correctly counts "started facing a
  different direction" as a change. Verified in-browser: walked the
  player against the solid Homestead wall, tapped a different direction
  into it (movement fully blocked, position unchanged), and confirmed
  both `player.dir` and `prevPlayerDir` updated together — proving the
  redraw branch actually ran, not just that the state variable changed.
  Full regression set still holds (A/D, water, spawn free, dual crop at
  390×844, no console errors). Glue and sibling still wait on Brandon.
- **2026-09-16 — Sprites shipped.** Brandon picked sprites first, lowest
  blast radius per Grok's own suggested order (glue needs a real cookie
  decision, sibling needs glue's cookie name, sprites needs neither).
  `commons/index.html`: player and passerby are no longer black squares.
  One procedurally-baked 16×24 sprite atlas (4 dirs × 2 walk frames, art.md)
  built once at init — same bake-then-blit shape as the tile bake, one
  `drawImage` per actor per frame (canvas.md: no per-frame `fillRect`, no
  save/restore, no `<img>` per direction). Player and passerby share the
  exact same atlas (locked: "online others: same sprite"). Facing follows
  input direction (updates even on a blocked step, so bumping a wall still
  turns you to face it); the walk-cycle leg-swap only advances on an
  actual successful move, resetting to a standing pose when idle or
  blocked. Renamed `passerby.dir` (its patrol ping-pong ±1) to `wpDir` to
  free up `.dir` for sprite facing — same field name, unrelated meaning,
  would have silently clobbered each other otherwise. Sprite atlas context
  is deliberately `{alpha:true}`, the one exception to canvas.md's
  `alpha:false` rule: that rule is for the opaque full-frame bake/display
  layers, and a sprite composited over the world needs real transparency
  or it blits as an opaque box. Verified in-browser: atlas cells visually
  distinct per direction (eyes down, none up, single dot left/right), walk
  frame toggles while moving and resets when stopped/blocked, facing
  updates correctly for all 4 directions, nameplate repositioned above the
  taller sprite's head, full regression set still holds (A/D, water/
  bridge, boarded-door toast, Hall confirm/open, spawn free, dual crop at
  390×844, no console errors). Still no glue, no sibling, no droplet
  contact. `liquidgov-platform/` untouched.
- **2026-09-16 — WebGPU textures crib.** `webgpu-textures.md`: textures
  cannot mapAsync; readback is copyTextureToBuffer. Pixel art uses
  nearest / textureLoad. Commons keeps Canvas 2D drawImage.
- **2026-09-16 — WebGPU mapAsync crib.** `webgpu-mapping.md`: staging
  copy, 8-byte align, buffer cannot be STORAGE and mapped. Why GPU
  collision loses to `solid[i]`. Still no shaders in commons/.
- **2026-09-16 — WGSL crib only.** `Persistence/TheCommons/wgsl.md` is
  syntax, not a renderer. `webgpu.md` still stands: no shaders in
  commons/. Next visible work still sprites / glue / sibling.
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
