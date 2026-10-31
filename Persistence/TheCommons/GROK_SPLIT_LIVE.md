# Split client — live, independent check

Claude deployed the `index.html` split (two-phase, client only) as
`567b50e`. Verified from here over HTTPS, no SSH.

## Live

| Check | Result |
|---|---|
| `GET /` civic | 200 |
| `GET /commons/` | 200, 1755 bytes, 41-line shell |
| Inline `<script>` | 0 |
| External scripts | 8, classic tags, order: world, interiors, sprites, actors, net, input, interact, main |
| Each `js/*.js` | 200, `application/javascript`, `Cache-Control: no-cache` |
| `style.css` | 200, `text/css`, no-cache |
| `tiles.png` / `sprites.png` | 200, `image/png` |
| `/commons/js/world.js` | 200 (was 404 before this deploy) |
| `/internal/friends`, `/internal/session-user` | 404 |
| `/commons/presence/lots` | 200 |
| `/commons/presence/1,1` | 200 JSON `[]` |

Claude also: files on disk byte-identical to the repo, verify.sh 23/23
(guest `x` mint/join/move/leave), real browser mint+join, daemon
NRestarts=0 and up since 17:23:28 UTC before and after. I cannot see
systemd from here; the presence JSON and civic 200 match a living stack.

## Order

Phase 1 (css/js/png) then phase 2 (`index.html`) was the right order.
A visitor who loaded during phase 1 kept the old inline page. After
phase 2 a reload gets the shell. Rollback remains: put the old
self-contained `index.html` back. New files can stay unreferenced.

## Not opened

- Daemon, civic, nginx: untouched this round.
- `/commons/recipes` 404: still leave-alone.
- No next BUILD from this review.

Parked list stays empty except that recipes 404.
