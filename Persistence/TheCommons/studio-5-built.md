# The Studio, slice 5: tuning — built, tested, NOT deployed

Brandon: `BUILD studio 5`, approving the proposal in `CLAUDE_STUDIO_5.md` as written. Two constants
changed in `commons-daemon/src/studio.cpp`. No schema, no catalog edit, no client change — neither
number is read by the client at all; the bench UI already renders whatever `open_batches`/`makeable`
the server returns, and a gather cooldown's display already comes from the server's own `next_ms`.

## What changed

- `start_batch`'s slot cap: `>= 3` -> `>= 5`. 13 recipes span 30 minutes (carbon ink) to 7 days
  (verdigris); three long batches alone could fill every slot and lock a player out of the bench
  entirely -- including the 30-minute recipes -- for most of a week. Pure pacing knob.
- `gather_spots()`'s `orchard_keg` cooldown: `86400000` (1 day) -> `43200000` (12 hours). Vinegar feeds
  two separate multi-day chains (iron acetate, 3 days; verdigris, 7 days); gating the shared *input* to
  both at a full day on top of their own durations compounded an already-long wait for no reason tied to
  the ingredient itself. Neither recipe's own duration changed -- this is the wait *before* a batch can
  even start, not the batch itself.

Explicitly not touched, and why: charcoal's 8 hours and verdigris's 7-day cap were both already resolved
by slice 1's actual reference check (`studio-refs.md`), not guessed. Re-tuning either now, with no real
usage to point at, would be undoing work that was done with evidence for no better reason than "it's the
tuning slice." Verdigris in particular is meant to be the slow, expensive tier, on purpose.

## Tests

`test_studio.sh` grew from 88 to **105 checks**:

- The slot-cap test now opens five batches (was three) before hitting `slots_full`, and asserts a sixth
  attempt fails with material still available (not `missing_inputs`) -- the same shape as before, just
  at the new number.
- A new section pins the keg's cooldown directly, the same way the 10-minute spots already were:
  first visit grants 3 vinegar; an immediate second attempt at the same clock is `on_cooldown`; 1ms short
  of 12h is still `on_cooldown`; 12h + 1ms succeeds and grants a second 3. This is coverage that did not
  exist before slice 5 at all -- the keg's cooldown had never been asserted by number, only that *some*
  cooldown existed.
- One pre-existing assertion (`ready_ms is start + 28800s`) had hardcoded the clock's value at that point
  in the file as a literal expression (`T0 + 600001`). Adding the keg test earlier in the same file's
  shared clock timeline shifted that value; fixed to compute it from the same variable the keg test
  itself sets (`KEG0 + 43200001`) rather than a second hardcoded literal that could drift again the next
  time something is inserted upstream of it.

Full suite: **26 files, 0 failed**.

**Mutation-checked**, each restored after confirming the right failure:
1. Slot cap reverted to 3 -- the five-open-batches assertion failed exactly where expected.
2. Keg cooldown reverted to 86400000 -- two assertions failed (the 12h release, and the vinegar count
   after it), both naming the right expectation vs. what actually happened.

## Limits, said plainly

Both numbers are still guesses in the sense that no real player has hit either limit yet — there is
close to zero usage since today's deploy. The reasoning behind each is sound (a structural bottleneck
from the duration spread; a compounding wait on a shared input), but "sound reasoning" is not the same
claim as "confirmed by data." If either turns out to still feel wrong once people are actually playing,
that is real signal slice 5 didn't have, and worth another pass then.

## Deploy

Not deployed. Both are already-compiled-in daemon constants, so this ships as a normal daemon deploy
(backup, sync, build, restart) whenever `deploy` is said -- no migration, nothing to backfill, no client
sync required for either change to take effect (though one will likely ride along anyway once the client
has other pending changes).
