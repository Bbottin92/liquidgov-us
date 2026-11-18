# BUILD studio 5 — tuning, proposed (awaiting Brandon's sign-off)

Brandon: "go build slice 5," then, asked how to handle a slice with no target numbers named anywhere:
"I propose defaults, you approve." This is that proposal. **Not built yet.** `studio.md` names the
knobs: "cooldowns, yields, pixels per unit, charcoal time." There has been almost no real play since
today's deploy, so this is reasoned from design intent and the numbers already in the tree, not from
usage data — the honest alternative (wait for real friction, then tune) was on the table and Brandon
picked this one instead.

## What I looked at, and why most of it isn't changing

Slice 1's catalog wasn't picked casually — every duration went through an actual reference check
(`studio_refs.md`) before the table was frozen, and several numbers already reflect a deliberate,
documented compromise (charcoal's 8h from Cennini's own "left overnight"; verdigris capped at 7 days
from a real 1–3 week range, explicitly "the slow, expensive one, as it was historically"). Re-tuning
those without a real reason would undo work that was already done carefully. I looked for problems, not
things to change for the sake of it, and didn't find a case for touching:

- **Pixels per unit (200)**: a full 32×32 canvas in one pigment costs 6 units (two gather visits' worth
  of surplus, given yields below) — not free, not punishing.
- **Most gather cooldowns (10 min) and yields (3 units)**: since no recipe needs more than 1 of an
  ingredient (no stoichiometry, decision 2), 3 units already outpaces what a single batch needs. Fine as
  is.
- **Charcoal's 8 hours**: resolved by the reference check already; changing it now would be reopening a
  decision that was made with actual evidence, not guessing.

## Two things I do think are worth changing

**1. Bench slot cap: 3 → 5.**
Recipe durations span 30 minutes (carbon ink) to 7 days (verdigris), and there are 13 recipes across 6
tiers. A player working through the ladder can fill all 3 slots with just verdigris + iron acetate + one
more multi-day batch and then be locked out of the bench entirely — including the 30-minute recipes —
for most of a week. That's a real, structural bottleneck the wide duration spread creates, not a made-up
one. Raising the cap to 5 gives room to run a couple of long batches alongside short ones without
removing the cap's actual point (a player still can't queue unlimited batches at once). Pure pacing
knob — doesn't touch a single recipe's real-world grounding.

**2. Orchard keg cooldown: 1 day → 12 hours.**
Vinegar is the one input shared by two otherwise-separate tier-4/5 chains: iron acetate (3-day batch) and
verdigris (7-day batch). Gating the shared *input* to both of those at a full day on top of their own
multi-day durations compounds a wait that's already long, for no reason tied to the ingredient itself —
real vinegar at a keg doesn't stop being usable after 12 hours. Halving the cooldown shortens the
before-you-can-even-start wait without touching either recipe's own duration, which is the number that's
actually historically grounded.

## Considered, not changing

**Verdigris's 7-day cap.** This is the one number that genuinely feels long for a game with no push
notification and no reason to remember to log back in a week later. I'm not proposing a change to it:
studio.md is explicit that green is meant to be "the slow, expensive one, as it was historically," and
shortening it would erase the one place in the ladder where "real ingredients, roughly real time" is
actually felt. If this turns out to be where people quit the ladder once there's real usage to look at,
that's the number to revisit then — with data, not a guess.

## What actually changes in code

`commons-daemon/src/studio.cpp`:
- `start_batch`'s slot-cap literal `>= 3` becomes `>= 5`.
- `gather_spots()`'s `orchard_keg` entry: `86400000` (1 day) becomes `43200000` (12 hours).

No schema change, no new column, no catalog edit — both are hardcoded constants in the daemon, not data.
Client-side: nothing reads either number directly (the bench UI already just renders whatever
`makeable`/`open_batches` the server returns, and the keg's cooldown display already comes from the
server's own `next_ms`), so no client change is needed for either knob to take effect.

## Tests

`commons-daemon/tests/test_studio.sh` has a slot-cap test already (asserts refusal at the 4th open
batch) — it becomes a refusal at the 6th. The keg cooldown isn't separately asserted by number anywhere
in the suite today (only that a cooldown exists and blocks a second gather); no existing assertion breaks,
but I'll add one pinning the new 12h value directly, the same way the 10-minute spots are already pinned,
so a future accidental change is caught the same way this one is being made deliberately.

## Deploy

Same as always: build it, test it, hold for a literal `deploy`. Both numbers are already-deployed-code
constants, so shipping this is a normal daemon deploy (backup, sync, build, restart) — no schema
migration, no data to backfill.

---

**Say `BUILD studio 5` to build this as written, or tell me what to change first.**
