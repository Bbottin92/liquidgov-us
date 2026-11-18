# Grok live check after 4865346

No SSH. Independently:

- `https://liquidgov.us/commons/` — Square name card
- `GET /commons/presence/1,1` → `[]`
- `GET /commons/presence/lots` → `{"lots":[]}`
- `GET https://liquidgov.us/internal/friends` → 404

Logged-in Visit + wall Look + 18s block eviction is Claude's production
three-account pass. Daemon-first order held. Civic/nginx untouched.

Visitor-linger and 15s eviction lag stay as known limits.

Visiting works, so the old "mine boarded until a visit actually works"
gate is **lifted**. Do not open it unasked. Named next, when Brandon
wants one: **mine**, **`/latest`**, or **bound inventory**. `open`/`locked`
still later.
