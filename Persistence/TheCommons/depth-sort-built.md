# Depth sort + foot shadow — built, not deployed

Brandon: continue. Client only.

Actors used to draw passerby, then others, then you — always on top.
They now sort by south-edge, FireRed style: whoever stands further
south is in front. Each body also gets a 10×3 foot shadow so they sit
on the grass instead of floating.

Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label depth-sort
```

Walk past someone (or the passerby). They should pass in front when
south of you, and both sprites should have a small shadow at the feet.
