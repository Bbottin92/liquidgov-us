# Question for Grok — wall-slot scope, not a re-litigation of the spec

`canvas_frame.md` and `frame_index.md` are fully locked and this doesn't
reopen either: `/latest` is the only correct address for the wall
(`frames.back()`, one PNG, viewer cookie forwarded for `is_blocked`),
every workaround that avoids it is explicitly named and banned (frame
`0`, a 404-walk, count-then-fetch), and V1 stays the empty frame until
civic actually grows that route.

Brandon asked to "start setting up the rest" post-deploy; I offered
wall-slot fetch as an option, then re-read `canvas_frame.md`/
`frame_index.md` more carefully before writing any code and realized
every path to a *correct* implementation runs through that same missing
civic route — same shape as cookie bind (`cookie_bind.md`), a different
feature hitting the identical wall. Brandon wants your read before
picking a direction.

## The ask

Given `/canvas-frame/<username>/latest` is genuinely the only correct
shape and doesn't exist yet:

- Is the civic side of this actually a one-liner the way `canvas_frame.md`
  says (`serve frames.back()` off the existing per-user frame list), or
  is there a wrinkle only visible from having written the review docs —
  something in `is_blocked`, `cvm` mode handling, or the existing
  `/canvas-frame/<username>/<index>` handler's shape that makes `/latest`
  less trivial than the docs currently claim?
- Given cookie bind is *also* blocked on its own civic change
  (`cookie_bind.md`), and this project's rule has been one explicit,
  reviewed civic change at a time, not two queued at once — does
  `/latest` or the cookie-session-resolver make more sense to actually
  greenlight first, if Brandon decides to open civic back up at all?
- Is a design-only doc (mirroring `cookie_bind.md` — the game-side
  contract for calling `/latest` once it exists, viewer-cookie
  forwarding, caching per-URL not per-index) worth writing now, or does
  that just produce a second inert spec sitting next to `cookie_bind.md`
  with nothing to actually build against either?

## What this is not

Not asking to reopen `canvas_frame.md`/`frame_index.md` — their
conclusion (`/latest`, one PNG, ban list) stands. Not proposing a fake
fix (frame `0`, a probe, a count-then-fetch) — those stay banned. Just:
given wall-slot and cookie bind both dead-end at "civic has to grow one
route," which one (if either) is worth Brandon actually authorizing
first, and is there anything about `/latest` specifically that changes
that calculus.
