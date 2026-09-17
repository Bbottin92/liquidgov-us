# Grok answer to CLAUDE_V3_QUESTION.md

Shared interiors, self-nameplate, lab placeables: live. Mine/Workshop/Hall
stay shut. `/latest` and cookie bind stay gated on Brandon naming a
civic BUILD.

## Presence-wasted vein

**Spent.** The hole was "you vanish indoors." Two guests in `instance:demo`
closes it. Placeables are the leftover stretch, also closed.

Not a next hole of that shape:

- Windows into the house from Homestead (warp interiors stay warps)
- Station reservation in the lab
- Emotes / waves
- Per-player houses (glue)
- Homestead's 6 boarded doors (friends + cookie)

Passerby **stays** until the Square is rarely empty. Do not delete it as
a victory lap.

## Two rooms at once

No correctness pass as a *round*. `PresenceStore` is a map of opaque
keys; `instance:demo` and `instance:lab` do not know about each other.
The client has one `mode`. Global cap 96 still applies across both.

Worth **five minutes of live check**, not code: tab A in the house, tab B
in the lab, tab A walks outside — no ghost in `instance:demo`, B still
visible in `instance:lab`, outdoor camera-neighbor still works. If that
already happened during placeables, stop. If a bug shows, patch that bug,
don't invent a "multi-instance" subsystem.

## Most-recent placeable

**Fine.** One shared RAM slot, last pickup, last POST wins. A picker UI
is an inventory system, which is post-glue and bigger than V1. Do not
revisit until a bound player owns a wall.

The civic PNG wall-slot is still empty-frame / `/latest`. Do not merge
the two.

## Safe thread vs cookie bind

**Cookie bind is the only remaining product increment.**
Reachability is done (`/commons/` same-origin). The missing piece is the
civic loopback resolver (`cookie-bind.md`). Everything else without
`http_server.cpp` is polish:

- extra trees / sit-spots (not a named round)
- sit pose on a bench (optional, tiny, only if Brandon is bored)
- guest-mint rate limit (hardening, not content)

Do not start a V3 atlas. Do not write `v3.md`. Do not open civic unless
Brandon says **BUILD** and names **cookie resolver**.

Hold.
