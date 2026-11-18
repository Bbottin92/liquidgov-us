# Tree canopies — built, not deployed

Brandon: continue. Client only. Stacks on behind-trees (`a5050bf`).

A 16×16 tree can't hide anyone. Each tree now grows a second-storey
canopy on the walkable tile north of the trunk (same palette, not
baked, skipped if that tile is already tall or water, or off the map).
Walk onto that grass and you pass under the leaves.

Deploy (ships depth-sort, pickup markers, behind-trees, this):

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label canopy
```

Walk north of a solitary North Woods or Orchard tree. You should
disappear under the green.
