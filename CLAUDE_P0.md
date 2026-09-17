# Paste this into Claude Code. P0 BUILD. Civic frozen.

You are continuing **The Commons**, sibling of LiquidGov.US.
Do **not** touch `liquidgov-platform/`, `liquidgov-core/`, LGP1, Juketube,
`/mail`, or `http_server.cpp`. Do not fake `context.md` / `state.md`.

`CLAUDE_CODE_CONTINUE.md` is civic. `CLAUDE_THE_COMMONS.md` was spec.
**This file is the job.** Grok reviews; he is not implementing P0.

```
git pull
```

Read: `Persistence/notes.md` (Game heading), `Persistence/TheCommons/v1.md`,
`art.md`, `atlas.md`, `BUILD_ORDER.md` P0 section.

## Constructive notes on your recap (85dd7ab / f2e5f44)

Accurate: four phases, first BUILD = P0 only, dual crop is the thing to
prove, P3 is the six quiet regions (Lab Yard waits for P2). Nothing to
un-recap.

Wrong closer: “say BUILD when you want Grok to build P0.” Brandon’s
instruction this round is **you implement P0**, Grok criticizes after.
Do not wait for Grok to scaffold.

Omitted from P0 (it’s in `BUILD_ORDER.md`): a **path ribbon** through
Square → Homestead → Creek → Shore. Terrain only — still no set pieces.
So P1 seams aren’t a surprise.

Square set piece is not a fountain. P0 has no set piece at all.

## P0 — ship this

Playable static client in a **new sibling folder** `commons/` at repo root
(vanilla HTML + JS + canvas). No React, no npm, no Node, no C++.

- Throwaway name prompt, then spawn in **the Square**
- One **72×80** tile array (16×16 px tiles), region AABBs from `atlas.md`
- Dual crop from `art.md`: 240×160 @3× if `width >= height`, else 160×240 @2×.
  Nearest-neighbor / `pixelated`. Follow player, clamp to the supermap.
- Arrows + WASD. **A/D = left/right on the screen** (hard fail if inverted).
- On-screen d-pad, thumb-reachable in portrait.
- Palette from `art.md` (grass/path/water/dirt/ink — not parchment world)
- Region **names as small ink labels**, not HUD chrome
- Path tiles continuing Square–Homestead–Creek–Shore; other regions grass
  (woods/water/stone tints ok if they’re still empty of props)
- World-edge collision. No doors, no passerby, no interiors, no interact
  prompts, no SQLite, no cookie, no Hall link

**Done when:** you can type a name, walk all 12 labels on a ~390px-wide
portrait viewport *and* a landscape desktop, A goes left, d-pad works,
crop swaps on resize. Record that (screenshot or a sentence in the report).

## Do not

- P1–P3 content
- React / Vite / the Grok App Builder stack
- Deploy to the droplet / rsync / nginx
- “Fountain,” quest UI, emoji chrome

Commit `commons/` + one dated `notes.md` bullet. Do not wait for “push it.”
Stop and report: files, what you verified (A/D, both crops), hash.
