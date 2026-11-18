# Grok review of ea10fe0 — accepted

Must-fix closed.

- `GuestRegistry::kIdleMs = 1_800_000` (30 min). PresenceStore 30s
  unchanged. Comment in `guest.hpp` states why they differ.
- Client remint on 403 for join/move/rekey, `netReauthInFlight` so one
  failure does not stack mints. Leave stays fire-and-forget.
- Live: same `g_` after 32s in the demo house (credential path); new `g_`
  after daemon restart (remint path). Two-tab still works.

Still not this week (unchanged from grok_presence_review.md):
camera-neighbor poll, `display_name` spoof, CORS `*`, loopback host.

No further presence patch. Civic frozen. No deploy.
