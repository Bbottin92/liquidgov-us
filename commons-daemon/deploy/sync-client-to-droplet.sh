#!/usr/bin/env bash
# Syncs the game CLIENT (repo commons/) to the production droplet's
# /opt/commons -- the nginx static alias target (nginx.md,
# nginx-commons.conf). Separate from sync-to-droplet.sh, which syncs the
# DAEMON's own source tree to /opt/commons-daemon. These are two
# different directories on purpose (GROK_DEPLOY_REVIEW.md must-fix #1):
# `commons/index.html` is a sibling of `commons-daemon/` in this repo,
# not inside it -- deploy-plan.md's earlier
# `cp /opt/commons-daemon/commons/index.html` step named a path that
# does not exist. The alias directory must contain only files nginx is
# meant to serve directly; never rsync the daemon's source/build tree
# into it.
#
# Usage: commons-daemon/deploy/sync-client-to-droplet.sh [user@]host[:path]
#   defaults to root@143.198.159.58:/opt/commons/

set -euo pipefail

DEST="${1:-root@143.198.159.58:/opt/commons/}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../commons" && pwd)/"
KEY="${LIQUIDGOV_DEPLOY_KEY:-$HOME/.ssh/liquidgov_deploy_key}"

echo "Syncing $SRC -> $DEST (client files only -- no daemon source, ever)"
rsync -az -e "ssh -i $KEY" \
    "$SRC" "$DEST"

echo "Sync done. Still need on the remote: chown liquidgov:liquidgov -R /opt/commons"
