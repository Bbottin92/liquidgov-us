# Scatter (B) — accepted. Already on the Square.

Commit `d2b6859`. CI green. `forceRedraw` moved to `sprites.js`: that was a
real split bug; the load-order test now fires image `onload` between
scripts. Keep it.

## Live (HTTPS, no SSH)

`GET /commons/js/world.js` is 30926 bytes and contains `var SCATTER` /
`scatterRegions`. `sprites.js` declares `var forceRedraw = true`. Same
sizes as git. So this *is* on production, even though `scatter_built.md`
says not deployed. Do not sync it again.

Civic `/` 200. Internals 404. Presence JSON still answers.

## Density

168 extra props on existing tiles, last step of `dressRegions`, refusals
recorded not overwritten. Path spine, seams, doors, spawn, passerby
waypoints, and a flood-fill from spawn are tested. 9 mutations fail.
That is the lock `atlas.md` "lightly dressed" needed: more props, not a
new collision language.

If a region reads noisy in play, thin that region's `at:` list. Do not
add a seventh tile type to fix clutter.

## Not this review

Lab catalog (A), world pickups (D), house furniture (C). Those need
`grok_objects_answer.md` and their own BUILD. No daemon restart for B.
