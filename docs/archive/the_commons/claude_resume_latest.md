# Resume — Claude ran out of credits mid-/latest

Brandon screenshot 2026-09-18 ~16:27 PDT. Desktop session **"Persistence
notes review and /mail PWA fix"** hit the plan limit (auto-resume ~5:31
PM). Origin still has **bound inventory live** (`15248c9` / Grok live
check `7b3a519`). **Nothing of `/latest` is on GitHub.**

## What was in flight (local only — do not lose it)

Uncommitted on that machine:

- `liquidgov-platform/src/http_server.cpp` **+27** — civic
  `GET /canvas-frame/<username>/latest`
- `commons/index.html` **+66 / −6** — wall fetch + draw

Browser at `127.0.0.1:8890/?daemon=http://127.0.0.1:8765`: bound
`testowner` in their house. Wall still shows the **game tile** (`chalk`
label over the empty frame). Claude had just confirmed `owner: nobody,
ok: false` — 404 = no portrait, placed item still draws the old way.
That split is correct. Keep it.

In-flight stub replies were rewriting the house key. Freeze network
while asserting draw, or drop the stub once civic `/latest` is real.

## Finish this, not the other thread

The session title mixed **/mail PWA**. **Do not** fold Messages PWA,
notes-only civic UX, or `/mail` into this commit. `/latest` is the
named leftover (`grok_v6_answer.md`, `grok_inventory_live.md`). One
civic route, one client wall fetch. Civic `/mail` is closed pins.

## Contract (already locked — `grok_wallslot_answer.md`)

```
GET /canvas-frame/<username>/latest
```

- **Separate path token.** Existing atoi index handler must never see
  the word `latest` (`atoi("latest")` is `0` = first stroke = banned).
- Same PNG pipeline as `/<index>`: username → account (404 unknown),
  viewer cookie → `is_blocked` (anonymous skips), empty list → 404,
  ignore `cvm`, `Cache-Control: private, max-age=300`.
- Cache key is the **URL**, never `(username, N-1)`.
- Game: `img` (or `createImageBitmap`) from
  `"/canvas-frame/" + owner + "/latest"` with credentials so block
  applies. **404 → empty frame, not a broken image.** Placed lab item
  still draws if present (the screenshot).
- Do not probe `0,1,2…`. Do not `proxy_pass /canvas-frame/` to the
  daemon. Daemon never opens `cvf` / LGP1.

## After the code is coherent

1. Local verify: owner with frames → PNG on the wall; owner with none →
   404, chalk/item still there; blocked viewer → 404; guest house → no
   fetch.
2. Write `latest_built.md` + notes.md bullet. **Do not deploy.**
3. Push. Grok reviews. Deploy is civic then client (the route must exist
   before the wall hits it).

If the local diff is tangled with `/mail`, stash/split. Ship `/latest`
alone.
