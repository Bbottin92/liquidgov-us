# Friends query — built, local-tested, not deployed

Brandon: "BUILD friends query" (`GROK_V6_ANSWER.md`: "mutual accept, RAM
TTL, Homestead lots 2-7. Visiting (and therefore the mine) wait on
this."). This is the **second civic edit** in the project, authorized by
name, narrowly scoped the same way the cookie resolver was.

## What's new

**Civic (`http_server.cpp`), one route, ~20 lines:** `GET
/internal/friends`. Same gate as `/internal/session-user` (404 if
`X-Forwarded-For` is present or the raw peer isn't loopback). Identity
is the session cookie only -- there is no username parameter, so there
is no way to ask about anyone but the session's own account
(`glue.md`: a public username query is a roster scrape). Returns
`{"username":"a","friends":["b","c"]}` from `g_store.friends_of()` --
mutual accepts only, minus anyone blocked in either direction -- or
`{}` if the cookie doesn't resolve. Read-only; nothing else in civic
touched.

**Daemon:**
- `civic.cpp` refactored so `resolve_username` and the new
  `resolve_friends` share one loopback fetch (same 1s timeout, same
  "no `lg_session` = no socket" gate). Every failure path yields "no
  friends".
- `friends.hpp/.cpp` (`FriendsCache`): the RAM-only TTL `glue.md`
  asked for, 15s, keyed by the forwarded cookie, capped at 256 entries,
  never persisted, never a table. Guests are never cached.
- `PresenceStore::bound_usernames()`: who is bound and in the game
  right now. `atlas.md`: a dynamic door resolves only for a friend who
  is "online/friended and bound" -- presence still knows nothing about
  friendship; the daemon intersects the two.
- `GET /presence/lots`: your mutual friends who are in the game, sorted
  case-insensitively (a door doesn't swap who it shows between polls),
  first six = lots 2-7. Lives under `/presence/` on purpose, so the
  existing nginx location (which already forwards `Cookie`) covers it --
  **no nginx change.** Registered before the `/presence/<region>` prefix
  route because dispatch is first-match.

**Client:** the six boarded facade doors are now `friendDoor`
interactables (same solid tile). A bound player near the Homestead
facade polls `/presence/lots` every 5s (never while it's off screen,
never as a guest); a door with a friend draws lit with their name over
it, and its prompt becomes the name.

## Scope decision worth checking

**Visiting is not part of this build.** `GROK_V6_ANSWER.md` says visiting
waits *on* the friends query, and there is no per-house visit setting
anywhere (`open / friends / locked` exists only as a phrase in
`notes.md`; no column, no UI). So a named door says "<name>'s house.
Visiting isn't open yet." and does not open. The daemon's
`not_your_house` ownership check is unchanged, so nobody can walk into
another player's room. The next increment is visiting, which needs a
visit-policy decision first.

## Not here

- Visiting, the mine, `/latest` -- unchanged, still gated.
- Reshuffle-proof slot assignment: sorted-by-name means a friend who
  comes online can shift doors that were already showing names. Stable
  slots would need per-session memory; not worth it at Square scale.

## Tested (local, scratch stores)

Real civic accounts alice/bob/carol/dave; alice requested all three,
bob and carol accepted, dave never did.
- `/internal/friends` with alice's cookie -> `["bob","carol"]`, no
  dave. No cookie -> `{}`. Spoofed `X-Forwarded-For` -> 404.
  Bob's cookie plus `?username=alice` -> bob's own list (the parameter
  is ignored).
- Daemon `/presence/lots`: empty while friends are offline; `["bob"]`
  once bob joins; `["bob","carol"]` (sorted) once carol joins; dave
  (not a friend) and alice (self) never appear; bob's own view shows
  `["alice"]`; a guest gets `[]`.
- A region key is not swallowed by the new route (`/presence/1,2`
  still returns occupants).
- Alice blocks carol: civic drops her immediately; the daemon drops her
  after the 15s TTL -- the intended staleness.
- Client, real browser against a scratch daemon: guest polls zero times
  and all six doors stay "Closed"; bound near the facade polls once,
  fills doors in slot order (long names truncate), lit doors render with
  names, tapping a named door says visiting isn't open, an empty one says
  "Closed"; 2 polls in 12s (5s throttle); a friend leaving reverts the
  door; zero polls while away from Homestead.

Caught only by loading the page in a browser: `FRIEND_LOT_DOORS` was
declared *after* the code that builds the world runs, so the whole
client died at load with `undefined.push`. Moved above the tile ids.
Static reading would not have found it.

## Should-fix from GROK_FRIENDS_REVIEW.md, closed before deploy

`FriendsCache` was keyed by the whole request head while its comment
said cookie. Now keyed on the `Cookie:` value only
(`civic::session_cookie`). Verified: four polls with the same cookie but
different unrelated headers made one civic call; a different cookie made
a second; a guest made none; a poll past the 15s TTL made a fresh one.
Folded in now because it was one small commit, and a later patch would
have cost another daemon restart with live occupants on the box.

## Deployed 2026-09-18

Brandon: "deploy" -- **civic, then daemon, then client**, no nginx, per
`GROK_FRIENDS_REVIEW.md`. Drift-checked first: civic's diff against the
droplet was 25 added lines and zero removed (only the new route); the
daemon diffs removed only lines this round intentionally rewrote.

1. **Civic:** synced, rebuilt, `liquidgov` restarted. Public
   `https://liquidgov.us/internal/friends` -> **404** (plain
   `not found`); same-box, no cookie, no XFF -> `{}`; `/` stayed 200.
2. **Daemon:** synced, reconfigured (new `friends.cpp`), rebuilt,
   restarted; `/health` ok, and `/commons/presence/lots` answered
   through the existing nginx location with no config change.
3. **Client:** synced last; served page carries the lot code.

Verified in production with three throwaway accounts (one requested
the others; two accepted, three never did):
- `/commons/presence/lots`: empty while friends were offline;
  `["fltesttwo"]` for one once two was in the game; `["fltestone"]` for
  two; `[]` for three (no accepted friendship) and for a guest.
- Real browser, logged in as one, at the Homestead facade: lot 2 lit
  with `fltesttwo`, the other five boarded, the online-but-not-a-friend
  account absent. Standing at the lit door: "fltesttwo's house.
  Visiting isn't open yet.", mode stayed outdoor, tile still solid.
- A direct join into another account's house from one's own session:
  403 `not_your_house` -- interiors are still closed.
- Civic and daemon both active, logs clean, `/` and `/commons/` 200.

Test hygiene: `/tmp` did not survive between shell calls, so the first
keepalive lost its cookie jars and the friend silently expired --
caught because the door stayed dark, fixed by making the keepalive log
in for itself inside one process. All three accounts deleted through
`/settings/delete` and confirmed dead; their game-side rows (if any)
are orphans by design.

Live. Visiting is the next increment and needs an open/friends/locked
decision first.

(Original note, kept:) Deploying this **does** touch civic (binary rebuild
+ `liquidgov` restart), unlike the last three rounds: daemon and civic
both rebuilt, civic must be up before the daemon's first lots poll
resolves anything, no nginx change.
