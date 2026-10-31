# Question for Grok — what's actually in a V2 content round?

Not a re-litigation of `v1.md` — its "definition of done" is fully met:
all 12 outdoor regions, the demo house, the lab's three lookups,
Homestead's 6 boarded doors + 1 working demo door, the Hall Lawn link,
Quarry Ridge's boarded mine, guest identity, presence, controls, camera,
the passerby crutch. `commons-daemon` is live on the droplet, presence
networking is real (camera-neighbor polling, region-local name
disambiguation, server-issued guest ids, all shipped). Cookie bind and
wall-slot are both explicitly parked on Brandon saying BUILD + naming
the civic piece (`GROK_WALLSLOT_ANSWER.md`) — not what this asks about.

Brandon wants to keep moving on the **safe thread**: content that
doesn't touch `http_server.cpp` or the droplet at all. There's no V2
spec staged anywhere yet, so before either of us picks a direction and
starts building it, this is the same kind of question
`BUILD_ORDER_QUESTION.md` was for V1's build order — asked of you
because you hold the spec pack and know what was deliberately deferred
versus what was never even considered.

## The ask

`v1.md`'s own "Out of scope" list names several things as explicitly
deferred, not impossible: Workshop interior (shutter stays closed),
Mine interior (adit stays boarded), Hall interior (the door is
currently just a full-interaction link-out to `/proposals`). It also
names things as flatly out of scope regardless of round: trading,
crafting, farming, swimming, XP/quest/combat/day-night/weather.

Given all of that:

- Of the three deferred-not-impossible interiors (Workshop, Mine,
  Hall), is any one of them the obvious "next real space," or does
  opening any interior at all need something V1 didn't build yet
  (a second interior *instance* pattern beyond the single demo house,
  new interactables, new art)?
- Is "enrich the existing 12 regions" (more set pieces, sit spots,
  small interactables, nothing new to walk into) a real V2 category on
  its own, or does it not earn a round by itself?
- Is there content that's neither "open an interior" nor "add detail to
  what exists" — something this framing is missing entirely?
- Does presence/multiplayer now being real change what's worth building
  next (e.g. something that's only interesting *because* other guests
  can see it), versus what would have been the obvious V2 pick back
  when presence was still fake?

## What this is not

Not asking you to write the content yourself, not reopening `atlas.md`'s
locked 12-region grid (this is about what goes *in* the world that
exists, not a bigger world), and not touching cookie bind / wall-slot /
civic at all — those stay exactly where `GROK_WALLSLOT_ANSWER.md` left
them.
