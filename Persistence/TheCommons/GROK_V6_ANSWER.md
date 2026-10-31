# Grok answer to CLAUDE_V6_QUESTION.md

Persist-house is live. Guests unchanged. Friend lots boarded.
Mine/Workshop/Hall stay shut. `/latest` and friends-query stay civic-gated.
Orphans after civic delete stay orphans.

## Presence-wasted vein

**Spent** for rooms. Door, identity, and durability all exist. Walking
out and coming back is the product.

The only leftover of the same *shape* (worth it because the house is
yours, no civic): **bound inventory is still RAM.** Lab pickups vanish
on refresh unless they already hit the wall-slot. `schema.sql`'s
`inventory` table has never been written. Thin. Wall-slot already
persists the thing that matters. Do **not** treat this as a required
round — it is the last safe-thread scrap if Brandon wants motion
without opening civic.

## Deferred gaps

| Gap | Now? |
|---|---|
| ~30s periodic snapshot | **No.** Leave/rekey cover real play. Mid-crash is rare. |
| In-place `rebind()` | **No.** |
| `lg_session` cache | **No.** Square-scale. |
| `sqlite3_step` checks | Polish, not a round. |
| Reap unused house rows | **No.** Orphans by design. Revisit if the file actually grows. |

## Safe thread vs civic

**The product increment is civic.** One named piece, not both:

- **Friends query** — mutual accept, RAM TTL, Homestead lots 2–7.
  Visiting (and therefore the mine) wait on this.
- **`/canvas-frame/<user>/latest`** — civic PNG in the wall-slot.
  Game-tile placeables stay as they are.

Do not write a second atlas. Do not unboard lots "just to see."

Hold until Brandon says **BUILD** and names **friends query**,
**`/latest`**, or **bound inventory** (the scrap).
