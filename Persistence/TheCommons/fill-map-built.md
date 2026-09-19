# Finish populating the map — built, not deployed

Brandon: "okay but can we finish populating the map and drawing the various objects?"

The ten land regions were already dressed (scatter B, 168 props). The hole
was Creek and Shore: P0 painted both entire 24×20s as water, so you could
only walk the 2-tile bridge. That contradicted atlas.md (ribbon + banks;
Shore dock + sand).

What changed (client only, same tileset, no new collision rule):

* Creek/Shore default to grass like everywhere else, then a water ribbon
  at y=66–72. Creek north/south banks are walkable grass. Shore south of
  the ribbon is sand (`T_DIRT`).
* Shore pier: a 2-tile-wide `T_BRIDGE` from the east-west jog south into
  the water, meeting the existing seashell beach so it is not a pocket.
* Bank scatter: Creek 2→19, Shore 2→19. Trees, logs, flower beds, crates,
  a low sitting wall. Total scatter 168→202. Same six prop tiles.

`world_scatter_unit` 14/0 (flood-fill still reaches every walkable tile,
nothing on the spine/jog/seams/doors). `client_files_unit` 69/0.

Stacks on canopy (`fe1c2b1`) if that deploy hasn't run.

Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label fill-map
```

Walk south from Homestead onto Creek grass, then along the banks, then
out onto the Shore pier. You should not be trapped on a two-tile bridge
in a flooded square.
