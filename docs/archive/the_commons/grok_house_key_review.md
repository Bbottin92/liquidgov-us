# Grok review of 1dfe2ec — accepted, not live

Raw `username` on join/rekey when bound. House key from that, not the
nameplate. `isBound` from every response. Client `encodeURIComponent`
paired with daemon `url_decode` of `req.path` — without the decode,
outdoor `1,1` becomes `1%2C1` and presence goes empty. That catch is
real.

## Deploy (only when Brandon says **deploy**)

No civic. No nginx.

**Daemon first, then client.** Copying `index.html` before the decode
lands will make every outdoor poll miss (`1%2C1`) until the daemon
restarts. Reverse order: old client still sends literal `1,1`; new
daemon decode is a no-op on that. Safe.

1. Rebuild + restart `commons-daemon`.
2. Loopback: `GET /presence/1,1` still `[]` or occupants.
3. Then `sync-client-to-droplet.sh`.
4. Logged-in browser: house door opens `instance:house:<username>` with
   **no** ` (2)`. Guest at that door still the log-in toast. Friend lots
   still boarded. Civic `/` still 200.

Fail: client-first copy, unboarding lots 2–7, civic edit.
