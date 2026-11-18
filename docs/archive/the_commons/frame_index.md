# Frame indexing strategies

Not a BUILD. Does not change `canvas_frame.md`: wall-slot is still one
PNG of **last frame (`N-1`)**, V1 still empty, no fetch in `commons/`.

Civic today: `GET /canvas-frame/<username>/<index>` with `index` in
`0..N-1` contiguous. A redraw **clears then re-appends**, so those
integers are only stable until the next drawing. Block / missing /
OOB all 404.

## Ways to name “the portrait”

| Strategy | URL / body | Verdict |
|---|---|---|
| Integer `0..N-1` | `/canvas-frame/u/3` | What exists. Fine if you already know `N`. |
| Hard-code `0` | `/canvas-frame/u/0` | First stroke. Wrong portrait. |
| 404-walk | `0,1,2,…` until 404 | Block looks like EOF. Banned. |
| Negative `atoi` | `/canvas-frame/u/-1` | `atoi("-1")` is −1; vector `[]` is UB or 404. Do not. |
| Count JSON then `N-1` | `GET /canvas-meta/u` → `{n:4}` then fetch `3` | Extra RTT. Race: owner redraws, `3` 404s. |
| **`/latest` alias** | `/canvas-frame/u/latest` | **One GET, always `frames.back()`**. Separate path token, not a fake int. |
| Blob id | `/blob/<id>` | Cache-perfect, not the public portrait contract. Game must not open LGP1. |
| Playlist | JSON of all frame URLs | For `/u/` CSS timelapse. Wall-slot is one picture. |

**Pick `/latest` for the wall.** Civic one-liner. Game never learns `N`.

## Races (why count-then-fetch loses)

1. Read `N=4`.
2. Owner hits “clear + draw” → now `N=1`.
3. Fetch `/3` → 404. Empty wall, even though a portrait exists.

`/latest` is one snapshot of `back()`. If they redraw during the GET you
still get *a* last frame, not a hole. `Cache-Control: private, max-age=300`
already on the route — wall-slot can live with five minutes of staleness.

Redraw **reuses** indices 0..N'-1 for **new** blob ids. Do not key a
cache on `(username, 3)`. Key on URL (`…/latest`) or `ETag` of the PNG
bytes if civic ever sends one (it may not today).

## What not to add on the game side

- Signed indices, sparse ids, or a second numbering scheme
- Animating `0..N-1` at 60 Hz in the house (that’s the profile page)
- Prefetching every frame “to have them” (droplet + 300s cache)

Sibling-era: civic grows `/canvas-frame/<username>/latest`. Daemon or
the img tag hits that with the **viewer cookie**. Until then, empty frame.
