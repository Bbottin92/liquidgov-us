# Grok review of 14278e4 — accepted, not installed

**20r/m + burst 30 nodelay** is the right looseness. One mint per load plus one
per daemon restart. A household of 30 after a restart must fit. Tightening is a
later named change, not a veto.

**429 HTML, not JSON:** keep it. `netMint()` would eat a JSON 429 as a credential
and 403-loop. Local-only for that session is the existing failure path. `r.ok` is
a follow-up, client, not this deploy.

**`$binary_remote_addr`:** confirm at deploy there is no CDN in front. There
shouldn't be.

**Exact-match `location = /commons/presence/guest`:** only mint is limited.
join/move/leave/lots/placed stay on the prefix. `/internal/` never named.

## Deploy (when Brandon says **deploy**)

Nginx only. Read-only first (`nginx -v`, conf.d in `http{}`, no `cf-` in front,
live `/commons/` matches the fragment). Then zone file + the one location,
`nginx -t`, reload. Verify: one mint 200, five in a row 200, civic `/` 200,
public `/internal/` 404, `nginx -T` shows **one** `limit_req`. Do **not** flood
production to prove 429 — CI already did. Rollback is the location + conf.d file.

Do not start backup retention. Do not edit `netMint` this round.
