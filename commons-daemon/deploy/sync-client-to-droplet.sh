#!/usr/bin/env bash
# Syncs the game CLIENT (repo commons/) to the production droplet's /opt/commons -- the nginx static alias
# target (nginx.md, nginx-commons.conf). Separate from sync-to-droplet.sh (the DAEMON's source, /opt/commons-daemon)
# on purpose (GROK_DEPLOY_REVIEW.md must-fix #1): the alias directory must contain only files nginx is meant to
# serve directly, so the daemon's source/build tree never goes into it, and the deployed-revision marker for this
# tree is kept in /opt/commons-daemon (.deployed-client-rev), not in the publicly served directory.
#
# Client goes LAST in a deploy: after the daemon it talks to is live. Same refusals as the daemon sync (dirty tree).
#
# Usage: sync-client-to-droplet.sh [-n|--dry-run] [--allow-dirty]
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

dry=""
for a in "$@"; do
  case "$a" in
    -n|--dry-run) dry="-n";;
    --allow-dirty) ALLOW_DIRTY=1;;
    -h|--help) sed -n '2,12p' "$0"; exit 0;;
    *) die "unknown argument: $a";;
  esac
done

sync_tree client ${dry:+"$dry"}
if [ -z "$dry" ]; then
  remote "chown -R $(q "$SERVICE_USER:$SERVICE_USER") $(q "$CLIENT_DIR")"
  say "Synced and chowned $CLIENT_DIR."
fi
