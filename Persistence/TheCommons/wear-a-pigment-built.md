# Wear a pigment — built, tested, DEPLOYED

Brandon: `BUILD wear a pigment`, approving `CLAUDE_WEAR_PIGMENT.md` as written. Unlike the backpack or the
door wording fix, this one is not client-only: a schema column, an occupant field, and a new daemon route.

## What changed

- **Schema**: `players.shirt_pigment_index INTEGER`, nullable (`schema.sql`, plus `Database::
  ensure_shirt_pigment()` in `db.cpp`/`db.hpp` for an existing database -- the same idempotent
  `ALTER TABLE ... ADD COLUMN` shape `visit_policy` already used, called from `main()` right alongside it).
- **`commons-daemon/src/studio.cpp`/`.hpp`**: `StudioStore::set_shirt(username, index)` -- the same unlock
  rule the easel's own palette already computes (charcoal 1-4 always legal, 5-13 gated on that pigment's
  own recipe being in `discoveries`), reusing the existing `PIGMENTS`/`pigment_by_index`/`has_discovery`
  rather than a second copy of the lock logic. `find_or_create_player`, not the read-only lookup --
  wearing charcoal can legitimately be a bound player's very first Studio action, same reasoning gather
  and bench already use. `shirt_hex_of(username)` for read-only lookups (join/rejoin hydration).
- **`commons-daemon/src/presence.hpp`/`.cpp`**: `Occupant` grew a `shirt_hex` field (`""` = default blue).
  `PresenceStore::update_shirt(id, hex)` -- a region-agnostic scan (same shape as `erase_id_locked`,
  `kGlobalCap` is 96, trivial to walk) that updates a *live* occupant in place, so a nearby friend sees a
  colour change without the wearer leaving and re-entering.
- **`commons-daemon/src/main.cpp`**: `POST /presence/wear {index}` (or `{}`/no index to clear) -- persists
  via `set_shirt`, then calls `presence.update_shirt()`. `occupants_json()` and the `/presence/join`
  response both grow an optional `"shirt"` field, omitted when empty (matches every other sparse field
  already in this codebase). Both `/presence/join` and `/presence/rekey` hydrate a bound occupant's
  `shirt_hex` from the database at the moment they join/move rooms.
- **`commons/js/easel.js`**: two new buttons in `#easelActions`, "Wear" and "Default", applying whichever
  palette swatch is currently selected for drawing (`easelColour`) -- the easel's own already-unlocked
  palette list is the picker, nothing new to build there (`CLAUDE_WEAR_PIGMENT.md` decision 4). Both route
  through `netWearPigment(index)`, which owns the bound-only gate itself (same shape as `studio.js`'s
  `netGather()`) rather than duplicating the check in each button handler.
- **`commons/js/net.js`**: `myShirtHex` (this client's own worn colour), set from every join/rekey
  response and from a successful wear call. `ingestOccupants()` and both region-poll call sites carry the
  new `shirt`/`shirt_hex` field through for every other occupant.
- **`commons/js/main.js`**: `drawOneActor()` paints one small `fillRect` over the sprite's own fixed body
  sub-rect (`destX+4, destY+8, 8, 10` -- the same offsets `paintSpriteAtlas`'s `drawFigure` already uses
  for every direction and frame) when `a.shirt` is set. No new atlas per colour. Applies identically to
  the local player and every other occupant in `drawSortedActors()`'s draw list.
- **`commons/index.html`**: the two new easel buttons.

## Tests

`client_files_unit.js` grew from 116 to **122 checks**: `netWearPigment` exists; the two buttons exist in
`index.html`; a guest calling `netWearPigment` directly never fetches and is told to log in; a bound call
posts to `/presence/wear`; `myShirtHex` exists after load; `main.js`'s source carries the body-recolour
`fillRect` and both `shirt`/`shirt_hex` push sites.

`test_studio.sh` grew from 121 to **129 checks**, a new section using a fresh account (`dave`) driven
through a real gather -> bench -> collect chain (not a database shortcut) to unlock pigment index 8:

- charcoal (index 1) wearable with zero discoveries; an out-of-range index is `400 unknown_index`; an
  undiscovered higher pigment (woad blue) is `409 not_unlocked`; an empty body clears back to the default
  and returns an empty hex.
- Index 8 (carbon ink) refused before its recipe is ever collected, then actually earned via `lab_gum` +
  `lab_soot` gathers, a `gum_solution` batch (binder), and a `carbon_ink` batch, each waited out on the
  test clock -- then wearable.
- Persists across a rejoin: the `/presence/join` response itself carries the worn hex.
- A second bound account (`erin`) standing in the same region sees `dave`'s occupant row carry `"shirt"`,
  and sees it disappear the instant `dave` clears it -- **without `dave` leaving or rejoining** -- proving
  `update_shirt()`'s live in-place push actually works, not just the persisted value at next join.
- `wear` was added to the existing guest-refusal loop (`gather`/`bench`/`mix`/`collect`/`wear`, all `403`).

Full suite: **`test_studio.sh` 129 checks, 0 failed**; **`client_files_unit.js` 122 checks, 0 failed**.

**A real bug found and fixed while writing these tests, not by inspection**: `set_shirt` originally used
the read-only `find_player_id`, which returns nothing for a brand-new account that has never gathered or
benched anything -- wearing charcoal (meant to be legal from minute one) would 409 for exactly the
players who'd most want to try it first. Fixed to `find_or_create_player`, matching gather/bench's own
precedent. Caught by the very first assertion in the new test section, not a hypothetical.

**A second issue found while testing, environmental rather than a code bug**: `presence.cpp`'s idle-reap
runs on the real wall clock; `studio.cpp`'s `now_ms()` reads the test harness's injectable clock file when
set. A frozen test clock left in the distant past (this file's shared `T0`, 2023) made freshly-joined test
occupants look decades idle to the real-clock reap, wiping them before the very next presence call. Not a
production bug (`COMMONS_TEST_CLOCK_FILE` is never set outside tests, so both clocks are simply "now"
there) -- worked around in the test itself by setting the clock to real time for the presence-touching
section, with a comment explaining why, rather than touching the reap logic.

**Resolved 2026-09-28**: `PresenceStore` now takes an injectable clock (`PresenceStore::Clock`, constructed in
`main.cpp` with the test-clock-aware `now_ms()`), so stamping and reaping share one timebase and the workaround
is gone; `test_studio.sh` now asserts the reap follows the test clock.

**Mutation-checked**, each restored after confirming the right failure:
1. Client: removed the guest gate from `netWearPigment` -- both the "never fetches" and "posts once"
   checks failed (the second failed by counting two fetches instead of one, since the guest call no
   longer no-op'd).
2. Client: removed the body `fillRect` line from `drawOneActor` -- the source-inspection check failed.
3. Daemon: removed the `not_unlocked` check from `set_shirt` -- the woad-blue refusal assertion failed
   (got `200` instead of `409`).
4. Daemon: removed the `presence.update_shirt()` call from the wear route -- the "gone from the region GET
   immediately, no rejoin needed" assertion failed, still showing the stale colour on `dave`'s live
   occupant row after he'd cleared it.

## Limits, said plainly

- Verified live in the browser preview only up to a point: the shared `inventory`/UI plumbing (the easel's
  buttons render, guest-safety toasts) was confirmed the same way the backpack was, but the actual sprite
  recolour could not be visually confirmed through this session's browser preview -- its caching layer
  served a stale `main.js`/`net.js` to the running page through several full reloads and even fresh
  server instances, while a direct cache-busted `fetch()` of the same URL confirmed the served file on
  disk was correct throughout. This is the same artifact noted in the door-wording fix, not a new one.
  The logic itself is exercised end-to-end by `client_files_unit.js` (loads the real file in a real JS
  engine) and `test_studio.sh` (real HTTP calls against the real compiled daemon), which is why it's
  reported tested rather than unverified -- but nobody has *looked* at a recoloured shirt on screen yet.
- Thirteen possible colours, one flat body rectangle -- no hat, hair, or pants, matching exactly what the
  existing sprite atlas can recolour without a new art file.

## Deploy

Deployed 2026-09-28, revision `cdea606` (daemon cycle + `--with-client`). Because it deploys HEAD, it also
carried the friend-door wording fix (`6f5ade1`), which had been waiting since before this build.
`verify.sh`: 37 checks, 0 failed, all 13 tables unchanged in row count against the pre-deploy backup.
`verify.sh` doesn't exercise the new column or route, so two extra read-only checks were run after it:
`POST /presence/wear` with no cookie answers `403 not_bound` (the route is live), and
`pragma table_info(players)` on the live database lists `shirt_pigment_index` (the migration applied).
Still true: nobody has *looked* at a recoloured shirt on screen (see Limits above) -- worth a glance the
first time a bound player earns a discovery-gated pigment on the live site.
