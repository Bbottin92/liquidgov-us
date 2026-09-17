# Glue — The Commons ↔ LiquidGov

Contracts only. No implementation this session — nothing below is added to
`http_server.cpp`. Two programs, one person; this file is the entire seam
between them.

## Session cookie

**TBD, not invented here.** Whatever cookie `/login` already sets on
LiquidGov is the candidate — do not name or shape a new cookie speculatively
in this document. When glue work actually starts: read the real
`Set-Cookie` behavior out of `http_server.cpp` first, then document the
actual name/attributes here.

**Open architecture question, not a V1 blocker but real:** cookies don't
cross origins. For the game process to read a `liquidgov.us`-set cookie, it
needs to run same-origin — e.g. reverse-proxied at `liquidgov.us/commons/`
or a subdomain sharing the cookie's `Domain=` attribute. This has to be
decided before glue lands; it is not decided here and should not be assumed
either way.

- **V1 fakes this entirely.** No cookie check. Guests type a throwaway
  display name and that's the whole identity for the session (see
  `presence.md`).

## Friends query

A **read-only** query, never a copied graph:

- Contract: `GET /internal/friends?username=<lg_username>` (path/params are
  a placeholder shape, not a committed route name) → JSON array of mutually
  accepted friend usernames.
- Game-side caching: short RAM TTL (seconds, not persisted), re-fetched
  per-session or on a timer — never written into `schema.sql`. There is no
  friends table in this project's schema and there must never be one.
- Used for: showing whether a Homestead Row dynamic door's owner is a
  friend, and for the "open / friends / locked" visit tiers `notes.md`
  already names. **Not** used to build a social graph inside the game.
- **V1 fakes this entirely.** With no cookie glue, there's no `lg_username`
  to query with, so all 6 Homestead dynamic doors render boarded (see
  `atlas.md`, `v1.md`). This is not a bug to fix in V1 — it's the correct
  behavior until the cookie question above is answered.

## Hall door

No server contract needed — it's a link. Hall Lawn's interact prompt opens
`https://liquidgov.us/proposals` in a new tab after a one-line confirm. This
already works with zero glue and ships in V1 as-is.

## Photo → civic blob + feed

**Future work, not touched this session, not even faked in V1** (see
`v1.md` — there is no photo-capture button in V1 at all). When it exists:

- Contract sketch: the game POSTs a captured image to a civic endpoint
  (doesn't exist yet, name TBD) that stores it as a blob and creates a feed
  post, the same way any other civic photo upload does today.
- The game never grows its own posts/comments/follows — this is strictly
  "hand the image to LiquidGov and let the civic feed own it," per the
  locked split in `notes.md`.

## Wall-slot bitmap (profile canvas)

- Contract sketch: the game's wall-slot prop needs to fetch a given
  `lg_username`'s profile-canvas bitmap, read-only, by some stable
  reference (URL or endpoint — **unverified whether one is already exposed
  externally today**; check `http_server.cpp`'s profile-canvas route when
  glue work starts rather than assuming either way).
- **V1 fakes this entirely.** The wall slot renders as an empty frame
  placeholder image. No fetch, no username, nothing dynamic.

## Summary: what V1 fakes vs. what the droplet must eventually expose

| Contract | V1 preview | Eventually needs |
|---|---|---|
| Session cookie | Not checked at all; throwaway name | Real cookie read + same-origin decision |
| Friends query | Not called; doors render boarded | `GET /internal/friends?username=` (or similar) |
| Hall door | Live today, zero glue needed | Nothing further |
| Photo → blob+feed | Feature does not exist in V1 | New civic upload endpoint (future work) |
| Wall-slot bitmap | Empty frame placeholder | Read-only fetch of the existing profile canvas |

Nothing in the right-hand column is built this session. This table exists so
the eventual glue work has a checklist instead of a re-discovery.
