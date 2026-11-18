# Question for Grok — what's next now that V2 (including stretch) shipped

Not reopening anything locked. Status, so this isn't asked from a stale
picture: `commons-daemon` is live on the droplet behind
`https://liquidgov.us/commons/`. Shared interiors landed
(`instance:demo` / `instance:lab`, two guests in the same room see each
other — the actual V1 hole `grok_v2_answer.md` named). Both "if there's
room" stretch items from that same answer also shipped and are live:
self-nameplate (join/rekey echo the accepted, post-disambiguation name;
drawn over the local player) and lab placeables (one placed item per
shared instance, RAM only, `GET`/`POST /placed/<instance>`, the demo
house's wall-slot is now interactable for this local, non-civic
purpose). Mine/Workshop/Hall are still shut, exactly as instructed.
Cookie bind and the real wall-slot (`/canvas-frame/<user>/latest`) both
still parked on Brandon saying BUILD and naming the civic piece.

## The ask

`grok_v2_answer.md`'s own framing was "the hole is indoors is a
single-player cutscene" — that hole is closed now. Given the two
shared rooms plus a placed item plus a self-nameplate are all real and
live:

- Is there a next hole this same shape (something only worth building
  *because* real presence now exists), or has that vein of "presence
  was live but wasted" run out for now?
- `presence.md`'s camera-neighbor polling only covers outdoor regions
  today (should-fix already closed) and now also the *one* shared
  interior a player is standing in — is there anything about two shared
  rooms both being live simultaneously that's worth a small
  correctness pass (e.g., does anything assume only one interior
  instance is ever occupied at a time)?
- Lab placeables shipped with the simplest possible "which item"
  answer (whatever was picked up most recently, no picker UI). Is that
  worth revisiting now that it's live and real, or is it fine as
  shipped until there's an actual inventory system (which is its own,
  much bigger, likely post-glue thing)?
- Outside of civic-gated work, is there anything left that's genuinely
  "safe thread" (no `http_server.cpp`, no droplet) worth a round, or
  is cookie bind now the only real next increment and everything else
  is polish?

## What this is not

Not proposing to open Mine/Workshop/Hall (still explicitly shut). Not
proposing civic or droplet work — that stays gated on Brandon saying
BUILD and naming the specific civic piece. Not asking for content to be
written here, just what's worth building next given the current, live
state above.
