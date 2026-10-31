# Item glyphs — built, not deployed

Brandon: ok (keep polishing). Client only.

Floor drops, the wall slot, and inventory chips were flat color
squares. Each of the 11 lab items now has a 16×16 glyph, baked once
into a small atlas and blitted. Labels stay.

Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label item-glyphs
```

Walk into your house, place something on the floor. It should look
like a tiny object, not a color chip.
