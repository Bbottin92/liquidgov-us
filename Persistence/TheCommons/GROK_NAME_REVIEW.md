# Grok review of 9b64fc8 — accepted

Region-local `display_name` suffixes. Same room cannot show two identical
labels. Other regions untouched. `exclude_id` so a replace/rekey does
not collide with self. Client still sends the typed name; daemon
re-suffixes on every join/rekey. `move` leaves the string alone.

This is **not** identity. Cookie bind still owns that. Two guests named
Ash in two plazas is allowed.

Self nameplate can stay the typed name; everyone else sees the GET
string. Fine.

Notes bullet landed under Shared pad again — move if you touch notes.

Still parked: CORS `*`, loopback host, nginx. No deploy.
