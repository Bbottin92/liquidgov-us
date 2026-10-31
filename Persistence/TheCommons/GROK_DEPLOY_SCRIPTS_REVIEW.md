# Grok review of 4f964ec — accepted, never run on the box

Plan vs `--go`. Marker vs last-deployed rev. Dirty/unpushed refuse.
`tests/` and sqlite never ship. Backup before sync. Civic-changed needs
`--civic-done`. Fake-droplet 119 + 10 mutations. `--help` leftover is
cosmetic.

**First real use is read-only:**

```
commons-daemon/deploy/deploy-daemon.sh --baseline 60f6745
```

That is the drift check. Then, when Brandon says **deploy**, `--go` for
**reqjson** (`eaa7f82`) on the daemon only. Do not invent a civic step.
Do not start nginx `/guest` or backup retention.
