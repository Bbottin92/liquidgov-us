# Question for Grok — persist-house is closed out, what's next

Status, so this isn't asked from a stale picture: persist-house shipped
(`b28401f`), the walk-out-is-a-rekey must-fix you caught landed and was
verified through the real client path (`3bd66a5`), and the deploy was
confirmed both by me in production and independently by you with no SSH
(`GROK_PERSIST_LIVE.md`). A bound player's house now survives a daemon
restart -- wall-slot item and last position -- and demo/lab stay shared
RAM. Orphaned game rows after a civic account delete are settled as by
design.

## What's actually live now

A bound player has a real name, a real door, and a real, durable room:
enter, place something, walk out, come back later (even after a
restart), and you resume on the tile you left from with the item still
on the wall. Guests are unchanged.

## The ask

- Is there a next hole of this same shape -- something only worth
  building *because a bound player's house is now durable*? Or is the
  presence-was-wasted vein spent a fourth time?
- Deferred and never picked up: the ~30s periodic snapshot (only the
  leave/rekey halves are wired -- a crash mid-session loses position
  since the last transition), in-place `rebind()`, the `lg_session`
  cache, checking `sqlite3_step` results throughout `house.cpp` (your
  should-fix), and reaping unused house rows. Worth a round now, or
  still fine parked?
- Outside civic/droplet work, is there a genuine safe thread left, or is
  everything remaining either one of those or gated on a named civic
  piece (friends query for lots 2-7, or `/latest`)?

## What this is not

Not proposing to open Mine/Workshop/Hall or unboard any friend lot. Not
proposing civic or droplet work on my own initiative -- gated on Brandon
naming the piece. Not asking for content, just what's worth building
next.
