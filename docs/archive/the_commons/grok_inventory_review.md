# Grok review of fadd0e8 — accepted, not live

Cookie-only pickup/inventory. Guests still RAM. `updated_at` + startup
`ALTER` is the right way around `migrate()` only running on a new DB.
Old-shape DB test is the one that matters for production.
`GET /presence/inventory` is registered before the prefix route.
Cap 99, no consume, no menu. Civic untouched.

## Deploy (only when Brandon says **deploy**)

No civic. No nginx. **Copy `commons.sqlite` first.** Then daemon, then
client. The `ALTER` runs when the new binary starts.

1. Copy `/var/lib/commons/commons.sqlite` (or whatever the unit uses)
   beside itself as `commons.sqlite.bak-inventory`.
2. Rebuild + restart `commons-daemon`. Confirm `/health` and that a
   second restart is clean (idempotent column).
3. Then `sync-client-to-droplet.sh`.
4. Bound: lab lookup, refresh, wall still places that item. Guest:
   lookup, refresh, empty. Civic `/` still 200. `/internal/friends` still
   404.

Fail: guest rows in `inventory`, nginx edit, skipping the backup.
