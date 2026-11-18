# Grok review of b28401f — not deploy yet

HouseStore writing `players` / `map_instances` / `placed` is the right
shape. Demo/lab still RAM. Ownership `403 not_your_house` on join/rekey/
place is correct now that the slot is durable. Kill-and-restart proving
chalk survives is the test that mattered — for **leave**. Civic untouched.

## Must-fix

The live client **does not call leave** when you walk out the door.
`exitToOutdoor` → next tick **rekey** to Homestead. Snapshot is only
wired on `/presence/leave` (tab close / `pagehide`).

Their test `POST /leave` then restart. Real play: walk out, restart,
resume is `DEMO_ENTRY` again. Chalk still there; **you** are not.

Fix: snapshot on **rekey away from** `instance:house:<self>` (read the
occupant position before the rekey erase, same as leave). Keep leave.
Still no 30s timer.

Also: `house_instance_for` INSERT instance + UPDATE player is two
statements **without** `BEGIN/COMMIT`. `schema.sql` already requires
that wrap. A crash between them leaves an orphan instance and the next
enter creates a **second** house (wall slot on the first, pointer on
the second). Wrap it. `set_wall_slot` already does.

## Should-fix (not this patch)

- Check `sqlite3_step` results (silent failed INSERT → `map_id` 0)
- `last_x` INTEGER truncates fractional tile coords (schema as written)

## Fail if “fixed” by

- Persisting demo/lab
- Guest `players` rows
- Deploying first
- A 30s thread "while we're here"

Paste `claude_persist_snapshot.md`. Then Brandon says **deploy**
(daemon + client; **daemon first** — new `house.cpp`). No civic, no nginx.
