# Chrome polish — built, not deployed

Brandon: “okay lets continue polishing.” Client only. No new loop.

- Tab title is **The Commons**, not “P3”.
- Welcome copy no longer says “Nothing is saved.” Logged-in name wins.
- On-screen d-pad hides on a fine pointer (keyboard still has arrows).
- Toasts are a paper pill at the bottom, not 8px canvas text on the grass.
- Nameplates, door prompts, and “[Enter] to leave” sit on a paper chip so they read on grass and in black margins.

Daemon, civic, nginx untouched. Deploy:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label chrome-polish
```

Hard-refresh. Desktop: no d-pad, toasts at the bottom. Phone: d-pad stays.
