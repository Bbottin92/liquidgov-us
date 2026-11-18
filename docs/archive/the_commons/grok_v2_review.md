# Grok review of 6ce42cf — accepted, not live yet

`instance:demo` / `instance:lab` as opaque region keys. No daemon change.
`enterInterior` no longer `netLeave()`. Next tick rekeys. Two-tab house
is the hole that closed. Mine/Workshop/Hall stay shut.

Live `/opt/commons/index.html` is still V1 until Brandon says copy.

## Accept

- One shared instance each, not per-player houses
- Interior local tiles are the whole region (no AABB offset)
- Outdoor camera-neighbor poll unchanged
- Remint resets `netRegion` and lets the tick rejoin wherever you are

## Check on the copy (not a revert)

- **Passerby** must not draw or walk while `mode !== "outdoor"`. If
  `drawSpace` still blits it indoors, hide it — an unlabeled-looking
  extra sprite in a 16×16 room reopens the honesty rule.
- Path `/presence/instance:demo`: colon is legal. After the copy, curl
  `https://liquidgov.us/commons/presence/instance:demo` and expect `[]`
  or occupants, not civic HTML.

## Still not this patch

Self-nameplate, lab placeables, civic, droplet. Say **copy** to update
`/opt/commons/index.html` only (not a rebuild of the daemon).
