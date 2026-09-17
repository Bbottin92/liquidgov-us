# Notes — ideas, decisions, parked work

Last updated: 2026-09-16

Shared working memory for **ideas and product decisions** (Grok + Claude).
Repo path: `Persistence/notes.md` on `Bbottin92/liquidgov-us` `master`.

Not bug classes. Not “what is live on the droplet.”

**How this file is used**
- Session start (both): read this file.
- During a turn: if Brandon decides, rejects, or parks something, append a dated
  entry *this turn*. Do not wait for “task done.”
- One heading per topic. Newest dated bullet at the top of that heading.
- Durable facts only. No transcript dumps. If it can be grepped from git, skip it.

---

## Shared pad

- **2026-09-16 — decided.** GitHub shares `Persistence/notes.md` (+ README) only.
  `context.md` and `state.md` exist in the *old* Grok sandbox and were never
  pushed. Do not invent fakes. Do not have Claude create them. A fresh Grok
  hard-gate that requires those two files will fail until we drop that read or
  Claude commits the *real* files from that sandbox. “Have Claude git pull”
  updates notes/README only.

## Game — atlas locked, no code until BUILD

- **2026-09-16 — Claude spec pack (no game code, civic frozen).** Paste
  `CLAUDE_THE_COMMONS.md` into Claude Code (not `CLAUDE_CODE_CONTINUE.md`).
  He fills `Persistence/TheCommons/` (atlas, schema, presence, art, glue, v1,
  pushback) from Grok bets in `GROK_BET.md`. V1 client stays the Grok preview
  after Brandon says BUILD. Droplet sibling later. Do not touch
  `liquidgov_http` / LGP1.
- **2026-09-16 — locked.** A + continuous outdoor + warp interiors + homestead
  gate. Title **The Commons**. Plaza region **the Square** (not “Commons”).
  Art: GBA / FireRed tiles. No code until Brandon says BUILD.
  - Outdoor 3×4, north up:
    North Woods | Lookout | Orchard
    Workshop Lane | **the Square** | Hall Lawn
    Lab Yard | Homestead Row | Garden
    Quarry Ridge | Creek | Shore
  - Interiors extra: per-player house, lab, hall (thin), mine (boarded / not present).
  - Homestead Row = gate (online / friends doors). Houses are instances, not 12 lots.
  - V1: all 12 lightly dressed; mine boarded; no workshop, crafting, farming,
    swimming, or XP. Lab output is placeable / wall-able, not “salt in a bag.”
  - **V1 runs in the Grok preview app.** Droplet sibling + SQLite later.
    `liquidgov_http` is never the game.
  - Presence: per-region at ~2 Hz even on one canvas. Droplet is 512 MB / 1 vCPU;
    do not flood.
  - Guests walk the 12; house binds only after LiquidGov cookie; no second
    password. Preview may use a throwaway name until glue exists.
  - Profile canvas from the site = one **wall slot**, not the floor.
  - Friends = civic **query**, short RAM TTL, never a second graph.
    Visit: open / friends / locked.
  - Photo → civic blob + feed only. Game does not grow posts, comments, or follows.
  - Touch: on-screen d-pad; A/D = left/right on screen.
- **2026-09-16 — parked, atlas not locked.** (superseded by lock above)
  Working title The Commons (not confirmed). Art lean GBA / FireRed cousin
  (not confirmed). Nothing built. Do not scaffold, sprite, or write game code
  until Brandon says BUILD. Do not start the game in the civic repo.
- **V1 proposed, not locked:** (superseded) walkable public ground, your house,
  lab, Hall door → live `liquidgov.us/proposals`, mine boarded. Guest play
  until cookie glue exists.
- **Atlas not locked.** (superseded) Brandon wants one greater map of about 12
  minor maps or instances. Options A/B and warp vs continuous were the choice;
  lock is A + continuous.

## Split: civic site vs game

- **2026-09-16 — decided.** Persistent FireRed/Sims/AC-style world is **not**
  grown inside `liquidgov_http` / LGP1. Two programs, one person.
  - LiquidGov keeps: login, friends, mail, votes, profile canvas, Juketube, civic pages.
  - Game keeps: tiles, positions, houses, inventory, lab, mining, visiting.
    Own process, own files. A game write must not take down Genesis.
  - Glue only: session cookie, friends as a *query* (not a copied graph),
    `/proposals` as an in-world door, blob+feed for photo mode. Game stores
    `lg_username` as a reference, never a second login.
  - Juketube-shaped hangout on this droplet (one screen, ~2 Hz, RAM presence)
    is a waiting room, not the concept. Don’t sell it as Animal Crossing.
- Chemistry: real *names and reaction classes* as a lookup table is fine.
  Real stoichiometry/safety is a different product. Mining without heat/grade/alloy
  is named fantasy tiers. Neither is a reason to pick this stack.
- Profile canvas = bitmap to hang on a wall. House layout =
  `{item, x, y, rot90}` rows. Don’t reuse the canvas as the house.
- **Do not start the game in this repo until Brandon explicitly says so.**
  When it starts, it is a sibling process + SQLite, not new LGP1 tags.

## Data: what lives where

- **2026-09-16 — decided.** Normalize catalogs and ledgers. Don’t NoSQL the civic site.
  - LGP1 stays the civic store. It is a tagged dump, not a database.
    Do not add game tags. Do not migrate LiquidGov to Mongo “to modernize.”
  - Game ledger (inventory, placed objects, recipes, trades): SQLite,
    ~3NF catalogs / 2NF instances. Trades are `BEGIN/COMMIT`.
    LGP1 cannot roll back a half-write.
  - Hot presence (position, who’s in the plaza): memory (or Redis later).
    Snapshot on timer/leave, not 10 Hz flushes.
  - House snapshot JSON is a *cache* derived from `placed` rows, not the only copy.
  - NoSQL that *does* fit: key–value for live presence; a document for a house
    blob you always load whole. Not the inventory ledger. Not a second friend graph.
  - If a column can be derived from LiquidGov or from `items`, don’t store it as truth.

## Messages home-screen app

- **2026-09-16 — confirmed live.** Checked droplet: `src/http_server.cpp` md5
  matches the WSL working copy exactly, `liquidgov_http` binary built the same
  day, service active. Curled production: `/mail` renders via
  `page_wrap("Messages", ...)` with `mail_install_hint()` in its own body (not
  site-wide), `manifest-mail.webmanifest` serves `start_url:"/mail"` live, and
  `/app/mail` already carries the Messages-specific `apple-mobile-web-app-title`
  / icon / manifest. Nothing left to implement — this item is closed.
  - Original ask, for context: iOS “Add to Home Screen” captures the
    **current URL**, so `/apps` → `/app/mail` (install copy) → icon would open
    the install page, not `/mail`. Fix was to nudge the install hint from
    inside live `/mail` itself with page-specific manifest/icon/title, which
    is what's now confirmed running.
  - Already-saved wrong icons from before the fix cannot be patched; those
    users delete and re-add from `/mail`.

## Standing product pins (copy, don’t reopen)

- Claude is sole droplet implementer of `/opt/liquidgov`. Grok: GitHub + prompts, no SSH.
- `lg-deploy` rsync as root left `/opt/liquidgov` `root:root` → store write SIGABRT.
  Directory must be `liquidgov:liquidgov` after rsync. Never rsync `*.lgp`.
- Friends are mutual accept. No age checkbox. Phone is signup binding, not login.
- Platform community ≠ AVND locality. No React/Vite/Node UI.
- Do not put game world state in LGP1 or `http_server.cpp`.
