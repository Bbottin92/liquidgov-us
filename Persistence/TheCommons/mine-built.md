# The mine (A: honest cave) — built, local-tested, not deployed

Brandon: "BUILD mine A" (`GROK_MINE_ANSWER.md`; my question was
`CLAUDE_MINE_QUESTION.md`). Client only. Civic, daemon, nginx and schema
untouched. No deploy.

## What ships (`commons/index.html`)

- **The adit is a door.** The bottom-centre tile of the Quarry Ridge adit is
  now `T_MINE_DOOR` (solid, prompt **Enter**, interactable `mineDoor`); the
  rest of the face is still rock. The outdoor sign stays and now reads
  **"Watch your step."** (the sign interactable carries its own `text`;
  fallback is still "Closed."). Sit-rock outside unchanged.
- **Interior, 16x12, `mode: "mine"`.** `INTERIOR_INSTANCE_KEY.mine =
  "instance:mine"`: the same shared-RAM pattern as demo/lab. Guests and
  bound players both walk in; no rows, no snapshot. Dirt floor, rock walls
  (`T_LOWWALL`), two sit-rocks (`T_BENCH`, the outdoor sit-rock tile), one
  lamp (`T_LAMP`, a lit flat-colour tile set into the back wall), one sign:
  **"No ore. That's honest."**
- **Nothing to do in it.** No ore, no bags, no stations, no place, no
  pickup. The interior tick skips `netPollPlaced()` for the mine (nothing can
  be placed there).
- **Leaving.** Enter with nothing in range, same as the demo house, and you
  land on the tile in front of the adit (`MINE_DOOR_TILE.y + 1`), so there is
  no re-entry oscillation. Standing beside the sign, Enter reads it instead
  (a real interactable always wins), same rule as the lab stations.
- **Passerby stays outside.** It is drawn only in `mode === "outdoor"`, like
  every interior.
- **Tiles.** Two new flat-colour ids (`T_MINE_DOOR`, `T_LAMP`) in the same
  palette scheme as `T_DOOR_HOUSE`. No second tileset, no sprite sheet.
- `MINE_DOOR_TILE` is declared beside `LAB_DOOR_TILE` (before the world is
  built during script load), the `FRIEND_LOT_DOORS` lesson.

## Tested (local, real browser, scratch daemon on 127.0.0.1:8765)

As a **guest**, real join through the name gate:

1. Walked up to the adit: prompt `[Enter]` on the door tile, region
   `Quarry Ridge`.
2. Enter (real key press): `mode: "mine"`, region rekeyed to
   `instance:mine`, spawned at the entry tile, screenshot shows the lamp,
   sign, both sit-rocks and the exit; **no passerby drawn**.
3. Daemon side: `GET /presence/instance:mine` listed the guest; `GET
   /placed/instance:mine` is `{"item":""}`.
4. Sign inside: toast "No ore. That's honest."
5. Enter away from the sign: back outdoors at the tile in front of the adit
   (10,68), rekeyed to `Quarry Ridge`, `instance:mine` presence empty again.
6. Outdoor sign: "Watch your step." Demo house still enters and exits
   (`instance:demo`); the workshop shutter is untouched.
7. No console errors.

Not exercised: two clients in the mine at once, and a bound player. Both
ride the generic shared-instance path demo/lab already use (`instance:` keys
are opaque to the daemon); nothing mine-specific touches either.

## Not here

- No deploy. Client-only when Brandon says the word, after Grok's review.
- No ore, no friends gate, no daemon/civic/nginx/schema change, no
  `http_server.cpp`. Workshop shutter still closed, Hall still a link.
