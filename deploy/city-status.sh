#!/bin/bash
# Read-only look at the live city deploy. No sleep. Does not change the server.
# Set LIQUIDGOV_DEPLOY_KEY to your own private key.
set -euo pipefail
KEY="${LIQUIDGOV_DEPLOY_KEY:-}"
if [ -z "$KEY" ]; then
  echo "Set LIQUIDGOV_DEPLOY_KEY to your own ssh private key."
  exit 2
fi
winpath() {
  local p="$1"
  case "$p" in
    /mnt/?/*)
      local drive="${p:5:1}"
      echo "${drive^^}:${p:6}"
      ;;
    *)
      echo "$p"
      ;;
  esac
}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SSH=/mnt/c/Windows/System32/OpenSSH/ssh.exe
WINKEY=$(winpath "$KEY")
"$SSH" -i "$WINKEY" -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new root@143.198.159.58 bash -s -- status < "$ROOT/deploy/city-remote.sh"
