# Per-frame `dt` clamp

Not a new lock. Do not add CCD. P1 is closed; this is why
`Math.min(rawDt, 1/20)` is in `frame()` and when to change it.

## What the code does

```
dt = min((t - lastT) / 1000, 1/20)   // 50 ms cap
```

Movement is `speed * dt` on each axis, then `boxBlockedAt`. There is no
raycast. If one step is larger than a wall, four-corner sampling can
start in grass and end in grass on the far side — the door is never
queried. That is the `dt = 1.0` “tunnel” Claude saw. It is not a
gameplay path.

## The real spike is a background tab

rAF pauses while `document.hidden`. First frame back, `t - lastT` can be
seconds. Unclamped at 76 px/s that is several tiles in one step, through
Homestead’s 1-tile facade. The clamp exists for that, not for 60 fps.

## Bound (keep 1/20 or derive it)

Player box is 12 px, tiles 16 px. Worst-case 4-corner tunnel through a
1-tile wall needs about **wall + box ≈ 28 px** in one step.

| Cap | Max step at 76 px/s | 1-tile door |
|---|---|---|
| none (`dt = 1`) | 76 px | tunnels |
| `TILE / speed` ≈ 0.21 s | 16 px | lands *in* the door, caught |
| `1/20` = 0.05 s (current) | **3.8 px** | safe ×7 |

Keep **1/20**. If speed changes, set

```
DT_MAX = TILE / (speed * 2)   // 8 px at 76 px/s, still < 16
```

and use `min(raw, DT_MAX)` for **both** player and passerby (same
`frame()` `dt` already).

Do **not**: swept AABB, sub-pixel CCD, “fix” the synthetic `dt=1` test.

## Clamp vs substeps

Clamp **drops** leftover time. After a 2 s tab-hide the passerby does
not catch up. That is correct for this walking game — nobody wants a
2-second slide through the Square.

Substeps (`while (acc >= DT_MAX) step(DT_MAX)`, cap 4–8) would simulate
the missing time. Don’t add them in P1/P2. Revisit only if passerby
desync on tab-return looks wrong.

Spiral of death: if you ever substep, cap the number of steps per
frame. Unbounded substeps + a slow frame = freeze.

## First frame

`start()` sets `lastT = now`. First rAF: `dt` ≈ 0 (and `|| 0` covers
negatives). Good. Don’t special-case it.

## Reviewer fail

- Removing the clamp because “60 fps is fine”
- Adding CCD to pass a `dt=1` unit test
- A second `dt` for the passerby that is not clamped
