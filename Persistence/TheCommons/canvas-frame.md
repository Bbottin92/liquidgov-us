# Canvas-frame index limits

Not a BUILD. Wall-slot stays an empty frame in V1 (`glue.md`).
Do not fetch `/canvas-frame/` from `commons/index.html` this week.

## What the store actually guarantees

LGP1 tag `cvf` (`file_store.hpp`):

```
cvf  <account_id>  <frame_index>  <blob_id>
```

A new drawing **`clear_canvas_frames` then `add_canvas_frame` in draw
order**. Comment in the header: **index is always `0..N-1` contiguous**
for whatever is currently stored. Holes are not a thing.

`add_canvas_frame` is `push_back` + flush. **The store itself has no
max N.** If HTTP POST `/me/canvas/frame` caps uploads, that lives in
`http_server.cpp`, not here. Do not invent a number (8, 24, 32) until
that handler is read.

`cvm` is the mode:

- absent or `0` — **static: last frame** (`N-1`) as the profile image
- `1` — **timelapse: CSS loops `0..N-1`** on `/u/<username>`

Empty `canvas_frames(id)` → no drawing → the route 404s every index.

## What the HTTP route does (already in `glue.md`)

`GET /canvas-frame/<username>/<index>` → `image/png`

- Public (same as `/portrait/`). Cookie optional.
- If the *viewer* cookie is present and either side blocked the other,
  that frame 404s. Anonymous GET skips the block check.
- Out-of-range index, unknown user, blocked, missing blob: all **404**.
  You cannot tell them apart. That is why probing `0,1,2,…` until 404
  is wrong — a block looks like “end of animation.”

`index` is a **non-negative integer path segment**. `latest` is not a
number. If civic `atoi`s the segment, a `/latest` alias must be a
**separate path** (`/canvas-frame/<username>/latest`), not a fake index.

## What the wall-slot should request (sibling-era)

One PNG. The Commons wall is not the CSS timelapse.

| Profile mode | Wall-slot |
|---|---|
| static | frame `N-1` |
| timelapse | still `N-1` (last snapshot), not a loop |
| none | empty frame (today’s V1 fake) |

Address it as **`/canvas-frame/<username>/latest`** (civic one-liner:
serve `frames.back()`). Until that exists, do **not** probe. Do not
hard-code `0` (that is the *first* stroke, often a blank-ish start).

Forward the **viewer** cookie so `is_blocked` applies (`GROK_GLUE_REVIEW.md`).

## Fail

- Game 404-walk `i=0,1,2,…`
- Assuming a max of 8/24/32 frames
- Using frame `0` as “the portrait”
- Animating the wall-slot at 60 Hz from N PNGs
- Opening LGP1 to read `cvf` from the game process
