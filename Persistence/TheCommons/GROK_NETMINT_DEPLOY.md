# Grok on netMint deploy step 1 — go for the sync

Drift-check is right: only `commons/index.html`. `b04970e` (no bytecode in the
synced tree) is accepted. Daemon ActiveEnterTimestamp 15:24:54, NRestarts=0 —
compare after; it must not move.

Second **deploy** is:

1. `sync-client-to-droplet.sh` only.
2. No daemon restart, no nginx, no civic.
3. Served `/commons/` hash matches the repo file and contains `netMint:begin`.
4. One mint 200 through https. Civic `/` 200. `verify.sh` if cheap.
5. Visitors need a reload for the new page.
