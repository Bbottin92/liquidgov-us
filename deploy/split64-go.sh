#!/bin/bash
# Put Store split into modules (same bytes on disk), faster and prebuilt builds, WebSocket caps on liquidgov.us. Run from WSL.
# Refuses unless the only argument is: go
# Set LIQUIDGOV_DEPLOY_KEY to your own private key (a Windows path).
#   LIQUIDGOV_DEPLOY_KEY=C:/Users/Brandon/.ssh/id_ed25519_grok bash deploy/split64-go.sh go
#
# Read-only preflight, then a dry run. A content file outside the allow-list
# stops the script before the backup. Then: copy the running binary to
# liquidgov_http.pre-split64, sync, chown, build -j1, restart, read the journal,
# public checks. No sleep after that. pre-city, pre-flow, pre-match and pre-iphone are left in place.
set -euo pipefail
if [ "${1:-}" != "go" ] && [ "${1:-}" != "dry" ]; then
  echo "Refusing. This restarts liquidgov.us. Run: bash deploy/split64-go.sh go"
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
if [ "${1:-}" = "dry" ]; then echo "DRY RUN (nothing on the server changes) from $ROOT"; else echo "RESTARTING liquidgov.us from $ROOT"; fi

remote() {
  "$SSH" -i "$WINKEY" -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new "$HOST" bash -s -- "$1" < "$ROOT/deploy/split64-remote.sh"
}

export LIQUIDGOV_DEPLOY_KEY="$WINKEY"
export LIQUIDGOV_DEPLOY_SSH="$SSH -i $WINKEY -o IdentitiesOnly=yes"

PRELOG="${TMPDIR:-/tmp}/split64-preflight.txt"
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
if ! grep -q '^PRE_SPLIT64_ABSENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_FLOW_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_CITY_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PREFLIGHT_DONE$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi

DRYLOG="${TMPDIR:-/tmp}/split64-dry-run.txt"
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
    "CMakeLists.txt",
    "assets/canvas.js",
    "libs/liquidgov_file_store/CMakeLists.txt",
    "libs/liquidgov_file_store/sources.cmake",
    "libs/liquidgov_file_store/include/liquidgov/platform/file_store.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/accounts.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/admin.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/city.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/communities.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/jukebox.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/mail.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/notices.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/pains.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/posts.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/prefs.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/profile.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/proposals.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/social.hpp",
    "libs/liquidgov_file_store/include/liquidgov/platform/records/styles.hpp",
    "libs/liquidgov_file_store/src/file_store.cpp",
    "libs/liquidgov_file_store/src/core/codec.cpp",
    "libs/liquidgov_file_store/src/core/codec.hpp",
    "libs/liquidgov_file_store/src/core/load_pending.hpp",
    "libs/liquidgov_file_store/src/records/accounts.cpp",
    "libs/liquidgov_file_store/src/records/admin.cpp",
    "libs/liquidgov_file_store/src/records/avatars.cpp",
    "libs/liquidgov_file_store/src/records/city.cpp",
    "libs/liquidgov_file_store/src/records/communities.cpp",
    "libs/liquidgov_file_store/src/records/conversations.cpp",
    "libs/liquidgov_file_store/src/records/counters.cpp",
    "libs/liquidgov_file_store/src/records/here.cpp",
    "libs/liquidgov_file_store/src/records/jukebox.cpp",
    "libs/liquidgov_file_store/src/records/mail.cpp",
    "libs/liquidgov_file_store/src/records/notices.cpp",
    "libs/liquidgov_file_store/src/records/pains.cpp",
    "libs/liquidgov_file_store/src/records/posts.cpp",
    "libs/liquidgov_file_store/src/records/prefs.cpp",
    "libs/liquidgov_file_store/src/records/profile.cpp",
    "libs/liquidgov_file_store/src/records/proposal_comments.cpp",
    "libs/liquidgov_file_store/src/records/proposals.cpp",
    "libs/liquidgov_file_store/src/records/social.cpp",
    "libs/liquidgov_file_store/src/records/styles.cpp",
    "libs/liquidgov_password/CMakeLists.txt",
    "src/http_server.cpp",
    "src/site/MODULES.md",
    "src/site/common.hpp",
    "src/site/sources.cmake",
    "src/site/core/adapters_store.hpp",
    "src/site/core/html.hpp",
    "src/site/core/http.cpp",
    "src/site/core/http.hpp",
    "src/site/core/names_categories.hpp",
    "src/site/core/state.cpp",
    "src/site/core/state.hpp",
    "src/site/core/ws_limits.cpp",
    "src/site/core/ws_limits.hpp",
    "src/site/features/admin/admin.hpp",
    "src/site/features/assets/assets_routes.cpp",
    "src/site/features/canvas/canvas.cpp",
    "src/site/features/feed/post_block.hpp",
    "src/site/features/here/here.cpp",
    "src/site/features/here/here.hpp",
    "src/site/features/jukebox/jukebox_script.cpp",
    "src/site/features/live/live_sessions.cpp",
    "src/site/features/live/live_sessions.hpp",
    "src/site/features/map/map_graph.hpp",
    "src/site/features/map/map_rules.hpp",
    "src/site/features/pains/pains.hpp",
    "src/site/features/profile/profile_routes.cpp",
    "src/site/features/proposals/proposals.hpp",
    "src/site/features/styles/styles.hpp",
    "tests/program_messaging_test.cpp",
    "tests/store_roundtrip_styles_here_test.cpp",
    "tests/store_roundtrip_test.cpp",
    "tests/e2e/canvas_asset.sh",
    "tests/e2e/trim_dead_routes.sh",
    "tests/e2e/version_flag.sh",
    "tests/e2e/ws_caps.sh",
    "tests/fixtures/lgp1_roundtrip/cases.expected.txt",
    "tests/fixtures/lgp1_roundtrip/store_all_tags.expected.lgp1",
    "tests/fixtures/lgp1_roundtrip/store_all_tags.lgp1",
    "tests/fixtures/lgp1_roundtrip_styles_here/cases.expected.txt",
    "tests/fixtures/lgp1_roundtrip_styles_here/store_all_tags.expected.lgp1",
    "tests/fixtures/lgp1_roundtrip_styles_here/store_all_tags.lgp1",
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
SYNCLOG="${TMPDIR:-/tmp}/split64-sync.txt"
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
journal="${TMPDIR:-/tmp}/split64-restart-journal.txt"
remote restart | tee "$journal"
if grep -E "unknown record type|store open failed" "$journal"; then
  echo ROLLBACK_NEEDED
  exit 1
fi
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
  echo "RECORD ctx log: python C:/Users/Brandon/shared-context/tools/ctx.py log <who> deploy \"liquidgov.us Store split into modules (same bytes on disk), faster and prebuilt builds, WebSocket caps (split64)\" --result ok --ref $REC_COMMIT"
  echo "RECORD notes.md: - **$(TZ=America/Los_Angeles date '+%Y-%m-%d %H:%M') PT -- Store split into modules (same bytes on disk), faster and prebuilt builds, WebSocket caps (split64) LIVE (commit \`$REC_COMMIT\`, PID $REC_PID, public checks FAILS 0).**"
fi
