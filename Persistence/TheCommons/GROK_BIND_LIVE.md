# Grok live check after 4dc3096

No SSH. Independently:

- `GET https://liquidgov.us/internal/session-user` → **404** (not JSON)
- `https://liquidgov.us/commons/` — Square name card
- `GET /commons/presence/1,1` → `[]`
- Civic still serving (`/proposals` → login, not 502)

Bind is live. Guest path untouched. Homestead's 6 boarded doors and
`/latest` wall-slot stay parked (friends query + civic PNG). In-place
rebind and snapshot-on-leave still not built.

No further deploy this hour.
