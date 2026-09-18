# Visiting (friends policy) — built, local-tested, not deployed

Brandon: "BUILD visiting friends" -- the house policy is **friends**:
mutual accept only. Treated as a fixed rule for every house, not a
per-owner setting (that would need stored data and UI; a later round if
wanted). No civic change this round -- it reuses `/internal/friends`.

## The rule (one function, `visit_allowed`, in `main.cpp`)

A bound player may be in another player's house iff that owner is one
of their mutual, non-blocked friends (civic's answer, through the same
15s `FriendsCache` the lot doors use -- never a client claim), **and**,
to *enter*, the owner is in the game right now (the same condition that
lights the door). Staying only needs friendship, so a visitor isn't
thrown out because the owner logged off.

- **join/rekey into a house:** owner path unchanged (creates rows,
  resumes position). Anyone else needs `visit_allowed(..., true)`;
  guests never qualify. Refused with the same `403 not_your_house`.
- **A visit is read-only.** No `players`/`map_instances` row is created
  for the visitor, no snapshot on leaving, no resume position -- only the
  owner's own entry does any of that. `/placed/` POST stays owner-only
  (403), so a visitor can look at the wall but not change it.
- **move re-checks friendship** for a visitor, so an unfriend or a
  block ends the visit within one TTL. Answered `200 {"ok":false,
  "reason":"not_welcome"}` after removing the visitor -- deliberately not
  403, because the client reads 403 as "my credential died".

## Client

The named lot door (prompt "Visit") enters a new `mode: "visit"` with
`INTERIOR_INSTANCE_KEY.visit = "instance:house:<friend>"`, reusing the
same interior template. The wall says "That's <owner>'s wall." and its
button reads **Look**. A refused visit or an eviction walks the player
back outside with a toast; the next tick's normal region-mismatch rekey
clears the optimistic visit key.

Real bug this round would have shipped: `netHandleJoinResponse` treated
*every* 403 as a dead credential and reminted. A declined visit is a 403
too, so the visitor would have reminted and retried forever. It now reads
the reason: `not_your_house` while visiting = declined; anything else =
reauth as before.

## Tested (local; real civic accounts, real daemon)

alice owns, bob is her mutual friend, carol is not:
1. alice offline -> bob refused (403).
2. alice enters her own house (owner path, unchanged).
3. bob welcomed once alice is in; response carries no resume fields.
4. carol refused (403); a guest refused.
5. alice and bob are in the room together (both listed).
6. bob's placement 403; alice's ok. bob can read the wall.
7. `players` holds only alice -- **bob's visit created no rows**.
8. bob moves fine while friends; after alice blocks him he is still ok
   inside the TTL and gets `not_welcome` after it; he is gone from the
   room, alice remains; he cannot re-enter (403).
9. alice's own leave/re-enter still resumes her position.

Client (real browser, scratch daemon, `/presence/lots` stubbed because a
local static page can't carry the civic cookie): the door reads "Visit";
a **real** daemon refusal walked the visitor out with a toast, zero
reauths, same credential; a stubbed welcome kept the visit, the wall
refused placing without calling the daemon; a stubbed `not_welcome`
evicted with a toast and no reauth.

## Not here

- Owner presence isn't re-required while staying; visitors linger in an
  empty house until they leave.
- Unfriend/block eviction is up to one TTL (15s) late by design.
- `open` and `locked` policies, the mine, `/latest` -- unchanged.
- The ~30s snapshot, `rebind()`, session cache -- still parked.

No deploy step run. This round changes daemon + client only (civic and
nginx untouched): daemon first, then client.
