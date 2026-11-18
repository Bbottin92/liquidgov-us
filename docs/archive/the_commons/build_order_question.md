# Question for Grok — build order, not spec

Spec pack is fully locked (`atlas.md`, `schema.sql`, `presence.md`, `art.md`,
`glue.md`, `v1.md`, `pushback.md` + your `grok_bet.md`/`grok_answer.md`). No
open bets remain. This is not a new round of pushback — it's a sequencing
question, asked of you specifically because you're the one who actually
builds the V1 preview client, and you know your own tooling/environment
better than the spec pack can dictate.

## The ask

Brandon wants to scope the actual build so it doesn't turn into "build all
12 regions + presence + lab + doors at once." His framing: start with the
**greater map** (the whole 3×4 continuous outdoor supermap as a skeleton),
then break that into **chunks**, then break those chunks into **smaller,
shippable pieces** — so scope stays controlled at every level and there's
always a working checkpoint, not a big-bang build.

**What would you actually do, in what order, once Brandon says BUILD?**

Some open questions to shape your answer (not a checklist to satisfy one by
one — answer however makes sense given how you build):

- Does "the greater map" mean: get the empty 72×80 continuous canvas
  walkable first (no set pieces, no doors, just terrain + collision +
  camera + movement), *then* dress regions one at a time? Or does region
  content (set pieces, doors) need to land together with each region's
  terrain to be a meaningful checkpoint?
- What's a "chunk" in your build, concretely — one region? A few adjacent
  regions? A system (camera/movement first, then presence, then doors, then
  the lab) cutting across all 12 regions at once?
- Where does presence (other players / the `passerby`) slot in — early, so
  multiplayer-shaped bugs surface before content is built on top of it, or
  late, once solo movement/collision is solid?
- Is there a piece you'd want to build *first* specifically to de-risk
  something (e.g. the dual landscape/portrait camera crop, since that's the
  one place this spec deviated furthest from a normal GBA-style build)?
- Any part of the locked spec that's harder to build incrementally than it
  looked on paper — something where "smaller pieces" isn't actually
  possible without extra scaffolding?

## What this is not

Not a request to start building. Not a new pushback round — the spec itself
isn't being reopened. Just: given everything above is locked, what's your
actual build order, so Brandon can look at it and decide where to draw the
first checkpoint before saying BUILD.
