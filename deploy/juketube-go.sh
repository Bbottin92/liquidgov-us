#!/bin/bash
# Put the Juketube fix (skip vote, embed check, keep playing on close/open) on liquidgov.us. Run from WSL.
# Refuses unless the only argument is: go
# Set LIQUIDGOV_DEPLOY_KEY to your own private key (a Windows path).
#   LIQUIDGOV_DEPLOY_KEY=C:/Users/Brandon/.ssh/id_ed25519_grok bash deploy/juketube-go.sh go
#
# Read-only preflight, then a dry run. A content file outside the allow-list
# stops the script before the backup. Then: copy the running binary to
# liquidgov_http.pre-juketube, sync, chown, build -j1, restart, read the journal,
# public checks. No sleep after that. pre-city, pre-flow, pre-match and pre-iphone are left in place.
set -euo pipefail
if [ "${1:-}" != "go" ]; then
  echo "Refusing. This restarts liquidgov.us. Run: bash deploy/juketube-go.sh go"
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
HOST=root@143.198.159.58
WINKEY=$(winpath "$KEY")
[ -x "$SSH" ]
echo "RESTARTING liquidgov.us from $ROOT"

remote() {
  "$SSH" -i "$WINKEY" -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new "$HOST" bash -s -- "$1" < "$ROOT/deploy/juketube-remote.sh"
}

export LIQUIDGOV_DEPLOY_KEY="$WINKEY"
export LIQUIDGOV_DEPLOY_SSH="$SSH -i $WINKEY -o IdentitiesOnly=yes"

PRELOG="${TMPDIR:-/tmp}/juketube-preflight.txt"
remote preflight | tee "$PRELOG"
if ! grep -q '^ACTIVE=active$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_JUKETUBE_ABSENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_FLOW_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_CITY_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PREFLIGHT_DONE$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi

DRYLOG="${TMPDIR:-/tmp}/juketube-dry-run.txt"
# WSL vsock accept4/110 has closed rsync before any backup. One retry.
# Any other rsync failure stops here.
try=0
while true; do
  try=$((try + 1))
  if LIQUIDGOV_DEPLOY_DRY_RUN=1 bash "$ROOT/deploy/sync-to-droplet.sh" | tee "$DRYLOG"; then
    break
  fi
  if [ "$try" -ge 2 ] || ! grep -q "accept4 failed" "$DRYLOG"; then
    exit 1
  fi
  echo WSL_RSYNC_RETRY
done

python3 - "$DRYLOG" << 'PY'
import sys
allow = {
    "assets/juketube.css",
    "assets/juketube.js",
    "src/http_server.cpp",
    "tests/e2e/juketube_skip.sh",
    "tests/e2e/juketube_skip_client.py",
}
bad = []
content = []
for raw in open(sys.argv[1], encoding="utf-8", errors="replace"):
    line = raw.rstrip("\r\n")
    if len(line) < 12 or line[0] not in ".<>c*h":
        continue
    if line[0] == "*":
        if "deleting" in line:
            bad.append(line)
        continue
    if line[0] == ".":
        continue
    if line[1] == "d":
        continue
    path = line[11:].strip()
    content.append(path)
    if path not in allow:
        bad.append(line)
missing = sorted(allow - set(content))
print("CONTENT", len(content))
for path in content:
    print("SEND", path)
if missing:
    print("MISSING", " ".join(missing))
    bad.append("missing expected file")
if bad:
    print("DRY_REFUSED")
    for item in bad:
        print(item)
    sys.exit(1)
print("DRY_OK")
PY

remote backup
cd "$ROOT"
unset LIQUIDGOV_DEPLOY_DRY_RUN
SYNCLOG="${TMPDIR:-/tmp}/juketube-sync.txt"
try=0
while true; do
  try=$((try + 1))
  if bash deploy/sync-to-droplet.sh | tee "$SYNCLOG"; then
    break
  fi
  if [ "$try" -ge 2 ] || ! grep -q "accept4 failed" "$SYNCLOG"; then
    exit 1
  fi
  echo WSL_RSYNC_RETRY
done
remote chown
remote build
journal="${TMPDIR:-/tmp}/juketube-restart-journal.txt"
remote restart | tee "$journal"
if grep -E "unknown record type|store open failed" "$journal"; then
  echo ROLLBACK_NEEDED
  exit 1
fi
echo GO_DONE
python3 "$ROOT/deploy/city-live-check.py"
echo CHECK_DONE
