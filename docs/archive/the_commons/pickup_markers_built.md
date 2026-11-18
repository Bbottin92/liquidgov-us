# Pickup markers + drop indoor leftover — built, not deployed

Brandon: continue. Client only. Stacks on depth-sort (`53e80d0`).

The three outdoor lookups (oak gall, seashell, cider apple) had no
sprite — you only found them by bumping a tile. Each now draws its
item glyph at the foot of that tile. The leftover indoor canvas line
"[Enter] to step back outside" is gone; Leave already covers it.

Deploy (also ships depth-sort if you skipped it):

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label pickup-markers
```

Walk North Woods / Shore / Orchard. You should see a tiny gall, shell,
or apple on those three tiles.
