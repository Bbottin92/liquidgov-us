# Lit friend door — built, not deployed

Brandon: ok continue. Client only.

A friend's Homestead lot used to overlay a gold square on the boards.
It now stamps the same wooden house-door tile as your own door, name
still above. Offline lots stay boarded.

Stacks on item glyphs (`f57d13d`) if that deploy hasn't run.

Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label lit-door
```

Log in, walk Homestead. A friend who's in the Square should have a
real door, not a brass chip.
