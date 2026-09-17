# Paste this into Claude Code. Spec pack only. Civic site is frozen.

You are continuing **LiquidGov.US + sibling game The Commons**.
Do not invent a new civic product. Do not merge the game into
`liquidgov_http` / LGP1. Do not rewrite the site in React/Node.
Do not treat the Grok App Builder scaffold as the civic site.

`CLAUDE_CODE_CONTINUE.md` is the **civic** continue file. **This file
is the job this session.** Civic slices A/B/C stay closed. Do not
“pick the next civic slice.”

## Before anything else

```
git pull
```

Then read, in order:

1. `Persistence/README.md`
2. `Persistence/notes.md` — especially **Shared pad** and **Game — atlas locked**
3. `Persistence/TheCommons/README.md`
4. `Persistence/TheCommons/GROK_BET.md`

Do **not** create `Persistence/context.md` or
`Persistence/Projects/LiquidGov.US/state.md`. Those exist in an old
Grok sandbox and were never pushed. Do not fake them.

List the files you opened. Then do the one slice below. Stop and report.

## What this is

Two programs, one person.

- **LiquidGov** (you, droplet, later): login, friends, mail, votes,
  profile canvas, Juketube, civic pages. Ordinary HTML from C++.
  Live: https://liquidgov.us — do not reopen closed pins.
- **The Commons** (Grok preview V1, droplet sibling later): tiles,
  positions, houses, inventory, lab, visiting. Own process, own files.

Glue only: session cookie (`lg_username` as a reference, never a second
login), friends as a **query**, Hall door → live `/proposals`, photo
→ civic blob+feed. Retention is **visiting**, not mining.

## Locked (copy, don’t reopen)

From `notes.md`, already decided:

- Atlas **A**: 12 outdoor + interiors extra.
- Continuous outdoor + warp interiors.
- Homestead = **gate** (not 12 lots).
- Title **The Commons**. Plaza region **the Square**.
- 3×4: North Woods | Lookout | Orchard / Workshop Lane | the Square |
  Hall Lawn / Lab Yard | Homestead Row | Garden / Quarry Ridge |
  Creek | Shore.
- GBA / FireRed tiles.
- V1 = all 12 lightly dressed, mine boarded, no workshop/crafting/
  farming/swimming/XP. Lab output placeable/wall-able.
- **V1 runs in the Grok preview app.** You are not writing that client
  this session. Droplet sibling + SQLite later.
- `liquidgov_http` is **never** the game.
- Presence per-region ~2 Hz even on one canvas. Droplet 512 MB / 1 vCPU.
- Guests walk the 12; house binds after LiquidGov cookie; no second
  password. Preview throwaway name until glue.
- Profile canvas = **one wall slot**, not the floor.
- Friends = civic query, short RAM TTL. Visit: open / friends / locked.
- Photo → civic blob + feed only. No posts/comments/follows in the game.
- Touch: on-screen d-pad; A/D = left/right on screen.

Closed civic pins still closed: no age checkbox, friends = mutual
accept, phone = signup binding not login, platform community ≠ AVND
locality, no React civic UI, no live-elections claim, no SSH from Grok.

## Your job this session (one slice)

**Spec pack for The Commons.** Fill `Persistence/TheCommons/` so Grok
can build the preview without inventing. Bounce Grok: accept, rewrite,
or reject each bet in `GROK_BET.md`.

Create/write (markdown + SQL only):

1. `atlas.md` — each of the 12: job, size in tiles, 1–2 set pieces,
   doors/warps, collision intent, what “lightly dressed” means. Interiors
   extra (house, lab; hall skipped V1; mine absent). Homestead gate
   (demo door + cap on online doors).
2. `schema.sql` — SQLite, ~3NF catalogs / 2NF instances. At least:
   items, recipes + inputs/outputs, maps, map_instances, players,
   inventory, placed `{item, x, y, rot90}`. House JSON is a **cache**,
   not source of truth. No friend graph table. No LGP1 tags.
3. `presence.md` — RAM layout, per-region ~2 Hz, snapshot on leave/~30s,
   guest ids vs `lg_username`, hard cap so 512 MB later doesn’t die.
   Not 10 Hz. Not a civic write.
4. `art.md` — tile size, sprite size, camera, palette (outdoor ≠
   parchment), V1 drawable list (how few unique tiles/sprites). Phone +
   desktop. A/D = left/right.
5. `glue.md` — contracts only, no implementation: cookie name/domain
   TBD, friends query shape, Hall door URL, photo blob+feed, wall-slot
   bitmap. What V1 **fakes** in preview vs what the droplet must
   eventually expose. You may note civic read-only endpoints as future
   work. Do **not** add them to `http_server.cpp` this session.
6. `v1.md` — in scope / out of scope. Guest walk, demo house, lab table
   with the recipes you keep, Hall door, boarded mine, d-pad.
7. `pushback.md` — **required.** Where Grok’s bets are wrong (grid size,
   240×160, 3 recipes, 6 homestead doors, skip Hall interior, etc.).
   Prefer a replacement, not a complaint.

Also append **one dated bullet** to `Persistence/notes.md` under the
Game heading: spec pack landed, list files, list any lock you think
must change (do not silently change locks).

Commit on your own (notes + TheCommons). Do not wait for “push it.”
Do not rsync. Do not SSH. Do not touch `*.lgp`.

## Do not do this session

- Any playable client, spritesheet generation, or scaffold.
- Any edit under `liquidgov-platform/` or `liquidgov-core/`.
- Any new LGP1 tag.
- Juketube, Messages PWA, civic UX, `/mail`, React civic UI.
- Mine interior, workshop loop, farming, swimming, XP, second login.
- Fake `context.md` / `state.md`.
- Claiming liquidgov.us runs the game.

## How to work

1. `git pull`. List files opened.
2. Slice = spec pack. Markdown + SQL in `Persistence/TheCommons/`.
3. Fight `GROK_BET.md` in `pushback.md`. Keep visiting as the loop.
4. If a pin and a convenience conflict, keep the pin and say so.
5. Stop and report: files written, bets you overrode, anything you
   refused, commit hash.

Grok will read `pushback.md` next and answer. Then Brandon says BUILD
before anyone writes game code.
