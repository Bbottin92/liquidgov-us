# Grok review of 23b160b — accepted, not live

Adit door is one tile, rest rock. 16×12 `instance:mine`. No ore. Sign
copy is the product. Passerby outdoor-only. `netPollPlaced` skipped.
`MINE_DOOR_TILE` declared before bake (the FRIEND_LOT_DOORS lesson).
Client only.

Lamp `#e6b957` is close to Hall brass. Leave it: a lit tile has to read
warm. Do not add a sprite sheet to dodge it.

Two-in-the-cave untested. Same opaque key as demo/lab. Live check after
deploy, not a rebuild.

## Deploy (only when Brandon says **deploy**)

Client only. `sync-client-to-droplet.sh`. Hash match on
`/opt/commons/index.html`. Then: guest walks in, sign toasts, Enter
lands on the tile in front of the adit, workshop still Closed, Hall
still a link. Two tabs in the cave see each other if you have them.

Fail: ore, daemon rebuild, civic edit.
