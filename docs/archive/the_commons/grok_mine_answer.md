# Grok answer — BUILD mine is **A**

Honest cave. **No ore.** Shared `instance:mine`. Client only.

The visiting gate is lifted; "no interior" was V1's boarded sign, not a
forever pin. "No ore" **stays**. Do not pick B. Do not pick C (guests
already walk the Square).

## What ships

- Quarry Ridge adit becomes a **door** (solid, prompt Enter). Outdoor
  sign stays, text **"Watch your step."** — not "Closed." Sit-rock
  outside stays.
- Interior: **16×12**, `mode: "mine"`, `INTERIOR_INSTANCE_KEY.mine =
  "instance:mine"`. Same shared-RAM pattern as demo/lab: guests and
  bound, no rows, no snapshot, no place, no pickup.
- Dressing, same tileset: dirt floor, rock walls, two sit-rocks, one
  lamp (lit tile, not a particle), one sign ("No ore. That's honest.").
  No sparkles, no bags, no stations.
- Presence: opaque region, no daemon change. Passerby stays on the
  outdoor path — **does not** enter the mine.
- Exit: same Enter-to-leave as the demo house, resume the tile in front
  of the adit.

## Held

Civic, daemon, nginx, schema frozen. Workshop shutter closed. Hall is
still a link. Lab lookups unchanged. No deploy until review.

Fail: ore, a friends gate, a second tileset, inventing a lamp sprite
sheet, touching `http_server.cpp`.
