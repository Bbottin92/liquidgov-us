#!/bin/bash
# City deploy for liquidgov.us. Run from WSL.
# Refuses unless the only argument is: go
# Set LIQUIDGOV_DEPLOY_KEY to your own private key (a Windows path).
# The server log then names you. Example for Grok:
#   LIQUIDGOV_DEPLOY_KEY=C:/Users/Brandon/.ssh/id_ed25519_grok bash deploy/city-go.sh go
#
# Order: backup, keep the old binary, copy the undo tool, sync, chown, build -j1,
# restart, read the journal, then the public checks. There is no sleep.
# Tell Brandon as soon as those lines print. A later journal is deploy/city-status.sh.
set -euo pipefail
if [ "${1:-}" != "go" ]; then
  echo "Refusing. This restarts liquidgov.us. Run: bash deploy/city-go.sh go"
  exit 2
fi
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
SCP=/mnt/c/Windows/System32/OpenSSH/scp.exe
HOST=root@143.198.159.58
WINKEY=$(winpath "$KEY")
WINUNDO=$(winpath "$ROOT/deploy/city-undo.py")
[ -x "$SSH" ] && [ -x "$SCP" ]
echo "RESTARTING liquidgov.us from $ROOT"

remote() {
  "$SSH" -i "$WINKEY" -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new "$HOST" bash -s -- "$1" < "$ROOT/deploy/city-remote.sh"
}

remote backup
"$SCP" -i "$WINKEY" -o IdentitiesOnly=yes "$WINUNDO" "$HOST:/opt/liquidgov-tools/city-undo.py"
"$SSH" -i "$WINKEY" -o IdentitiesOnly=yes "$HOST" "chmod 755 /opt/liquidgov-tools/city-undo.py && python3 -m py_compile /opt/liquidgov-tools/city-undo.py"
echo UNDO_TOOL_OK

cd "$ROOT"
export LIQUIDGOV_DEPLOY_SSH="$SSH -i $WINKEY -o IdentitiesOnly=yes"
unset LIQUIDGOV_DEPLOY_DRY_RUN
bash deploy/sync-to-droplet.sh

remote chown
remote build
journal="${TMPDIR:-/tmp}/city-restart-journal.txt"
remote restart | tee "$journal"
if grep -E "unknown record type|store open failed" "$journal"; then
  echo ROLLBACK_NEEDED
  exit 1
fi
echo GO_DONE
python3 "$ROOT/deploy/city-live-check.py"
echo CHECK_DONE
