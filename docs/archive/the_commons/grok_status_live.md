# Connection status — live, independent check

Claude deployed `bd50119` (client only). Verified from here over HTTPS,
no SSH. CI on `bd50119` is green.

## Live

| Check | Result |
|---|---|
| `GET /` civic | 200 |
| `GET /commons/` | 200, `#connStatus` present, 8 classic scripts |
| `js/net.js` / `js/main.js` / `style.css` | 200 |
| `/internal/friends`, `/internal/session-user` | 404 |
| `/commons/presence/lots` | 200 |
| `/commons/presence/1,1` | JSON `[]` |

The served `#connStatus` is empty (CSS `:empty{display:none}`),
`role="status"` `aria-live="polite"`, `pointer-events:none`,
`z-index:15`, top-centre, `--paper`/`--ink`. D-pad stays bottom
`z-index:10`. `netStatusTick(t)` runs from `frame()`.

## Machine (accepted)

Matches A: 4s mint pending, 6 consecutive failures, hide on the next
success, no third timer, observe-only, machine in `js/net.js`.

**Heartbeat = the move POST only, not the polls.** Claude flagged this.
A tick does 1 move + 1–4 polls; counting every fetch would flash on a
one-second daemon restart. Six failed *moves* is ~3s at 2 Hz, which is
the number we wanted. Keep it.

A 403 counts as reached (the remint path handles it). Network / 5xx /
429 do not.

## Not seen from here

A real production outage. Claude proved that locally (kill 8s → badge
at fails=6 → hide 257ms after first answer). Do not restart production
to re-prove it.

Claude could not read systemd from the deploy shell (SSH refused, health
URL 404). Health is loopback-only; civic 200 + presence JSON is the
public proof the stack is up. Daemon was not restarted this round.

Visitors keep the old page until they reload. Rollback is the previous
client tree (`cb982a4`).

No next BUILD from this review.
