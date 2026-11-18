# Grok review of 7875d84 — accepted, not live

Civic `GET /internal/friends`: same-box, XFF → 404, cookie identity
only, `friends_of` + block filter, no `?username=`. Daemon RAM cache
15s, no table. `GET /presence/lots` registered **before** the prefix
route. Client: bound Homestead poll, guests never, named door still
solid. Civic file is in the commit (+26). Browser catch on
`FRIEND_LOT_DOORS` was real.

## Visiting stays shut

Correct. `grok_v6_answer.md` said this query lights lots 2–7; visiting
waits. No `open / friends / locked` column exists. Toast + untouched
`not_your_house` is the honest door. Do **not** open interiors this
round.

## Should-fix (not a deploy gate)

`FriendsCache` is keyed by `headers_raw` (whole head). The comment
says cookie. Same tab is stable enough at Square scale. Next patch:
key on the `Cookie:` line only.

Slot shuffle when a friend comes online: documented, fine.

## Deploy (only when Brandon says **deploy**)

**Civic, then daemon, then client.** No nginx.

1. Rebuild + restart `liquidgov`. Confirm
   `curl -s https://liquidgov.us/internal/friends` is **404** (plain),
   and loopback no-XFF no-cookie is `{}`.
2. Rebuild + restart `commons-daemon` (new `friends.cpp`).
3. Then `sync-client-to-droplet.sh`.
4. Civic `/` still 200. Bound + an online mutual friend: that Homestead
   door shows their name and still says visiting isn't open. Guest: all
   six "Closed.", zero lots polls.

Fail: unboarding a walkable friend house, client-first copy, skipping
the public-404 check, LGP1 from the daemon.
