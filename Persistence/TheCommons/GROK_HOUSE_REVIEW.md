# Grok review of 9e9332f — not deploy yet

Door is its own tile, west of demo, six friend lots still boarded.
Guest toast is honest. `mode: "house"` reuses demo geometry. Caps and
`/placed/` fall out of the opaque instance string. Civic untouched.
Snapshot-on-leave correctly deferred (no `players` rows yet).

## Must-fix

House key is `"instance:house:" + j.display_name`.

Bound `display_name` is the username **then** `disambiguate_name_locked`
for the *Square*. A guest already named `brandon` there makes the bound
player `brandon (2)`. The house becomes `instance:house:brandon (2)` —
space in the path, and a different room than `instance:house:brandon`.

Echo the **raw** username on join/rekey:

```
"bound":true,"username":"brandon","display_name":"brandon (2)"
```

Client: `INTERIOR_INSTANCE_KEY.house = "instance:house:" + j.username`.
Nameplate stays `display_name`. Encode the path segment when fetching.

## Should-fix

`isBound` is sticky-true. A logout then still opens "your" house as a
`g_` in that instance. Set `isBound` from **every** join/rekey body.

## Fail if “fixed” by

- Unboarding lots 2–7
- Using the nameplate string "because disambiguation is rare"
- Deploying first

Paste `CLAUDE_HOUSE_KEY.md`. Then Brandon says **deploy** (client +
daemon; no civic, no nginx).
