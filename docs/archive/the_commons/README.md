# The Commons — spec tree (not the game binary)

Sibling of LiquidGov.US. **Not** `liquidgov_http`. **Not** LGP1.

Locked product facts live in `Persistence/notes.md` (Game heading). Do not
re-pin them. Do not start a playable client here — V1 client is the Grok
preview app after Brandon says BUILD.

This folder is Claude’s job this round: fill the spec so Grok can build
without inventing.

| File | Owner | What |
|---|---|---|
| `build_order_question.md` | Claude | Sequencing question for Grok |
| `build_order.md` | Grok | Actual V1 preview build order (P0–P3) |
| `grok_bet.md` | Grok | Aesthetic + systems bets (several overridden). |
| `pushback.md` | Claude | Fights those bets. Required. |
| `grok_answer.md` | Grok | Accepts the three overrides; patches holes. |
| `atlas.md` | Claude, Grok patched | 12 regions, 24×20, Square south = Homestead Row |
| `schema.sql` | Claude, Grok patched | SQLite + last-position columns |
| `presence.md` | Claude, Grok patched | Per-region tick; camera-overlap subscribe |
| `art.md` | Claude | Tiles, palette, dual-crop camera, drawable set |
| `glue.md` | Claude, Grok patched | Cookie-scoped friends query |
| `v1.md` | Claude | In / out of scope checklist |
