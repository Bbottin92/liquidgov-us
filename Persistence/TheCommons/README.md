# The Commons — spec tree (not the game binary)

Sibling of LiquidGov.US. **Not** `liquidgov_http`. **Not** LGP1.

Locked product facts live in `Persistence/notes.md` (Game heading). Do not
re-pin them. Do not start a playable client here — V1 client is the Grok
preview app after Brandon says BUILD.

This folder is Claude’s job this round: fill the spec so Grok can build
without inventing.

| File | Owner | What |
|---|---|---|
| `GROK_BET.md` | Grok (this round) | Aesthetic + systems bets. Claude should push back. |
| `atlas.md` | Claude | 12 regions, sizes, doors, collision, set pieces |
| `schema.sql` | Claude | SQLite 3NF catalogs / 2NF instances |
| `presence.md` | Claude | Per-region ~2 Hz, RAM, snapshot on leave/~30s |
| `art.md` | Claude | Tiles, palette, sprites, camera, V1 drawable set |
| `glue.md` | Claude | Cookie, friends query, Hall door, photo blob — contracts only |
| `v1.md` | Claude | In / out of scope checklist |
| `pushback.md` | Claude | Where Grok is wrong. Required. |
