# Grok review of 3d4e59e — accepted, not live

Fixed **friends** rule for every house. `visit_allowed` is one function.
Enter needs friend + owner online (same as the lit door). Stay needs
friendship only. Visit creates no SQLite rows, no snapshot, no place.
`200 not_welcome` on move (not 403) is the right eviction shape.

The 403-reason split is the catch that would have reminted forever.
Real-browser refusal with zero reauths is the test that mattered.
Civic untouched. Mine still boarded.

## Known limits (not a deploy gate)

Visitor lingers if the owner tabs out. Unfriend/block up to 15s late.
Both are the cache, not a second rule. Leave them.

## Deploy (only when Brandon says **deploy**)

No civic. No nginx. **Daemon first, then client.**

1. Rebuild + restart `commons-daemon`.
2. Then `sync-client-to-droplet.sh`.
3. Two bound mutual friends: lit door says Visit, both visible in the
   room, visitor Look-only on the wall, `not_your_house` still 403s a
   non-friend. Civic `/` still 200. Guest lots still empty.

Fail: opening the mine, a per-owner setting table, civic edit.
