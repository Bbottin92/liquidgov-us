# Walk behind trees — built, not deployed

Brandon: continue. Client only. Stacks on depth-sort + pickup-markers.

Tall tiles (tree, hedge, wall, adit, sign, notice-post) now sort with
actors by south edge, so a 24px sprite passing south of a tree is in
front, and passing on the north/same-row side can tuck behind the
canopy. Same idea indoors with interior walls.

Also a must-fix: depth-sort's foot-shadow used `camX`/`camY` as if they
were global. In this file's strict mode that is a ReferenceError the
moment anyone draws an actor. It wasn't live yet. This pass threads
the camera in. Do not deploy `53e80d0` without this.

Deploy (ships depth-sort, pickup markers, this):

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label behind-trees
```

Walk south of a North Woods tree, then stand just north/beside one.
You should pass in front from the south and tuck behind from the north.
