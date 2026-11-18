# The Workshop interior (A: thin, like the Hall) — built, tested, DEPLOYED 2026-09-20

Brandon: "A, E" (2026-09-20), answering my question on the parked list: **A** = a thin
Workshop (open the shutter, one plain room; no crafting, no items, nothing new in the daemon
or database). **E** = leave Privacy/Terms as they are (not a lawyer draft; that needs a
lawyer, not a rewrite by me). No spec existed: V1 said "no workshop, crafting, farming" and
`claude_v2_question.md` called it "deferred, not impossible"; the P3 code comment pinned
"no interior, ever ... without Brandon reopening the pin". This is that reopening, scoped to A.
Client only. Civic, daemon, nginx, schema, LGP1 untouched. **No deploy.**

## What ships (`commons/js/`)

- **The shutter is a door.** The Workshop Lane facade tile (11,27) was `T_DOOR_BOARDED` with a
  `shutter` interactable that toasted "Closed for now." It is now `T_DOOR_DEMO` (the existing
  solid Enter door, no new art) with a `workshopDoor` interactable, prompt **Enter**. The
  `shutter` interactable and its toast are gone. `WORKSHOP_DOOR_TILE` is declared beside
  `LAB_DOOR_TILE`/`MINE_DOOR_TILE` (world is built during script load).
- **Interior, 14x10, `mode: "workshop"`** (`interiors.js`, `workshopSpace`).
  `INTERIOR_INSTANCE_KEY.workshop = "instance:workshop"` (`net.js`): the same shared-RAM
  pattern as demo/lab/mine; guests and bound players both walk in; no rows, no snapshot. The
  daemon treats `instance:` keys as opaque, so nothing changes there.
  Contents, all existing tiles: a lamp set into the back wall, three workbenches on each side
  of it along the back wall, a worktable (two `T_TABLE`) with two stools (`T_CHAIR`), four
  crates, a two-tile rug, one sign: **"Sawdust and a bench. Nothing to make here yet."**
- **Nothing to do in it.** No stations, no pickups, no place; the interior tick skips
  `netPollPlaced()` for the workshop like the mine. The place banner reads "the Workshop".
- **Leaving.** Enter with nothing in range, onto the tile in front of the door
  (`WORKSHOP_DOOR_TILE.y + 1`), so no re-entry oscillation. Next to the sign, Enter reads it
  (a real interactable wins), same rule as everywhere.
- Passerby stays outside (drawn only in `mode === "outdoor"`).

## Tested

- `world_scatter_unit.js` extended (15 -> 27 checks): the door tile carries exactly one
  `workshopDoor` interactable with the Enter prompt; no `shutter` is left; it is the solid
  demo-door tile on the facade; its front tile is reachable from the spawn. `interiors.js` is
  now run on top of `world.js` in the same context as the browser loads them, and the room is
  checked for size and real floor (82 open tiles, so an empty room can't pass vacuously), the
  entry/door geometry, the doorway tile, that **every open tile is reachable from the entry**,
  one sign interactable with text, the sign reachable, and that `net.js` maps
  `workshop -> "instance:workshop"`.
  **That flood fill caught a real bug on its first run:** with the sign at (12,2) the corner
  tile (12,1) was sealed off (81 of 82 reachable). Sign moved to (12,3); 82 of 82.
- `test_world_scatter` 7/7, `test_client_files` 23/23 (syntax + load order of all eight scripts).
- Real browser, scratch daemon on 127.0.0.1:8765 (throwaway DB, removed after), as a **guest**
  through the name gate: teleported to the tile in front of the door (`Workshop Lane`, prompt
  `Enter`); a real Enter key press -> `mode: "workshop"`, region `instance:workshop`, spawn (7,8),
  banner "the Workshop", screenshot shows lamp, benches, worktable, crates, sign; the daemon's
  `GET /presence/instance:workshop` listed the guest and `GET /placed/instance:workshop` was
  `{"item":""}`. Sign toast read exactly. Enter away from the sign -> back outdoors at (11,28),
  region `Workshop Lane`, `instance:workshop` presence empty again. Regression: the mine and the
  demo house still enter/exit. No console errors.
- Note for automation: the browser tool's key name must be `Enter` (`Return` does nothing).

Not exercised: two clients in the workshop at once, and a bound player. Both ride the generic
shared-instance path demo/lab/mine already use (`instance:` keys are opaque to the daemon).

## Not here

- No deploy. Client-only when Brandon says `deploy`, after Grok's review (the client sync
  script, then a live check with two guest tabs).
- No crafting, no items, no friends gate, no new art (a real workshop sprite/tiles would be
  its own build), no daemon/civic/nginx/schema change.
- **Privacy/Terms (E): unchanged.** The site-sense pages remain plain-language, "not a lawyer
  draft" (`site_sense_built.md`). Real coverage needs a lawyer or a vetted template; I can draft
  a version for a lawyer to mark up if asked.
- **Stale parked list corrected:** `rebind()` and orphan reaping were already built and
  deployed 2026-09-19 (`rebind_built.md`, `orphans_reap_built.md`); they were not open.

## Deployed (2026-09-20, Brandon: "deploy")

Client only, via `commons-daemon/deploy/sync-client-to-droplet.sh` (two phases, `index.html` last),
then `chown -R liquidgov:liquidgov /opt/commons`. Commit `e6bd485` (pushed first; the script refuses
unpushed/dirty trees). Before syncing I diffed md5s droplet vs local: exactly the five changed files
(`interact.js`, `interiors.js`, `main.js`, `net.js`, `world.js`) differed and nothing else had drifted
(droplet was at `9e4611a`, the PWA deploy). After: all client files md5-equal to local,
`.deployed-client-rev` = `e6bd485`, `/commons/`, `/js/world.js`, `/js/interiors.js`, `/presence/lots`
all 200 over HTTPS, served files contain `workshopSpace`/`workshopDoor`, public `/internal/friends`
404. Daemon, civic, nginx, schema untouched and never restarted; liquidgov, commons-daemon, nginx
active throughout.

Live check on production, two guest tabs (`shopA`, `shopB`): both walked into `instance:workshop`
(`GET /commons/presence/instance:workshop` listed both, and **each tab drew the other**, the
two-clients case that was untested locally); sign toast "Sawdust and a bench. Nothing to make here
yet."; Enter away from the sign put each back on Workshop Lane at (11,28) with the workshop presence
empty again. No console errors. Not exercised on production: a bound player in the workshop (same
generic path as demo/lab/mine).

Full-suite note: in one full local `run_all.sh`, `test_client_files` (node exited 5 with no output)
and one `test_visiting` assertion failed; both passed on an immediate solo rerun (23/23 and 22/22).
Every other file passed. Treated as load flakes on this WSL box (not a code regression: the change is
client-only and both files were green before and after); CI is the second opinion.
