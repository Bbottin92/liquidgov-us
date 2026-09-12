#!/usr/bin/env bash
# Syncs liquidgov-platform/ source to the production droplet's /opt/liquidgov
# (which is NOT a git clone -- see the repo's other deploy docs). Does not
# build, chown, or restart the service; run those separately afterward.
#
# Why this script exists, not a hand-typed rsync command: on 2026-09-12 a
# hand-typed `rsync -az --exclude build --exclude .git ./ root@...:/opt/liquidgov/`
# silently overwrote the live liquidgov_accounts.lgp (and would have done the
# same to liquidgov_blobs/) with a stale local copy that happened to be
# sitting inside liquidgov-platform/ from an earlier local test run -- real
# account data was lost with no backup anywhere. The source tree and the
# live data directory share a parent folder, so "sync everything except
# build and .git" is never actually safe here. This script hardcodes every
# exclusion that matters so a future deploy can't reintroduce that mistake
# by hand-typing an rsync command from memory.
#
# Usage: deploy/sync-to-droplet.sh [user@]host[:path]
#   defaults to root@143.198.159.58:/opt/liquidgov/

set -euo pipefail

DEST="${1:-root@143.198.159.58:/opt/liquidgov/}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/../liquidgov-platform" && pwd)/"
KEY="${LIQUIDGOV_DEPLOY_KEY:-$HOME/.ssh/liquidgov_deploy_key}"

echo "Syncing $SRC -> $DEST (excluding build/.git/data files)"
rsync -az -e "ssh -i $KEY" \
    --exclude build \
    --exclude .git \
    --exclude '*.lgp' \
    --exclude '*.lgp.*' \
    --exclude 'liquidgov_blobs/' \
    --exclude '.env.local' \
    "$SRC" "$DEST"

echo "Sync done. Still need on the remote: cmake --build <path>/build -j1, chown, systemctl restart liquidgov."
