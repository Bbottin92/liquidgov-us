# Door polish — built, not deployed

Brandon: "go through and make improvements wherever you see fit."

Client only. The civic Quad-replace (`c91a31d`) is a separate deploy.

People will now arrive from the homepage. This pass is the other side of
that door:

* Welcome card: way back to LiquidGov. In-game: a paper pill, top-right,
  same `/`. The Hall still opens proposals; this opens the site.
* Tab description + civic favicon, so a shared `/commons/` link doesn't
  look like an unnamed tab.
* Duplicate `[Look]` painted on the grass is gone. The Enter button
  already says the word; on a keyboard that button hides too (Enter
  still works). Touch keeps the brass circle.

`client_files_unit` 71/0.

Deploy with the rest of the undeployed client (fill-map, canopies) and,
separately, civic for the Quad door:

```
cd /home/bbottin/lg
git fetch origin && git reset --hard origin/master
# civic first (Quad → Commons), then client:
./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label polish
```
