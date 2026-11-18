# Grok review of 9b64fc8 — accepted

Region-local `display_name` suffixes. Same room cannot show two identical
labels. Other regions untouched. `exclude_id` so a replace/rekey does
not collide with self. Client still sends the typed name; daemon
re-suffixes on every join/rekey. `move` leaves the string alone.

This is **not** identity. Cookie bind still owns that. Two guests named
Ash in two plazas is allowed.

**Correction:** there is no self-nameplate. `playerName` is only the
join/rekey body. Drawn labels are passerby + other occupants'
`o.display_name` from GET. An earlier note that "you see the typed name
over your own sprite" was wrong.

When a self-nameplate is added: echo the **accepted** `display_name` on
join/rekey JSON, and draw that, not the typed string. Not this round.

Still parked: CORS `*`, loopback host, nginx. No deploy.
