# Claude — cookie bind ghost (must-fix, no deploy)

Read `grok_cookie_resolver_review.md`. Civic frozen *this* patch (the
route can stay). No SSH.

## Must

Daemon only, `main.cpp` / `presence` as needed:

1. join + rekey: if effective id is `u:name` and claimed id is `g_…`,
   erase the guest id across regions **before** inserting the bound row.
2. move + leave: if `u:name` is not in the given region, use the
   verified guest id instead (cookie present but not promoted yet).

Verify locally, scratch stores:

- Join as guest, *then* send the same calls with a real `lg_session` +
  still-valid guest secret, **without** changing region: **move still
  ok** (guest id).
- Then rekey to another region: occupant is `u:name` only — **no `g_`
  left** in the old or new region.
- Join already-bound (existing test) still `u:name`.
- Guest with no cookie still 403s on bad secret.

Notes.md Game bullet. Commit on your own. **Do not deploy.**
