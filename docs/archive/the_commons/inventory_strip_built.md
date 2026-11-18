# Inventory strip — **live** (`a9a998d`)

Deployed 2026-09-19 by Brandon (`--label inv-strip`). Verify 33/0.

# was: built, not deployed

Brandon: continue (after objects live). The play gap was “how do I use
these” — you could hold 11 things and only see a toast.

## What

A chip strip (`#invHud`) at the top-left, same DOM chrome as the
reconnect badge. One chip per item you actually have, with colour, short
name, and count. The item in hand (what wall/floor Place uses) is
outlined. Tap a chip to put that item in hand. Empty = hidden.

Not a backpack. Not a crafting screen. Counts still never go down on
place. Guests still vanish on refresh. Daemon untouched.

## Deploy (client only)

```
cd /home/bbottin/lg
git pull
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label inv-strip
```

Hard-refresh. Look something up — the chip appears. Tap another after
two lookups to change what’s in hand, then Place in the house.
