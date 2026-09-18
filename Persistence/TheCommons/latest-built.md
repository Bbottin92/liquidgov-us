# /latest — built, local-tested, not deployed

Brandon: "BUILD /latest" (`GROK_V6_ANSWER.md`: "`/canvas-frame/<user>/latest`
-- civic PNG in the wall-slot. Game-tile placeables stay as they are").
This is the **third civic edit**, and the shape follows
`GROK_WALLSLOT_ANSWER.md` and `glue.md`'s contract.

## Civic (`http_server.cpp`), one branch, ~20 lines

`GET /canvas-frame/<username>/latest` serves the account's newest saved
frame (`frames.back()`). It is a **separate branch registered ahead of**
the existing index route, and the index route is byte-identical: that
handler `std::stoi`s the last segment, so `latest` must never reach it
(today it just 404s; any looser parse would read as frame 0, the first
stroke). Copying the index route's checks exactly -- unknown or invalid
user, the *viewer's* block check (an anonymous request skips it),
no frames, missing blob -- every one is a plain 404, which the game reads
as "empty slot" rather than a broken image. Same PNG and
`Cache-Control: private, max-age=300` headers. Read-only.

No civic path to LGP1 is new (it uses the same `canvas_frames` accessor
the profile page already does), the daemon never touches it, and there is
no nginx change: `/canvas-frame/` already falls through to civic's
`location /`, and it is deliberately **not** proxied to the daemon
(`nginx.md`).

## Client

In a house -- yours, or a friend's you are visiting -- the owner's real
profile drawing hangs in the wall slot: a 3x2-tile frame centered on the
slot, the image fit inside it. `img.src = "/canvas-frame/<owner>/latest"`,
same origin as the page, so the browser sends the viewer's own cookie
and civic's block check applies. The owner is taken from the instance key,
so a visit shows the *host's* portrait. The shared demo house and the lab
have no owner and keep the empty slot. Any failure (no drawing, a block,
no civic on a local dev page) just means no portrait.

**Placed items and the portrait share the one slot.** With no portrait
the placed lab item draws exactly as before. With one, the item shrinks to
a badge on the frame's corner with its label underneath, so neither hides
the other. That layout is my call; it is easy to change.

## Tested (local)

Civic, real accounts, **real uploaded PNG frames** (`POST
/me/canvas/frame`):
- Unknown user, an invalid name, and a user with no drawing: 404.
  `/canvas-frame/latest` and `/canvas-frame//latest`: 404.
- One frame (red): `/latest` is red. A second frame (blue): `/latest` is
  now **blue**, index 0 is still red, index 1 blue -- so it is the last
  frame, not frame 0.
- The index route is unchanged: out-of-range, non-numeric and negative
  indexes all still 404.
- Headers: `image/png`, `Cache-Control: private, max-age=300`.
- A viewer who blocked the artist gets 404; an anonymous request and an
  unrelated user still get the image.
- Artist clears the canvas: `/latest` returns to 404.

Client, in a real browser on a page that serves a real PNG at the same
origin, with only the house join/rekey answer stubbed (no civic cookie on
a local page): the drawing hung in a frame on the wall; with a placed item
it shared the slot (frame plus label); the shared demo house showed no
portrait; a house whose owner has no drawing showed the original empty
slot with the item drawn the old way. My stub initially rewrote the house
key mid-check and produced a confusing reading; I froze networking and
re-checked deterministically rather than trust it.

Not separately exercised locally: the visit path (it uses the same
owner-from-key code with the visit key). Production verification of a real
visit covers it.

## Not here

- No picker, no editing the portrait from the game -- it is read-only.
- The 5-minute browser cache means a redrawn canvas can take up to that long
  to show on the wall.
- `mine`, `open`/`locked`, and the parked snapshot/rebind/cache items are
  unchanged.

No deploy step run. Deploying this touches civic: **civic first**
(rebuild + restart `liquidgov`, then check `/latest` answers), then the
client. No daemon change and no nginx change.
