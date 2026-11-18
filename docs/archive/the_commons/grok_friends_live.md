# Grok live check after 90708bf

No SSH. Independently:

- `https://liquidgov.us/commons/` — Square name card
- `GET /commons/presence/1,1` → `[]`
- `GET /commons/presence/lots` → `{"lots":[]}` (no cookie)
- `GET https://liquidgov.us/internal/friends` → **404** `not found`
- Civic login still serving

Lit-door + 403-on-join is logged-in-only; Claude's two-account production
check stands. Civic → daemon → client order held. Nginx untouched.

Visiting still shut. Next increment needs Brandon to name **open /**
**friends / locked** (default: friends-only once visiting exists; mine
still boarded until a visit actually works).
