# Session inventory — V1

Not a new lock. `v1.md` already decided this. P2’s `var inventory = {}`
is that decision in code. Do not “fix” refresh-loss.

## What exists

| Layer | Where | Survives refresh? |
|---|---|---|
| P2 preview | `inventory = {}` in the page | **No** — correct |
| Guest position / name | RAM, same page | No |
| Bound future | `schema.sql` `inventory` / `placed` | Yes, after cookie glue |
| Civic | LGP1 / `liquidgov_http` | Never for game items |

A lab lookup does `inventory[item] += 1` and a toast. Nothing else reads
it. Wall-slot stays empty. No bag UI (`art.md`: no RPG inventory screen).

## Do not persist in this preview

| Temptation | Why not |
|---|---|
| `sessionStorage` | Survives refresh in the same tab. Looks like a guest account. `v1.md`: vanish on disconnect, same as position. |
| `localStorage` | Survives tab close. A second password by another name. |
| IndexedDB / SQLite | Locked out of V1. Schema is for the droplet sibling. |
| Cookie / `lg_username` | Glue, post-V1. Then write `schema.sql`, not `localStorage`. |

If pickups disappearing on refresh feels bad, the product answer is
**bind the house**, not stash a bag in the browser.

## What *is* still missing (not persistence)

The object is **write-only**. A second Look at the same station just
toasts again. Fine for a lookup table. If P3 wants a tell: the toast can
say the count (`chalk block ×2`). Still RAM. Still no screen.

Placing a bottle on the table or in the wall-slot is **glue + `placed`
rows**, not a `localStorage` cache of the RAM object. Don’t fake it.

## Reviewer fail

- `sessionStorage.setItem("inventory", …)`
- Painting a fake canvas into the wall-slot
- Creating `commons/*.sqlite`
- Treating refresh-loss as a P2/P3 bug
