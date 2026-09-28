# Wear a pigment — proposed (awaiting Brandon's sign-off)

Brandon: `BUILD wear a pigment`, the second pick off `CLAUDE_IDEAS.md`'s build order. That doc named the
idea ("recolour your own shirt to a discovered pigment") but didn't settle how it's picked, whether other
players actually see it, or where it's stored -- real decisions, same reason every other feature here got
its own doc before code. This one touches the daemon (a small schema addition and a network field), not
just the client, so it's more than a wording tweak.

## Scope, plainly

Every player is the identical blue-shirt sprite today (`sprites.js`'s `BODY = "#3a5a7a"`, one constant,
`DRAW` for everyone). This lets a **bound** player recolour their own shirt to any pigment they've
unlocked, and -- the actual point, per Habbo/Club Penguin's own version of this -- **other players see it
too**, not just you in your own client. No new art file: the body is a flat-filled rectangle in the sprite
atlas already; recolouring it is a `fillStyle` swap.

## Decision 1 — what's wearable, and by whom

Reuse the daemon's own fixed `PIGMENTS` table (`studio.cpp`, 13 entries, already mirrored client-side as
`STUDIO_PIGMENTS`) rather than inventing a second list. Same unlock rule the easel's palette already uses:
indices 1-4 (charcoal's four greys) are always wearable -- they're `unlock_recipe == nullptr`, "always
legal," the same reason a fresh player can still paint with them before collecting anything. Indices 5-13
need that pigment's own recipe in the player's `discoveries`.

**Bound only.** A guest never has a `discoveries` row (studio state is `null` for a guest), and
`CLAUDE_STUDIO_3.md` already draws the line at "no guest calls gather/bench/mix/collect/studio" -- this is
the same boundary, not a new one. A guest keeps the default shirt; the button doesn't fetch for them at
all (a toast, same "Log in on LiquidGov to..." family the notebook and bench already use for guests).

## Decision 2 — persistence and letting other players see it

**A new nullable column, `players.shirt_pigment_index INTEGER`,** migrated the same idempotent way
`visit_policy` was (`db.cpp`'s `migrate()`: `if (!has_column(...)) exec("ALTER TABLE ...")`) -- direct
precedent, not a new mechanism. `NULL` = the default blue, same as today.

Occupants are RAM-only (`presence.hpp`'s `Occupant` struct: id/display_name/x/y/facing/last_update_ms,
"never a SQLite table"). A worn colour has to ride the same path `facing` already does to be visible to
anyone else: `Occupant` gains an optional `shirt_hex` field, sent as the resolved hex (not the index -- the
client should never have to cross-reference a table to render another player, just paint the string it's
given), set from the DB at join/rejoin for a bound player and left empty for a guest. `occupants_json()`
grows one more optional field, omitted when empty (matches how sparse fields already serialize elsewhere
in this file).

**A new route, `POST /presence/wear { index }`** (or `{}`/`null` to go back to default): validates the
index is actually unlocked for that player (same check as above), writes the column, and updates the
*current* in-memory `Occupant` row immediately if they're present in a region -- so a friend standing next
to you sees it change without needing to leave and re-enter. Mirrors `netSetHousePolicy`'s own
immediate-apply shape.

## Decision 3 — rendering another player's colour

No new atlas per colour (13 pigments x every occupant on screen would be wasteful and pointless -- the
body is one flat rectangle, not a texture). After the normal sprite `drawImage` blit, if that occupant has
a `shirt_hex`, paint one small `fillRect` over just the body's destination sub-rect (the same fixed offset
`drawFigure` already uses: `ox+4, 8, 8, 10` relative to the sprite origin, translated to on-screen
coordinates for that draw call). Applies identically to your own player and to every other occupant drawn
from the same atlas -- one small function, called from wherever `blitTallTile`/the actor draw loop already
positions each figure.

## Decision 4 — where you pick it

**A "Wear" button in the easel's own `#easelActions` row** (next to Save/Close), applying whichever
palette swatch is currently selected for drawing (`easelHex`) as the worn colour too. This is the smallest
new-surface option: the easel already lists exactly the unlocked-pigment swatches this feature needs
(`easelPalette`, gated the same way), already tracks which one is selected, and already gates itself to
"log in to use this" for a guest -- nothing new to build for the picker itself, one button and one route
call. A second, small text control ("Wear default") reverts to the plain blue.

Rejected: a separate wardrobe/picker screen -- would duplicate the easel's own palette list and its unlock
logic for no real gain; the notebook's own discovery lines -- charcoal is wearable before it has ever been
collected (no notebook line for it yet on a fresh account), so the easel's "always shows the always-legal
four" list is the one that's actually complete from minute one.

## Out of scope, said plainly

No dye recipe, no new item, no currency. No hat/hair/pants -- one shirt colour, matching exactly what the
sprite atlas can flatly recolour without new art. No guest access (see decision 1). No history of past
colours worn.

## Tests

- `test_studio.sh` (daemon): the new route rejects an unheld/undiscovered index (`403`/`409`-style
  refusal, matching this project's existing refusal shape), accepts any of 1-4 with zero discoveries,
  accepts 5-13 only after that recipe is discovered, persists across a rejoin (read back at bind), and a
  friend standing nearby sees the occupant's `shirt_hex` change without leaving the region.
- `client_files_unit.js`: the Wear button and its guest-toast path exist; a guest never calls the new
  route; the body-recolour function exists and is called from the actor draw path.

## Deploy

Not client-only this time -- a schema migration (idempotent, additive, no backfill needed since `NULL`
already means "default") plus a daemon route plus the occupant JSON change, then the client. Same
two-step `deploy` / `deploy --go` either way.

---

**Say `BUILD wear a pigment` to build this as written, or tell me what to change first.**
