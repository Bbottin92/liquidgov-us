# Your house — built, local-tested, not deployed

Brandon: "build" (in reply to the offer naming this exact increment,
`GROK_V4_ANSWER.md`'s "your house"). Safe thread, exactly as scoped: no
`http_server.cpp`, daemon + `commons/index.html` only.

**2026-09-17 update: must-fix found and fixed before deploy.** See
"Must-fix: house key built from the wrong field" below -- the house key
described in the rest of this doc as `j.display_name` was wrong; it's
`j.username` now, a new, separate field.

## The hole this closes

`GROK_V4_ANSWER.md`: "you still live in the shared demo house" -- the
id was real (cookie bind), the door was not. A bound player walking
into the demo house was indistinguishable from a guest doing the same
thing; nothing about presence being real for them was actually visible
anywhere in the world.

## What's actually new

**Civic:** nothing. Untouched.

**Daemon (`main.cpp`):** `/presence/join` and `/presence/rekey` now
echo `"bound":true/false` alongside the existing `display_name`, computed
from whether `effective_id` came back `"u:..."` or a guest id. No new
route, no new PresenceStore primitive -- the per-username house
instance is just an ordinary opaque region string
(`instance:house:<username>`), exactly the same mechanism `instance:demo`
and `instance:lab` already use. `PresenceStore`'s per-region/global caps,
idle reap, and disambiguation all apply to it with zero special-casing,
same as every other region.

**Client (`commons/index.html`):**
- One new tile, `T_DOOR_HOUSE`, one new door gap in the Homestead Row
  facade -- physically adjacent to the demo door, not one of the six
  boarded friend-lot doors (`doorXs` untouched). Visible to everyone,
  guest or bound; only a bound player can actually walk through it.
- `isBound` (client state, set once `bound:true` ever arrives on a
  join/rekey response -- no downgrade path, a resolved cookie doesn't
  un-resolve mid-session) and `INTERIOR_INSTANCE_KEY.house`, filled in
  with the real per-user key the first time binding is confirmed.
- New `mode: "house"`. Reuses `demoHouseSpace` as its interior --
  same 16x16 room, same table/chairs/rug, same wall-slot at (7,0) --
  because what makes it *yours* is the instance key (a separate
  presence room, a separate `/placed/` slot), not different art. This
  is the same "shared shape, different instance" pattern `instance:demo`
  / `instance:lab` already established; it just needed the daemon to
  actually know who "you" were before it was worth building.
- A guest who interacts with the new door gets a toast ("Log in on
  LiquidGov to open your own house."), same honesty as the six boarded
  doors' "Closed." -- not a silent wall, not a fake prompt.

## What's still NOT here (deliberately)

- **Snapshot-on-leave.** `GROK_V4_ANSWER.md`: "only matters once that
  house exists... rides that same BUILD if there's room." Turned out not
  to be a small addition once actually scoped: `schema.sql`'s `players`
  table snapshot columns are real, but nothing anywhere has ever created
  a `players` row for anyone, bound or guest, and `house_instance_id`/
  `last_instance_id` are `INTEGER REFERENCES map_instances(instance_id)`
  -- an actual foreign-key row, not the plain RAM string key this round
  uses. Wiring that up is a real, separate unit of design (when does a
  `map_instances` row get created for a personal house? on first visit?
  on signup?) and code, not a few extra lines alongside this. Deferred,
  not forgotten -- flagged here rather than rushed.
- **In-place `rebind()`.** Still not decided, still not needed --
  binding still happens at join/rekey (a room change), same as cookie
  bind's own must-fix fixed it to.
- **`lg_session` cache.** Still Square-scale fine, per `GROK_V4_ANSWER.md`.
- **Friends doors, `/latest`.** Still civic-gated, still not this round.
  The six boarded doors are untouched -- this build did not open any of
  them.

## Tested (local, scratch data only)

Backend (curl, real signup, real cookie, garbage client id/secret same
as every prior round's methodology):

- Join to `instance:house:housetest` -> `{"ok":true,...,"bound":true}`.
- Rekey back outdoors -> still `"bound":true`.
- Guest join, no cookie -> `"bound":false`.
- Two different bound accounts' houses are isolated: `housetest`'s
  house empties out after they leave, `housetest2`'s house shows only
  `housetest2` -- no cross-contamination, confirming the per-username
  instance key needs no special-casing in `PresenceStore` (same proof
  shape as the two-shared-interiors check from the V3 round, just with
  a dynamic key this time).

Client (served statically, pointed at a scratch local daemon via the
existing `?daemon=` override; `isBound`/`INTERIOR_INSTANCE_KEY.house`
set directly in-console to stand in for a real cross-origin cookie,
which local static serving can't carry the way production's same-origin
`/commons/` does):

- New door tile renders distinctly (confirmed the tile id and its own
  color, not a demo-door reuse) and is reachable as its own
  interactable, not shadowed by the adjacent demo door.
- Guest interacting with it: toast, no interior change, `mode` stays
  `"outdoor"`.
- Bound: `tryInteract()` enters `mode: "house"`, the room renders
  (table/chairs/rug baked correctly), the network tick rekeys into
  `instance:house:<username>` and starts polling/moving against that
  exact instance (confirmed in the request log), the wall-slot accepts
  a placed item, `[Enter]` with nothing in range exits cleanly back
  outdoors, and switching to the demo house afterward shows a clean
  `placedItem: null` -- no bleed between instances.

Real cross-origin cookie -> real house entry is still only provable in
production, same as cookie bind itself -- next after Brandon says
**deploy**.

## Must-fix: house key built from the wrong field (found by Grok review, fixed)

`GROK_HOUSE_REVIEW.md`: the house key used `j.display_name`, but that's
the *disambiguated Square nameplate*, not the raw username --
`disambiguate_name_locked` appends " (2)" etc. when another occupant in
that region already has the same display name. A guest who types
"brandon" into the Square before the real `brandon` ever binds turns
the bound player's own display_name into "brandon (2)" -- a space and
parens in what was about to become a URL path segment, and a
*different, wrong* house room than the one every other client would
expect that door to lead to.

Fixed:

- **Daemon (`main.cpp`):** join/rekey now also echo a raw `"username"`
  field (only present when `bound`), taken from `eff_id->substr(2)` --
  the same unsuffixed string `bind_or_promote`/`effective_id_in_region`
  already derive from the cookie, never touched by disambiguation.
  `display_name` is untouched -- still the nameplate, still
  disambiguated, still what every other client actually sees drawn
  over that occupant.
- **Client (`commons/index.html`):** `INTERIOR_INSTANCE_KEY.house` now
  builds from `j.username`, never `j.display_name`. `isBound` is now
  set from every join/rekey response (`= !!j.bound`) instead of only
  ever latching `true` once, per Grok's should-fix -- a stale `true`
  surviving into a fresh guest reauth was a real (if minor) risk.
- **Defense in depth, both sides:** region/instance keys used as a URL
  *path* segment (not a JSON body field, which needs no encoding) are
  now `encodeURIComponent`-ed client-side
  (`regionPathSegment()`, four call sites: the two presence polls, the
  placed-item GET and POST). This surfaced a real, previously-latent
  bug while implementing it, not just theoretical hardening: the
  daemon's `http.cpp` never url-decoded `req.path` at all -- outdoor
  keys like "1,1" were only ever safe because a literal comma happens
  to survive unencoded in every browser/curl call this project has
  ever made. Encoding client-side without decoding server-side would
  have broken every existing outdoor poll (`1,1` -> `1%2C1`, no longer
  matching `PresenceStore`'s literal keys) the moment it shipped. Fixed
  by decoding `req.path` once, in `http.cpp`, exactly where the query
  string was already decoded -- one line, symmetric with the client's
  new encoding, and it's what should have been there from the start
  regardless of this round's own bug.

**Verified locally**, reproducing the exact scenario `CLAUDE_HOUSE_KEY.md`
asked for: a guest joins the Square as display_name "boundname" first;
a real civic account named `boundname` then joins bound -- its
`display_name` comes back `"boundname (2)"` exactly as expected, but
`username` stays the raw `"boundname"`. Rekeying into
`"instance:house:" + username` lands in `instance:house:boundname`
with no suffix; `GET /presence/instance:house:boundname` shows the
occupant, and the wrong, suffixed key
(`instance:house:boundname%20(2)`) is confirmed empty -- no ghost room
ever created there. Separately verified through the real client
(pointed at a scratch daemon): outdoor polling still resolves real
occupant data through the new encode/decode round trip (a second guest
placed via curl showed up correctly in `otherOccupants`), and
`netHandleJoinResponse` builds the house key from `username` while
`isBound` correctly flips back to `false` on a subsequent guest-shaped
response.
