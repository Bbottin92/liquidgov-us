# Glue — The Commons ↔ LiquidGov

Contracts only. No implementation this session — nothing below is added to
`http_server.cpp`. Two programs, one person; this file is the entire seam
between them.

Everything cited below (line numbers, exact strings) was read from the
WSL working copy of `http_server.cpp`, per this round's scope ("WSL copy,
not SSH"). This project has a known pattern of the droplet running ahead
of git, so the WSL copy's md5 was checked against the live droplet's
(read-only, no deploy) before writing any of this down:
`03fd46eb02f03ed917816c84534f58d2` on both, 2026-09-16 — byte-identical
right now. Still re-verify before actually building the sibling if any
time passes; this is a point-in-time match, not a standing guarantee.

## Session cookie

**Read from `http_server.cpp` directly (WSL copy), not invented.**
Correction to this session's own first pass, caught by Grok citing
`deploy/ux-on-droplet.patch` independently and rechecked directly in the
source rather than taken on trust: the first read of `set_cookie()`
missed a conditional two lines below the literal string and reported "no
`Secure`" as flat fact. Wrong — see below.

- **Name: `lg_session`.** Built in `set_cookie(sid)` (line 1110):
  `Set-Cookie: lg_session=<sid>; HttpOnly; Path=/; SameSite=Lax;
  Max-Age=2592000` (30 days), **then `; Secure` appended if `g_https`**
  (line 1112). `g_https` is set once per request (line 6824) from
  `X-Forwarded-Proto` (case-folded, compared to `"https"`) — i.e. it
  reflects what nginx tells the app, not TLS the app terminates itself.
  On the real site behind nginx this is true for real traffic, so the
  cookie the browser actually receives in production **does** carry
  `Secure`. `clear_cookie()` (line 1116) mirrors the same conditional.
  Whoever proxies `/commons/` needs to keep forwarding
  `X-Forwarded-Proto` correctly, or `g_https` (and therefore `Secure`)
  silently goes false — an nginx concern (`nginx.md`), not this file's.
- **No `Domain=` attribute anywhere.** This is a host-only cookie —
  browsers do not send it to a subdomain unless `Domain=.liquidgov.us` is
  added. That answers the "path vs subdomain" question this file used to
  leave open, as a hard fact rather than a preference: **path
  (`liquidgov.us/commons/`) works with zero civic changes; a subdomain
  needs `http_server.cpp` to gain a `Domain=` it does not have today.**
  Not deciding which to use here — just removing the "assume nothing"
  caveat, since one side of it no longer needs to be assumed.
- **`HttpOnly` means the canvas cannot `document.cookie` this, ever** —
  not a V1 restriction, a browser one. Any cookie-based glue reads the
  `Cookie` header **server-side** (the daemon, or civic), never client JS.
  See `cookie.md` for the actual per-request parse algorithm (split on
  `;`, exact `lg_session=` prefix match — not a `Cookie:`-header-wide
  `strstr`, which `not_lg_session=` would false-positive on).
- **`sid` is an opaque session id, not a username.** Resolution chain,
  exactly what any future glue endpoint should replicate rather than
  reinvent (`account_from_req()`, line 1632):
  `Cookie: lg_session=<sid>` → `cookie_of(headers)` (raw string search,
  case-sensitive `"Cookie:"`/`"cookie:"`, cut at `;`) → `g_store.get_session(sid)`
  → `Session{account: AccountId, live_connection_ids, ...}`
  (`libs/liquidgov_session/.../session.hpp`) → `g_store.find_by_id(account)`
  → `PlatformAccount{username, ...}`. There is no `lg_username` stored on
  the session directly — it's always this two-step lookup. Looking up a
  session also calls `touch_session(sid)` (updates `last_seen`) as a side
  effect, worth knowing if a daemon calls this often.
- Sessions already track their own live WebSocket connection ids for
  LiquidGov's own presence concept (`is_live()`). Unrelated to the game's
  presence, not reusable as-is, just prior art worth knowing about.

- **V1 fakes this entirely.** No cookie check. Guests type a throwaway
  display name and that's the whole identity for the session (see
  `presence.md`).

## Friends query

A **read-only** query, never a copied graph, **cookie-scoped**:

- Contract (placeholder path, not a committed route): the game process
  calls a civic friends endpoint **authenticated by the existing session
  cookie**. The server takes `lg_username` from the session. **Never
  `?username=` (or any client-supplied name).** A public username query is
  a roster scrape.
- Confirmed implementable exactly as designed, not just plausible: the
  cookie→session→account→username chain above (`account_from_req()`) is
  real, existing code, not something a new endpoint would have to invent.
  A future `/internal/friends`-shaped route calls the same chain, then a
  mutual-accept lookup — it does not need a new auth mechanism.
- Response: JSON array of mutually accepted friend usernames for *that*
  session.
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

**Checked, and the mental model in the old version of this section was
wrong: there is no single "profile-canvas bitmap."** The public
`/u/<username>` page's identity card animates through *frames* via CSS
(`profile_canvas_css()`, line 562) — a `@keyframes` loop cycling
`background-image` across per-frame URLs, not one static image.

- **The per-frame bitmap route already exists and is public:**
  `GET /canvas-frame/<username>/<index>` (line 7497) → `image/png` bytes,
  `Cache-Control: private, max-age=300`. No cookie required to fetch it —
  comment in the source calls it "public either way, same as `/portrait/`."
  This *is* the stable reference the old TBD was asking about; the game's
  wall-slot fetch can hit this directly, read-only, exactly as already
  planned ("a friend's canvas is whatever their public portrait already
  is" turns out to be literally true, not just a reasonable assumption).
- **One real gap, not a blocker:** the route is indexed by frame number,
  and nothing today exposes *how many frames exist* or *which index is
  current* to an outside caller — the profile page itself already knows
  this server-side (`g_store.canvas_frames(id).size()`), but there's no
  endpoint that hands that count out. Whoever builds this glue should add
  one small addressing convenience (e.g. a `/canvas-frame/<username>/latest`
  alias, or a tiny JSON endpoint returning the count) rather than have the
  game probe indices upward until a 404. Not deciding the exact shape here
  — just flagging it as the one thing that doesn't already exist, versus
  the frame-serving route itself, which does.
- **One behavioral nuance:** the route silently 404s a frame if the
  *viewer* (from their own cookie, if any) has blocked or is blocked by
  the frame's owner (`g_store.is_blocked`). An anonymous/unauthenticated
  fetch (no cookie forwarded) skips that check entirely and always sees
  the frame if it exists — worth knowing before assuming "public" means
  "identical for every viewer."
- **V1 fakes this entirely.** The wall slot renders as an empty frame
  placeholder image. No fetch, no username, nothing dynamic.

## Summary: what V1 fakes vs. what the droplet must eventually expose

| Contract | V1 preview | Eventually needs |
|---|---|---|
| Session cookie | Not checked at all; throwaway name | `lg_session` exists today (`Secure` when `g_https`); daemon parses it server-side (`cookie.md`); path `/commons/` receives it, subdomain needs a civic `Domain=` |
| Friends query | Not called; doors render boarded | New `/internal/friends`-shaped route reusing `account_from_req()`'s existing chain — no new auth to invent |
| Hall door | Live today, zero glue needed | Nothing further |
| Photo → blob+feed | Feature does not exist in V1 | New civic upload endpoint (future work) |
| Wall-slot bitmap | Empty frame placeholder | `/canvas-frame/<username>/<i>` already exists and is public; only missing piece is a way to address "the latest frame" |

Nothing in the right-hand column is built this session. This table exists so
the eventual glue work has a checklist instead of a re-discovery. Session
cookie and wall-slot are now verified facts (read from `http_server.cpp`,
2026-09-16), not open questions; friends query and photo are still sketches
of routes that don't exist yet.
