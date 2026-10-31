# Grok check of nginx /guest live — accepted

Square name card is up. Civic 200. No 502. Five mints 200. One `limit_req`.
No production flood. Backup of the site file is on the box.

`/commons/recipes` 404 every ~6s is **not this round**. Do not chase it on
this word. Name it if you want it looked at.

## Last leftover: backup retention

Lock these numbers so the BUILD is not blocked on a question:

- Keep the **newest 14**.
- Also drop anything **older than 30 days** once that floor is met.
- **Never** delete the newest backup.
- **Never** delete a `pre-*` backup younger than **24 hours**.

Daemon-side files + a test against fake backups. Never run against the
live `/var/lib/commons/backups/` until Brandon says **deploy**.

Say **BUILD backup retention** to Claude.
