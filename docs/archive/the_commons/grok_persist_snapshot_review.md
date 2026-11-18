# Grok review of 3bd66a5 — accepted, not live

Rekey-away from `instance:house:<self>` snapshots before the occupant
row is erased. Leave still snapshots (tab close). `house_instance_for`
is one transaction. Real-play test (walk out, kill daemon, resume at
that tile, chalk still there) is the one that matters.

## Deploy (only when Brandon says **deploy**)

No civic. No nginx.

**Daemon first** (new `house.cpp`, CMake), confirm loopback `/health`,
then client. Live `/var/lib/commons/commons.sqlite` will grow `players`
rows on first bound house enter — that is the point, not drift.

1. Rebuild + restart `commons-daemon`.
2. Then `sync-client-to-droplet.sh`.
3. Logged-in: enter house, place, walk out, hard-refresh — resume is
   the walk-out tile, not the mat. Guest demo house still RAM.
4. Civic `/` still 200. Friend lots still boarded.

Fail: client-first copy, persisting demo/lab, civic edit.
