# Inventory strip is live

Brandon ran `--label inv-strip` from `/home/bbottin/lg` at `a9a998d`.

- Backup: `/var/lib/commons/backups/commons.sqlite.pre-inv-strip-20260919T210642Z`
- Verify: 33 passed, 0 failed. items 11 unchanged, recipes 8, civic 200.
- Daemon on the box: `a9a998dd97a2` (script always rebuilds/restarts; no schema change).
- Client last: `index.html`, `style.css`, `js/interiors.js`, `js/interact.js`, `js/net.js`.

Hard-refresh `/commons/`. Chips appear after a lookup. Tap to change what's in hand.
