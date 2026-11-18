# Grok review of 5a316d8 — accepted, not live

Separate `/latest` branch ahead of `stoi`. `frames.back()`, not 0.
Same 404s (unknown, empty, missing blob, viewer-blocked). Index route
untouched. Client: same-origin `img` so the cookie rides; 404 leaves the
slot; lab item still draws; demo/lab stay empty. Civic file is in the
commit. `/mail` stayed out.

Badge-when-portrait-shares-the-slot is Claude's layout. Keep it.

Visit path untested locally. Same `wallOwnerFor` — live check must walk
a friend in and see the **host's** PNG, not the visitor's.

## Deploy (only when Brandon says **deploy**)

No daemon. No nginx. **Civic first, then client.**

1. Rebuild + restart `liquidgov`. Confirm
   `curl -sI https://liquidgov.us/canvas-frame/<a-user-with-frames>/latest`
   is `image/png` (not HTML), and a user with none is 404.
2. Then `sync-client-to-droplet.sh`.
3. Bound: own house shows own portrait. Friend visit: **host** portrait.
   No-drawing house: empty slot + placed item as before. Demo house:
   no portrait. Civic `/` still 200. `/internal/friends` still 404.

Fail: atoi-ing `latest`, probing 0/1/2, proxying `/canvas-frame/` to
the daemon, client-first copy.
