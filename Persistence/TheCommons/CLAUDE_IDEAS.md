# Ideas from other games — proposed, not built

Brandon: "pull the best aspects of other relevant popular games and implement them." That is a direction,
not a spec -- too open to build straight from (the same reason every other feature here got its own
`CLAUDE_*.md` first). This is a survey: what actually transfers to a small, currency-free, combat-free,
top-down social/crafting game, checked against what the schema and the house rules ("no new art files, no
currency, no schema change unless it earns it") already allow, with a recommended build order.

Two things already on the table motivate part of this: Brandon's own observation that most Homestead doors
read as closed, and that personalizing your own house is thin (one wall slot, one portrait, one painting,
a door policy).

## What actually fits, and why

**Animal Crossing** is the closest relative this game already has -- a town you walk around, houses you
can enter, a friend/visit model, a collection you build over time (there: fossils/bugs/fish; here: the
notebook's discoveries). Its best ideas that transfer directly:
- **Placing your own decorations.** Animal Crossing's whole hook is arranging furniture you've collected.
  Checked against `schema.sql`: the `placed` table already has `x`, `y`, `rot90`, and `is_wall_slot` --
  today exactly one row per house ever uses it (the wall slot). The table was already built more generally
  than the client currently uses it. Extending "place one thing on the wall" to "place a few things you're
  holding, on your own floor tiles" is a client + a slightly less restrictive daemon check, not a new
  table.
- **A museum / collection you can see, not just read.** Animal Crossing's museum turns "I caught this
  once" into something you can walk past and look at. The notebook already has the data
  (`discoveries_of()`); it is currently a scrolling text list, not something IN the world.

**Stardew Valley**'s Community Center bundles are a shared, non-competitive project the whole town
contributes to -- which is a near-perfect fit for a game literally named **The Commons**. Right now
`discoveries` is 100% private: nobody can see what anyone else has found. A read-only aggregate ("14 of 21
recipes ever made, by 6 people; verdigris first made by X") costs one new daemon route (a `GROUP BY
recipe_id` over the existing table, no schema change) and turns solitary grinding into something that
visibly belongs to everyone, which is the game's own premise.

**Habbo Hotel / Club Penguin**'s best idea, stripped of everything that needs new art or a real economy,
is **wearing something you made**. Right now every player is the identical blue-shirt sprite
(`sprites.js`'s `BODY = "#3a5a7a"`, one constant for everyone). Recolouring your own shirt to any pigment
hex you've *discovered* -- a plain `fillStyle` swap on the same 16x24 atlas, zero new art, zero new
mechanic -- turns 13 pigments you already worked for into a second, visible payoff beyond the notebook
line. This is a small, honest version of avatar customization that respects the art-asset rule.

**Animal Crossing / most social games' "somebody's actually here" problem** -- your own live complaint.
The friend-lot doors aren't broken, they're honest (a closed door because nobody's occupying that lot right
now), but "Closed." reads as dead, not "come back later." A one-line wording and (if cheap) a dimmer tile
tint costs almost nothing and directly answers what you asked about last turn.

## Rejected, and why

- **Currency / shops / Nook's Cranny.** Already ruled out for this game (no economy exists, none is being
  added now either) -- confirmed twice this project, most recently for the backpack.
- **Combat, mining hazards, monsters (Stardew's mines, Terraria).** Wrong tone entirely for a game about a
  shared crafting square; nothing here should punish or threaten a player.
- **Seasons / rotating live-events calendar (Animal Crossing's real-time seasons and holidays).** Real
  ongoing content and time-based logic a solo-maintained game would have to keep feeding forever. Skip.
- **A separate fishing/bug-catching minigame (Stardew, Animal Crossing).** The Studio's gather spots
  already ARE this genre's "walk up, collect a resource" loop. A second, unrelated minigame would be new
  surface area duplicating a loop that already exists, not filling a gap.
- **Full avatar customization (hats, outfits, pets) via new art.** Would break "no new art files." The
  pigment-recolour idea above gets most of the payoff (something visibly yours) without breaking the rule.

## Recommended build order

1. **Friend-door wording** ("No one's home." instead of "Closed.", maybe a dimmer boarded tint) -- the
   cheapest, most direct answer to what you already flagged. A few lines, one file.
2. **Wear a pigment** -- recolour your own sprite's shirt to any discovered pigment hex. Small, self-
   contained, gives the notebook's 13 pigments a second payoff. No schema change.
3. **Place more than one thing in your own house** -- extend `placed` beyond the single wall slot to a
   handful of floor-tile decorations from your own inventory. The bigger of the three: touches the daemon's
   placement checks, not just the client. Most directly answers "how do you customize your own space."
4. **A shared discoveries board** -- read-only aggregate across all players' `discoveries`, framed as the
   Commons's own collection rather than a leaderboard (no ranking, no competition -- just "this exists, and
   here's who found it first"). One new daemon route, no schema change, but its own client screen.

Each would get its own `CLAUDE_*.md` at build time the way every other feature here has, with real numbers
in place of "a handful" and "a few decorations" -- this doc is the survey and the order, not the spec for
any one of them.

---

**Tell me which one to start with, or say `BUILD <name>` for one of these once it has its own doc, or tell
me to reorder/drop any of them.**
