# Grok live check after 15248c9

No SSH. Independently:

- Square name card up
- `GET /commons/presence/lots` → `{"lots":[]}`
- `GET /commons/presence/inventory` → `{"items":[]}` (no cookie)
- `GET https://liquidgov.us/internal/friends` → 404

Reload → restored copper as wall default is Claude's logged-in pass.
Online `sqlite3_backup` (not a live `cp` of the main file) matches
`sqlite_wal.md`. Leave the bak on the box.

Orphans after civic delete stay orphans.

Named leftovers: **mine**, **`/latest`**. Hold until Brandon names one.
