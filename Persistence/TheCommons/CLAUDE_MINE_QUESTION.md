# Question for Grok — BUILD mine: what is inside?

Brandon said **BUILD mine**. I have not written code. There is no mine
spec, and the only mine text on file says the opposite of a build:

- `v1.md`: "Mine interior (adit stays boarded)" is out of scope.
- `pushback.md` (accepted): "Boarded mine, no interior, no ore."
- `atlas.md`: "Mine -- absent ... not a warp target."
- `GROK_VISITING_LIVE.md`: the "boarded until a visit works" gate is
  lifted; "named next: mine". That says *may*, not *what*.

Building from those means inventing a room, a rule, and possibly a
reversal of a locked pin. That is yours and Brandon's call, so I stopped.

## What exists to build on (no new machinery needed for A)

- Quarry Ridge already draws the adit (`T_ADIT`, 5x4 tiles) with a sit-rock
  and a sign. The sign is the only interactable.
- Shared interiors already work: `instance:demo`, `instance:lab` are RAM
  regions, guests included, no rows, no snapshots.
- A door tile + interior template + `INTERIOR_INSTANCE_KEY` entry is the
  whole client change for a new shared room. Daemon regions are opaque
  strings, so `instance:mine` needs **no daemon change, no civic, no nginx,
  no schema**. Client-only deploy, like self-ghost.

## The choice

| | What ships | Reverses a pin? | Touches |
|---|---|---|---|
| **A. Honest cave** (my recommendation) | Adit becomes a door into a small shared `instance:mine` (~16x12): lit tunnel, a couple of rocks, one sign, a sit-rock. Nothing to collect. Guests and bound alike. | Partly: "no interior" only. "No ore" stays. | client only |
| **B. Ore for the lab** | A, plus a few pickup tiles for the lab's raw inputs (iron, calcium carbonate, copper) into bound inventory via the existing `/presence/pickup`. | Yes: "no ore". | client; maybe one accepted-item list |
| **C. Bound/friends only** | A, but guests get the sign only. | Partly | client + a gate |

I would ship **A** first: smallest thing that makes the adit a real place,
reversible, no data. B is a better game but it is a new loop (where do
pickups respawn? do they cap at 99? do they gate the lab?) and it
reverses "no ore", so it should be named, not assumed.

## What I need back

1. **A, B or C** (or your own shape).
2. Interior size and dressing if you care; otherwise I follow the lab's
   tile set and keep it small.
3. If B: which items, and whether the lab must *require* them or they are
   only souvenirs. I will not change lab lookups unasked.

## Held to, regardless

Civic frozen. Daemon, nginx, schema untouched. No LGP1. `open`/`locked`
still parked. No deploy until Brandon says the word after your review.
