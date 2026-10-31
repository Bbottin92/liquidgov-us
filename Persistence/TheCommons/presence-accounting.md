# Presence accounting

Not a BUILD. Complements `presence.md`. The 92e379d patch made `join`
replace-or-move and stopped `rekey` from `--total_` on a miss. This file
is *how to keep that true*, not a new feature.

Presence is RAM. Guests never hit disk. Bound `players.last_*` is a
leave/~30s snapshot, not a live table.

## Two numbers that must match

| Number | Meaning | Cap |
|---|---|---|
| Occupants in one region list | who GET `/presence/<region>` returns | 24 |
| Distinct `id`s in the whole store | who exists at all | 96 |

An `id` is a **person**, not a row you may duplicate. Accounting that
allows two rows for one id will fill the 96 cap with one tab.

## Methods

### 1. Cached `total_` (what shipped)

`++` on insert, `--` on erase. Cap check is O(1).

Lies the moment a mutation path forgets a `++`/`--` (duplicate join,
`rekey` decrementing a miss, dest-full after already `--`). 92e379d
closes those holes **in the current paths**. A future path can reopen
them. Treat `total_` as a **shadow**. If it ever disagrees with a walk
of the maps, the maps win — or you do not have a cache.

### 2. Derive the global count (honest, cheap)

`total = sum(list.size())` or `id_index.size()`. At cap 96 this is
nothing. Region cap is `list.size()` of the dest. **No shadow.** The
code that shipped could drop `total_` and ask `id_index.size()`.

### 3. `id → region` index (structural uniqueness)

```
id_index:  map<id, region_key>
regions:   map<region_key, vector<Occupant>>  // or map<id, Occupant> per region
```

Insert: if `id` in index, it is a move/replace, not a second person.
`join` and `rekey` become the same function: “put this id in that
region.” Ghosts are impossible without a bug in the index. Global cap =
`id_index.size()`. This is the method that does not need a careful
`total_`.

### 4. Time accounting (the other axis)

Count is “how many.” **Lease** is “still here.” `last_update_ms` plus a
~30s reap (`presence.md`) is how a crashed tab releases a cap slot.
Without it, `total_` is correct and the plaza is still full of ghosts.
Not implemented. Should-fix before nginx, not this file’s job to code.

`move` must stamp `last_update_ms` or the lease thinks walkers are idle.

## What not to do

- A presence **SQLite table** (schema forbids it)
- Client-chosen `id` as a security boundary (impersonation once public)
- 10 Hz writes of `last_*` (snapshot on leave / ~30s only)
- Two occupancy graphs (game friends vs civic friends)

## For this repo

Keep 24 / 96, RAM, loopback. Next presence edit should prefer an **id
index** (method 3) over another clever `total_` patch. Body cap 64 KiB
and idle reap are still open; deploy is not.
