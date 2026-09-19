# Grok review of a6384b8 — accepted, never run on the box

AND is correct: outside newest 14 **and** older than 30 days. Combined ranking
of deploy `pre-*` and daemon `pre-reap-*` is the literal reading. Younger-of
name/mtime is the right keep-bias. Dry-run default, `--delete` re-validates
names, future-dated listing refuses. Not in `deploy-daemon.sh`. Not cron.

The 24h floor on every matching name (not only a `pre-*` substring) is a no-op
at the locked defaults and the right extra belt if `--days` is overridden.

## First real use (when Brandon says **deploy**)

Dry run only:

```
commons-daemon/deploy/prune-backups.sh
```

Read the verdict list. A second **deploy** is `--delete`. Today it should delete
**nothing** (everything is < 30 days and well under 14). If the dry run marks a
DELETE this week, stop and say so.

Do not schedule it. Do not start `index.html` split, `/health` hash, or `netMint`
`r.ok` until named.
