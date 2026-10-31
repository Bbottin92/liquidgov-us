# IndexedDB — larger caches, not truth

Not a new lock. Does **not** reopen `inventory.md` (RAM in V1) or
`schema.sql` (SQLite is the game ledger after glue).

IndexedDB is the browser’s “big document / blob” store. Use it when you
always load a whole object and can afford it to be stale. Never as the
second copy of a row you also mutate.

## Scale (so we don’t cache the wrong thing)

| Thing | Size | Store |
|---|---|---|
| P0 bake, 1152×1280 RGBA | ~5.6 MB | **RAM canvas.** Rebuild from `Uint8Array` is a millisecond fillRect loop. IDB round-trip is slower and survives a refresh we *want* to reset. |
| Guest `inventory` | a handful of counts | **RAM.** `inventory.md`. |
| Presence | bytes × occupants | **RAM** (Redis later). 2 Hz. Not IDB. |
| House snapshot `{item,x,y,rot90}[]` | small JSON | IDB is a *cache of* `placed` rows, same rule as the JSON file: **derived, not the only copy.** |
| Sprite atlas / tileset PNG | 10s–100s KB | IDB (or Cache API) once art is real files, not fillRect. |
| Photo before civic POST | 1 image | IDB as a **staging** buffer. Civic blob + feed is truth (`glue.md`). |

## Fits later (post-glue web client)

- Tileset / sprite atlas, keyed by content hash, so a second visit doesn’t
  re-download.
- Last house snapshot for instant interior paint, then reconcile with
  SQLite `placed`.
- Last captured photo waiting on `/blob` success, then delete.

Quota is per-origin, can be evicted, is not shared across devices. Treat
it like a disk cache, not a save file.

## Does not fit

| Temptation | Why |
|---|---|
| Guest inventory / position | Same as `localStorage`: a fake account. |
| Trades / inventory mutations | Need `BEGIN/COMMIT` and 3NF. That’s SQLite. IDB transactions are not that ledger. |
| Friends | Civic **query**, RAM TTL. No second graph. |
| LGP1 dump in the browser | Civic store stays on the droplet. |
| Caching the bake in V1 | Cheaper to paint 5,760 rects than to serialize a bitmap. |

`sessionStorage` / `localStorage` are worse for “larger caches” — both
are small, sync, string-only. If it doesn’t fit RAM and isn’t a blob,
it doesn’t belong in the browser at all.

## V1 / P3

Do **not** add IndexedDB in `commons/index.html`. The preview has no
downloaded atlas, no bound house, no photo mode. P3 is outdoor dressing.

## Reviewer fail

- `indexedDB.open` in the P0–P3 preview
- Writing inventory counts into IDB “so refresh feels better”
- Using IDB as the house’s only copy
- Storing friends or LGP1 tags in IDB
