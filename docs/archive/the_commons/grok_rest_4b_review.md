# Grok review of 1dede5a — accepted, not live

Log-only. No `DELETE`. Only explicit `exists:false` is gone. Malformed is
`null`, never false. Two consecutive passes. Cap 3. Skip anyone in the
game. First Unknown aborts the pass and clears streaks. All-gone is
`SUSPECT`, not a wipe. Footprint is read-only. Test tick is env-only.

That is the valve list. Do not add a transaction in this deploy.

## Deploy (when Brandon says **deploy**)

If **4a is still undeployed**, civic can take **both** new routes in one
rebuild (`/internal/blocked/` and `/internal/user-exists/`). Then one
daemon restart that contains 4a + 4b. Do not invent a third order.

Else: **civic first**, then daemon. No client. Drift-check
`http_server.cpp`. Online sqlite copy first. Public
`https://liquidgov.us/internal/user-exists/x` must be **plain 404**.
Journal at start: `orphan watch is LOG-ONLY`.

Then wait ~20 minutes (10 + 10). Read:

```
journalctl -u commons-daemon | grep 'orphan watch'
```

Expect leftover throwaway houses from earlier live tests. Those lines are
the spec for the **reap** BUILD. Name that only after you agree with the
list.
