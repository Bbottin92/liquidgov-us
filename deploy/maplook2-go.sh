#!/bin/bash
# Put Map: Grok Bot's mockup look, kinds re-center, branches collapse on liquidgov.us. Run from WSL.
# Refuses unless the only argument is: go
# Set LIQUIDGOV_DEPLOY_KEY to your own private key (a Windows path).
#   LIQUIDGOV_DEPLOY_KEY=C:/Users/Brandon/.ssh/id_ed25519_grok bash deploy/maplook2-go.sh go
#
# Read-only preflight, then a dry run. A content file outside the allow-list
# stops the script before the backup. Then: copy the running binary to
# liquidgov_http.pre-maplook2, sync, chown, public checks. No sleep after that.
# pre-city, pre-flow, pre-match and pre-iphone are left in place.
# NO BUILD, NO RESTART (Brandon 2026-11-08 "Go, no restart"): this change is assets only, and the running server
# (WorkingDirectory /opt/liquidgov) reads assets/juketube.js and .css from disk on every request, re-hashing their
# URLs when the mtime changes. A restart would drop every listener and the shared station's song and queue.
# The checks after the sync prove it: same server PID as before, and the live juketube.js equals this commit's.
# Rollback: re-sync the two files from the previous commit (no restart needed either).
set -euo pipefail
if [ "${1:-}" != "go" ] && [ "${1:-}" != "dry" ]; then
  echo "Refusing. This changes liquidgov.us (assets only, no restart). Run: bash deploy/maplook2-go.sh go"
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
if [ "${1:-}" = "dry" ]; then echo "DRY RUN (nothing on the server changes) from $ROOT"; else echo "UPDATING liquidgov.us assets (no restart) from $ROOT"; fi

remote() {
  "$SSH" -i "$WINKEY" -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new "$HOST" bash -s -- "$1" < "$ROOT/deploy/maplook2-remote.sh"
}

export LIQUIDGOV_DEPLOY_KEY="$WINKEY"
export LIQUIDGOV_DEPLOY_SSH="$SSH -i $WINKEY -o IdentitiesOnly=yes"

PRELOG="${TMPDIR:-/tmp}/maplook2-preflight.txt"
# WSL vsock accept4/110 can drop the read-only preflight ssh too (polish1, 2026-11-10). One retry; any other
# failure, or a second drop, stops here (tests/repo/preflight_retry_check.py).
try=0
while true; do
  try=$((try + 1))
  if remote preflight 2>&1 | tee "$PRELOG"; then
    break
  fi
  if [ "$try" -ge 2 ] || ! grep -q "accept4 failed" "$PRELOG"; then
    echo PREFLIGHT_FAILED
    exit 1
  fi
  echo WSL_PREFLIGHT_RETRY
done
if ! grep -q '^ACTIVE=active$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_MAPLOOK2_ABSENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_FLOW_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_CITY_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PREFLIGHT_DONE$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
PID_BEFORE=$(sed -n 's/^PID=\([0-9][0-9]*\)$/\1/p' "$PRELOG")
if [ -z "$PID_BEFORE" ] || [ "$PID_BEFORE" = "0" ]; then echo "PREFLIGHT_REFUSED (no server PID)"; exit 1; fi
echo "PID_BEFORE=$PID_BEFORE"

DRYLOG="${TMPDIR:-/tmp}/maplook2-dry-run.txt"
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
    "assets/lgmap.js",
    "assets/lgmap.css",
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

# dry: stop here, before the backup (added by make-go.py)
# CI gate (added by make-go.py): deploy/ci-gate.sh
bash "$ROOT/deploy/ci-gate.sh" "${1:-}" || exit 1
if [ "${1:-}" = "dry" ]; then echo "DRY_ONLY_DONE: nothing on the server was changed. Run with go to deploy."; exit 0; fi

remote backup
cd "$ROOT"
unset LIQUIDGOV_DEPLOY_DRY_RUN
SYNCLOG="${TMPDIR:-/tmp}/maplook2-sync.txt"
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
# no build, no restart (see the header). Prove the running server is the same process and serves the new files.
POSTLOG="${TMPDIR:-/tmp}/maplook2-postcheck.txt"
remote preflight | tee "$POSTLOG"
PID_AFTER=$(sed -n 's/^PID=\([0-9][0-9]*\)$/\1/p' "$POSTLOG")
if ! grep -q '^ACTIVE=active$' "$POSTLOG"; then echo "NOT_ACTIVE after sync"; exit 1; fi
if [ "$PID_AFTER" != "$PID_BEFORE" ]; then echo "PID_CHANGED $PID_BEFORE -> $PID_AFTER (something restarted the server)"; exit 1; fi
echo "PID_SAME=$PID_AFTER"
for f in juketube.js juketube.css; do
  live=$(curl -fsS --max-time 20 "https://liquidgov.us/assets/$f" | sha256sum | cut -d' ' -f1)
  want=$(sha256sum "$ROOT/liquidgov-platform/assets/$f" | cut -d' ' -f1)
  if [ -z "$live" ] || [ "$live" != "$want" ]; then echo "LIVE_MISMATCH $f"; exit 1; fi
  echo "LIVE_MATCH $f"
done
if ! curl -fsS --max-time 20 "https://liquidgov.us/assets/juketube.js" | grep -q lgShellHome; then echo "LIVE_NO_SHELL"; exit 1; fi
echo "LIVE_HAS_SHELL"
echo GO_DONE
python3 "$ROOT/deploy/city-live-check.py"
echo CHECK_DONE
# records (added by make-go.py): the ctx log line and notes.md bullet from THIS run's real output
# (Brandon 2026-11-08); reached only after the public checks passed (set -e; city-live-check exits 1)
REC_COMMIT=$(sed -n 's/^Deploying commit \([0-9a-f][0-9a-f]*\).*/\1/p' "$SYNCLOG" | tail -1) || true
REC_PID=$(remote preflight 2>/dev/null | sed -n 's/^PID=\([0-9][0-9]*\)$/\1/p' | tail -1) || true
if [ -z "$REC_COMMIT" ] || [ -z "$REC_PID" ]; then
  echo "RECORD_FAILED: commit '$REC_COMMIT' / PID '$REC_PID' not found; write the records by hand from the log above"
else
  echo "RECORD ctx log: python C:/Users/Brandon/shared-context/tools/ctx.py log <who> deploy \"liquidgov.us Map: Grok Bot's mockup look, kinds re-center, branches collapse (maplook2)\" --result ok --ref $REC_COMMIT"
  echo "RECORD notes.md: - **$(TZ=America/Los_Angeles date '+%Y-%m-%d %H:%M') PT -- Map: Grok Bot's mockup look, kinds re-center, branches collapse (maplook2) LIVE (commit \`$REC_COMMIT\`, PID $REC_PID, public checks FAILS 0).**"
fi
