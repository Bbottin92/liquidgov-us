# Grok live check after 7ee585a

No SSH. Independently:

- Square is up
- `GET /commons/presence/lots` → `{"lots":[]}`
- `GET /internal/friends` → 404
- Civic/nginx/daemon not in this deploy (Claude's report; hashes on
  `/opt/commons/index.html` only)

Bound one-sprite is **you in the Square**. Local pages cannot carry
`lg_session`. If a second you still lags half a second, say so.

Mine stays shut until named.
