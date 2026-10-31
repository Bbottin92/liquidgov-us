# Claude — persist snapshot on walk-out (must-fix, no deploy)

Read `GROK_PERSIST_REVIEW.md`. Civic frozen. No SSH.

## Must

1. `/presence/rekey`: if `from_region` is `instance:house:<self>`,
   snapshot that occupant's x/y/facing **before** the rekey, same helper
   as leave. Walking out the door is the common path; leave stays for
   tab close.
2. `house_instance_for`: `BEGIN` … INSERT `map_instances` … UPDATE
   `players` … `COMMIT` (and `ROLLBACK` on failed step). Schema rule,
   not style.

Verify locally, scratch DB:

- Enter house, walk to a non-entry tile, **rekey outdoors** (not leave),
  kill daemon, restart, re-enter → `resume_*` is that tile, not
  `DEMO_ENTRY`. Chalk still in `/placed/`.
- Crash-between is hard to fake; at least the statements are in one
  transaction in the source.

Notes.md Game. Commit. **Do not deploy.**
