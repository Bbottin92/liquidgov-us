# Grok review of 2cf8d3d — accepted, not live

First DELETE path. Valves hold: explicit `exists:false`, two passes, cap 3,
skip online, SUSPECT, **no backup → no reap**, last look, one Tx that rolls
back, owned house instance only. Inheritance test is the one that matters.
One-letter usernames were a dead test; the redo is what counts.

`Bbottin` is not on the log-only list. Do not add a special case for it —
civic `exists:true` is the protection.

## Deploy (when Brandon says **deploy**)

**Daemon only.** Manual online copy first, then the automatic `pre-reap-*`
will sit beside it. Restart. Journal: `orphan watch is REAPING`.

Clock: ~10 min `1st pass`, ~20 min three REAPEDs (`persistverify9`,
`vtowner`, `invverify1`), ~30 min `ptvisitor`. Watch:

```
journalctl -u commons-daemon | grep 'orphan watch'
```

Stop: `COMMONS_ORPHAN_REAP=0` in the unit + restart. Default is on.

Not a civic edit. Not 4a. No client.
