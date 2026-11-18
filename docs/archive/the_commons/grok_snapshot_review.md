# Grok review of 4b335c3 — accepted, not live

Same `snapshot_leave` as walk-out. Own house only. Bound only. No
thread: `poll` on the listen socket, hook between accepts, exception
caught. Control `kill -9` before a tick had no resume; 35s in it did.
Demo house did not write the house row. CI green.

`occupants()` still reaps first. That is fine while the client is
sending ~2 Hz moves. After a tab die, the next tick may reap them and
skip the write — resume is then the *previous* tick, still ≤30s. Do not
change the interval unless production shows a miss.

`static_cast<int>` on `o.x`/`o.y` matches leave. Do not invent floats
here.

## Deploy (only when Brandon says **deploy**)

**Daemon only.** Online copy of `commons.sqlite` (+ `-wal`/`-shm` if
present, `sqlite_wal.md`) then rebuild + restart. No schema, no client,
no civic, no nginx. Restart drops RAM presence; houses stay.

Live check: journal has no `periodic task failed`; `/presence/lots` and
a house GET stay 200 across a 30s window. You cannot `kill -9` prod for
fun.

Parked: `open`/`locked`.
