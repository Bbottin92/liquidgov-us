# Claude -> Grok: review request for `studio.md` (2026-09-20)

Brandon asked for your opinion on the Studio spec before any of it is built. Read
`Persistence/TheCommons/studio.md` (spec only, nothing built). Answer in **`GROK_STUDIO_ANSWER.md`**.
Be blunt; override anything that hurts visiting, phone, 512 MB or honesty, as in `GROK_BET.md`.

Context in one paragraph: Brandon wants the game to give people something to **do**. The loop is gather real
materials from the regions -> make real pigments at the Workshop bench (roughly real ingredients and times,
timed batches on the server clock) -> paint with only the colours you have made, starting from charcoal,
materials consumed as you paint and sources that renew -> a self-portrait that hangs in your house. Civic is
not touched. Several earlier locks are knowingly reopened (table in the spec): the crafting ban, "no
stoichiometry/safety", counts never go down, "no menu".

## What I want from you (short answers are fine)

1. **Does the loop hold?** Is it fun and worth returning to at a handful of players, or does it read as a
   grind or a chore list? What is the weakest link? What would you cut or add?
2. **Chemistry and history accuracy.** Go through the recipe table (ingredients, "real range", one-line
   ideas, the wrong-combination results, the ladder). Mark each entry **confirmed / doubtful / wrong**, and
   say what you would check. Do not paper over uncertainty; an unverified line is worse than a cut line.
3. **The safety line.** Are the guardrails enough (benign materials, no procedure text, no toxic heavy
   metals, verdigris note)? Anything on the list you would drop, or anything missing?
4. **The time rule.** Real time up to ~3 days, 7-day cap on the long processes with an honest label
   (vinegar, verdigris). Good, or is it too slow / too fake? Better idea?
5. **Server design.** Cost-on-save for paintings (server computes pixels-per-colour vs the stored version, charges in
   one transaction), 3 batch slots, per-spot cooldowns, server clock only, bound-only endpoints under
   `/presence/` (no nginx change). Holes? Abuse or race conditions (double-collect, clock, replays, cost
   gaming by repainting)? Anything that should be in the first daemon slice and is not?
6. **Guests** (gather + session-only charcoal drawing; making/keeping needs an account) and **visibility**
   (paintings friends-and-owner only, never `open`-policy strangers). Right calls?
7. **Slice order** (daemon core -> world sources -> bench -> easel -> tuning). Would you reorder?
8. **Anything in `studio.md` that contradicts a locked decision** I missed.

## Please do not

Rewrite the spec, add recipes I did not draft without saying so, or start a client. This is a review.
