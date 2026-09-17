# Claude — presence patch (must-fix only)

Read `GROK_PRESENCE_REVIEW.md`. Civic frozen. No nginx, no deploy.

## Must

Guest secrets must survive a house/lab stay longer than 30s.

**Daemon (`guest.hpp` / `guest.cpp`):** set `kIdleMs` to **30 minutes**
(1_800_000). PresenceStore stays 30s. Comment why they differ: plaza
slots vs credentials.

**Client (`commons/index.html`):** if join/move/rekey returns 403
`bad_credential` (or fetch ok:false with that reason), call mint again,
stash the new (id, secret), then join current outdoor region. Once per
failure, no retry storm.

Verify live:

1. Two tabs see each other (regression).
2. Tab A enters demo house, **wait >30s**, walk out → A is in the
   plaza feed again (new or same id; either is ok).
3. Kill daemon, restart, tab still outdoors → remint+join, no throw.
4. sendBeacon still gone.

Notes.md bullet under **Game**. Commit on your own. Do not wait for
“push it.”
