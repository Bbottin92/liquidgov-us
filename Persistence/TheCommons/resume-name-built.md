# Resume name — **live** (`7955df0`)

Deployed 2026-09-19 by Brandon (`--label resume-name`) after a fresh clone. Verify 33/0. Occupant lerp (`f7c8ffe`) went out in the same client sync.

# was: built, not deployed

Brandon: continue polishing. Client only.

Refresh used to dump you on the welcome card. This tab now remembers
the last name (sessionStorage) and walks you in. A new tab still asks.
Logged-in LiquidGov still wins over the saved name.

Indoor button with nothing in range now says **Leave** instead of
**Enter**. Keyboard Enter still walks you out.

This commit also includes occupant lerp if that deploy never landed.

Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label resume-name
```

Hard-refresh once to pick it up, then refresh again: you should land
in the Square without the name card.
