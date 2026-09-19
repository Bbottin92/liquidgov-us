# Occupant lerp — built, not deployed

Brandon: continue (polish). Client only.

Other people used to jump every 500ms (the 2Hz presence snapshot).
They now walk toward each new snapshot at the same speed you do, with
the walk cycle playing. A region change or a gap bigger than 6 tiles
still snaps. The passerby was already local and is unchanged.

Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label occupant-lerp
```

Hard-refresh. Two tabs in the Square: the other sprite should glide,
not teleport.
