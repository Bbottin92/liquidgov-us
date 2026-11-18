# Knock out the rest — four slices, not one gulp

Brandon: do the remaining pile. **One BUILD per slice.** Credits die
mid-file if you merge them. No workshop, Hall interior, ore, V2 map.

| # | Name | What | Touches | Deploy |
|---|---|---|---|---|
| **1** | Remaining tiles + sprite | `paintTile` for every id that is still a flat rect (tree, path, dirt, door, adit, hedge, flower, crate, railing, lamp, notice-post, interior wood). Tighten the existing 16×24 4×2 player (head/body/legs, still one look). Same palette, same bake. **No PNG file.** | client | client |
| **2** | Daemon janitor | `sqlite3_step` checks in `house.cpp`. 15s RAM cache of `lg_session` → username (same shape as `FriendsCache`, keyed on Cookie). | daemon | daemon |
| **3** | `rebind()` | Mid-session login promotes `g_…` → `u:` **in the current region** without waiting for a door. No ghost. | daemon (+ tiny client if a 403 still means remint) | daemon then client |
| **4** | `open` + orphans | Civic: same-box `GET /internal/blocked/<username>` (either direction, 404 public). Daemon: `open` policy + reap `players`/`placed`/`inventory` when civic says the user is gone. | civic, daemon, client | civic, daemon, client |

Slice 4 is the only civic. Do not start it until 1–3 are live.

`open` still needs that block probe. Do not ship B.

Orphans: only after a civic "this username does not exist" the daemon can
trust. Do not DELETE FROM players on a 404 from `/latest`.

## Slice 1 fail

A second tileset, `imagine` sheets, WebGPU, a walk cycle of 8, character
creator, day/night.
