#!/usr/bin/env bash
# Syncs commons-daemon/ source to the production droplet's /opt/commons-daemon
# (a sibling path -- NOT inside /opt/liquidgov, never the LGP1 store; see
# sibling.md). Does not build, chown, or restart the service; run those
# separately afterward (see Persistence/TheCommons/deploy-plan.md).
#
# Modeled on deploy/sync-to-droplet.sh (the civic script) for the same
# reasons that one exists: no hand-typed rsync command for a production
# target. This script's own reason to be careful is different from
# civic's (no live account data here to overwrite) but just as real:
#
#   commons-daemon/schema.sql is a git symlink to
#   ../Persistence/TheCommons/schema.sql (deduped there on purpose --
#   see notes.md, 2026-09-16). A plain rsync copies the symlink itself,
#   not its target. On the droplet that symlink would point at
#   /opt/Persistence/TheCommons/schema.sql, which does not exist --
#   Database::migrate() would fail to find the file at daemon startup.
#   `-L` (--copy-links) makes rsync send the real file content instead
#   of the symlink, exactly the fix GROK_DAEMON_REVIEW.md's deploy-gate
#   list already named ("Sibling install should copy the real file").
#
# Usage: commons-daemon/deploy/sync-to-droplet.sh [user@]host[:path]
#   defaults to root@143.198.159.58:/opt/commons-daemon/

set -euo pipefail

DEST="${1:-root@143.198.159.58:/opt/commons-daemon/}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/"
KEY="${LIQUIDGOV_DEPLOY_KEY:-$HOME/.ssh/liquidgov_deploy_key}"

echo "Syncing $SRC -> $DEST (excluding build/.git/data, dereferencing the schema.sql symlink)"
rsync -az -L -e "ssh -i $KEY" \
    --exclude build \
    --exclude .git \
    --exclude 'data/*.sqlite' \
    --exclude 'data/*.sqlite-shm' \
    --exclude 'data/*.sqlite-wal' \
    "$SRC" "$DEST"

echo "Sync done. Still need on the remote:"
echo "  cmake -S /opt/commons-daemon -B /opt/commons-daemon/build"
echo "  cmake --build /opt/commons-daemon/build -j1   # 512MB box, matches civic's own -j1"
echo "  chown -R liquidgov:liquidgov /opt/commons-daemon"
echo "  mkdir -p /var/lib/commons && chown liquidgov:liquidgov /var/lib/commons"
echo "  systemctl daemon-reload && systemctl enable --now commons-daemon"
echo "See Persistence/TheCommons/deploy-plan.md for the full sequence and verification steps."
