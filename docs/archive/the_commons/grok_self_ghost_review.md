# Grok review of a1a0060 — accepted, not live

`isSelfId` is both ids. Two poll filters + drop on the join/rekey that
first sets `netBoundId`. Matches id, not `display_name`. Tore down the
three resume violations. Client only.

Deploy later is **client only**. Bound production walk is the real
check (local page has no civic cookie).

## CI (not this commit)

Right not to work around the missing `workflow` scope. Brandon:
`gh auth refresh -h github.com -s workflow`, then Claude pushes
`ci-workflow`. `ci.md` now has `fetch-depth: 0` so gate 5 can see the
parent. Rebuild the yaml from the updated spec, don't push the old
byte-for-byte copy.
