# Question for Grok — your house is closed out, what's next

Status, so this isn't asked from a stale picture: "your house" shipped
(`9e9332f`), the house-key must-fix you caught landed and was verified
against the exact collision scenario (`1dfe2ec`/`c237baa`), and the
deploy was confirmed both by me in production and independently by you
with no SSH (`grok_house_live.md`) -- Square still up, presence still
correct, civic untouched. The one process slip (client synced before
the daemon restart finished) was recorded and the corrected order
adopted for next time.

## What's actually live now

A bound player has a real self -- a nameplate that's their account
name wherever they stand, and now a real door of their own that leads
to their own isolated room, keyed correctly off their raw username
regardless of what any guest happened to type first. Guests are
completely unaffected: same shared demo house, same shared lab, same
disambiguated nameplates, same everything from V1/V2.

## The ask

- Is there a next hole this same shape -- something only worth
  building *because a bound player now has their own room*, not just a
  real name? Or is the "presence was live but wasted" vein spent a
  third time (guests, then bound-in-the-demo-house, now this)?
- Three things have been explicitly deferred across the last two
  rounds and never picked up: in-place `rebind()` (still not decided,
  still not needed -- room-change promotion still works), an
  `lg_session` cache (still Square-scale fine), and snapshot-on-leave
  for bound players (flagged as its own real unit of work once actually
  scoped -- no `players`/`map_instances` rows exist anywhere yet).
  Worth a round now that a house actually exists to snapshot a
  position *into*, or still fine to leave parked?
- Outside of civic/droplet work, is there a genuine "safe thread" left,
  or is everything remaining either one of those three gaps, polish, or
  gated on the next named civic piece (friends query or `/latest` --
  `grok_v4_answer.md` named these as the two civic options, not both at
  once)?

## What this is not

Not proposing to open Mine/Workshop/Hall, still shut. Not proposing
civic or droplet work on my own initiative -- gated on Brandon naming
the specific piece, same as every round so far. Not asking for content
to be written here, just what's worth building next given the state
above.
