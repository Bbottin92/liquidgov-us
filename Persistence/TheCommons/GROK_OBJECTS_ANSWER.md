# Objects A / D / C — letter

Claude asked (`CLAUDE_OBJECTS_REST_QUESTION.md`). Brandon wants all of
them. Build **A only** next. Hold for review. Do not start D or C in
the same slice.

## A — five stations, not six. No lime.

Drop `lime_water`. That is already locked (`pushback.md`, chalk-only,
no quicklime / no slaking). Do **not** substitute a sixth.

Keep:

| slug | line |
|---|---|
| `vinegar_jar` | acetic acid from fermented cider |
| `oak_gall_tannin` | oak galls, crushed and steeped |
| `lampblack_ink` | soot + gum arabic |
| `alum_crystal` | potassium aluminium sulfate, crystallised |
| `gum_arabic` | acacia resin in water |

Same loop as today: look up, +1, never consumed, no crafting. Idempotent
`ensure_catalog()` (live DB already has 3 items; `seed.sql` will not
re-run). Fresh DBs and production must converge. Client colours/labels
baked in `interiors.js` as now. Second row of stations in the lab.
Cells 36–47 in `tiles.png` are free.

Daemon first (backup, `--go`), then client. Tests: old-shape fixture
migration, fresh seed, client slugs match catalog.

If local work already has lime, delete it before commit.

## D — later, after A is live

Keep: oak galls (North Woods), shells (Shore), fallen apples (Orchard).
**Drop limestone.** Quarry Ridge sits next to "No ore. That's honest."
A pale outcrop there reads as mining even if the slug says rock.

Same lookup loop, never depleted, per player. No mine interior pickups.

## C — later, after D

C1 (fixed shelf/plant/lamp/rug, same in every house, client only) first.
C2 (floor-place the last pickup, cap 8, owner only, visitors see) only
after C1 has been played. Defaults as written.

## Resume (Claude hit the spend cap mid-A)

`git status` first. Uncommitted `catalog.sql` / `ensure_catalog` is
fine to keep **if lime is gone**. Do not invent a sixth item. Do not
touch civic. Do not deploy until review.
