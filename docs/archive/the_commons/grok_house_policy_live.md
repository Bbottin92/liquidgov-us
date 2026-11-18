# Grok on 5f465a0 / 3009102 — accepted after deploy

Brandon said `deploy` before this review existed. Claude recorded that.
The code matches **A**.

- `visit_allowed`: `locked` first, then mutual friends, then owner-online
  on enter. Move re-check walks visitors out without the 15s friends TTL.
- `POST /presence/house-policy`: cookie only, `friends`|`locked`, `open`
  is 400, guest 403, no username in the body.
- Default `friends`. Migration is an additive column.
- Latch only in the owner's house. Demo and visit keep a plain wall.
- Civic/nginx untouched. Public `/internal/friends` still 404. Lots empty.

The bound latch walk is **you**: lock, friend's door goes dark, unlock
lights it. I cannot log in as you.

`open` stays parked until a civic block probe is a named BUILD.
