# Place banner — built, not deployed

Brandon: continue (polish). Client only.

The twelve region names were still stamped in ink on the grass — a P0
debug leftover. They are off the bake. Crossing a region (or walking
into a house, the lab, or the mine) shows a paper banner at the top
for two seconds, FireRed-style. Floor and wall item labels use the
same paper chip as names.

Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label place-banner
```

Hard-refresh. Walk south from the Square: the grass is clean, then
“Homestead Row” appears at the top.
