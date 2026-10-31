# Question for Grok — BUILD house open/locked: what does each mean?

Brandon said **BUILD house open/locked**. No code written. The only spec on
file is a phrase: `open / friends / locked` (`GROK_FRIENDS_LIVE.md`,
`friends-query-built.md`: "no column, no UI"). Visiting shipped with one
fixed rule, **friends** (`visiting-built.md`), and both docs leave
`open`/`locked` as "later". Several real decisions are open, and one of them
collides with a lock, so I stopped instead of choosing.

## What exists

- `visit_allowed(headers, owner, FriendsCache, presence, require_owner_online)`:
  entry needs a mutual, **non-blocked** friend (civic's `/internal/friends`
  already removes blocks in either direction) **and** the owner online;
  move re-checks friendship only; refusal is `403 not_your_house`, eviction is
  `200 {"ok":false,"reason":"not_welcome"}`. Guests never qualify.
- `/presence/lots` lights a door only for a mutual friend who is in the game.
- Nothing per-house is stored. `players` has no policy column; nobody can set
  one; there is no UI to set it.

## Decisions I need

1. **Who sets it, and where?** My proposal: the owner, from inside their own
   house: one interactable (a latch/sign tile) where Enter toggles the
   door and toasts the new state. Stored as `players.visit_policy`
   (`'friends'` default, so every existing house is unchanged), added by an
   idempotent startup `ALTER TABLE` like `ensure_inventory_recency()`.
   Owner-only `POST /presence/house-policy`, identity from the cookie only.
2. **What does `locked` mean?** Proposal: nobody but the owner, friends
   included. Existing visitors are walked out on their next move (the same
   `not_welcome` path unfriend/block already use). The owner's lot door goes
   dark for friends.
3. **What does `open` mean, and does it honour blocks?** This is the
   collision. Civic's only answer today is `/internal/friends`: it returns my
   friends *minus* blocked ones. It cannot answer "is this stranger blocked
   by the owner?". So a stranger-facing `open` cannot honour a block without a
   **new civic route** (frozen; needs its own named BUILD), and without it a
   player the owner has blocked could walk into an `open` house. I do not
   want to ship that quietly.
4. **Does `open` mean guests too, and does the owner have to be online?**
   Guests have no identity to block or evict, so I would say no: bound only.
   Online-to-enter I would keep (a house empties when its owner leaves;
   `open` then just means friends-of-nobody can drop in while they are here).

## Options

| | Ships | Civic change? | Risk |
|---|---|---|---|
| **A. friends / locked** (recommended) | Owner toggles friends <-> locked. `open` deferred. | No | None: it only ever *tightens* access, blocks stay honoured. |
| **B. friends / locked / open, blocks not honoured** | All three; `open` = any bound player. | No | A blocked player can enter an `open` house. Needs to be said out loud. |
| **C. all three, blocks honoured** | Same as B plus a narrow civic route (e.g. "viewer is blocked by owner") | **Yes** (a fourth named civic edit, like `/latest`) | Larger: civic deploy first, then daemon, then client. |

I recommend **A** first. It is the version with no privacy hole, needs no
civic, and `open` can follow as its own BUILD once you decide between B and C.
The block-honouring question is really "should strangers ever reach a house",
which is yours and Brandon's call, not an implementation detail.

## What A touches (so you can veto it)

- **Schema:** one column, `players.visit_policy TEXT NOT NULL DEFAULT
  'friends'`, plus an idempotent startup migration. **Online SQLite backup
  first**, same as inventory.
- **Daemon:** `visit_allowed` reads the owner's policy (locked => refuse);
  the move re-check does too; `/presence/lots` skips locked owners; one
  owner-only setter route. The join response can echo the caller's own policy
  so the client can label the latch.
- **Client:** one latch tile inside the *owner's* house (not visit mode, not
  demo), a toast, and lit friend doors already follow `/presence/lots`.
- **Not touched:** civic, nginx, guests, demo/lab/mine, `/latest`.
- **Deploy order:** daemon fully first (migration + policy), then client. No
  deploy until Brandon says `deploy` after your review.

## What I need back

1. **A, B or C.**
2. Confirm the owner-set-from-inside-the-house shape (or name another).
3. If B or C: whether `open` includes guests, and whether it still needs the
   owner online.

## Held to, regardless

Civic frozen unless C is named. No LGP1. Daemon stays loopback-only. No
deploy until the word.
