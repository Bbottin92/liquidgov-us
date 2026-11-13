#!/bin/bash
# Put site server split into modules (no behavior change) on liquidgov.us. Run from WSL.
# Refuses unless the only argument is: go
# Set LIQUIDGOV_DEPLOY_KEY to your own private key (a Windows path).
#   LIQUIDGOV_DEPLOY_KEY=C:/Users/Brandon/.ssh/id_ed25519_grok bash deploy/modular1-go.sh go
#
# Read-only preflight, then a dry run. A content file outside the allow-list
# stops the script before the backup. Then: copy the running binary to
# liquidgov_http.pre-modular1, sync, chown, build -j1, restart, read the journal,
# public checks. No sleep after that. pre-city, pre-flow, pre-match and pre-iphone are left in place.
set -euo pipefail
if [ "${1:-}" != "go" ] && [ "${1:-}" != "dry" ]; then
  echo "Refusing. This restarts liquidgov.us. Run: bash deploy/modular1-go.sh go"
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
  "$SSH" -i "$WINKEY" -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new "$HOST" bash -s -- "$1" < "$ROOT/deploy/modular1-remote.sh"
}

export LIQUIDGOV_DEPLOY_KEY="$WINKEY"
export LIQUIDGOV_DEPLOY_SSH="$SSH -i $WINKEY -o IdentitiesOnly=yes"

PRELOG="${TMPDIR:-/tmp}/modular1-preflight.txt"
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
if ! grep -q '^PRE_MODULAR1_ABSENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_FLOW_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PRE_CITY_PRESENT$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi
if ! grep -q '^PREFLIGHT_DONE$' "$PRELOG"; then echo PREFLIGHT_REFUSED; exit 1; fi

DRYLOG="${TMPDIR:-/tmp}/modular1-dry-run.txt"
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
    "src/http_server.cpp",
    "src/site/MODULES.md",
    "src/site/common.hpp",
    "src/site/router.cpp",
    "src/site/router.hpp",
    "src/site/sources.cmake",
    "src/site/core/adapters_store.cpp",
    "src/site/core/adapters_store.hpp",
    "src/site/core/html.cpp",
    "src/site/core/html.hpp",
    "src/site/core/http.cpp",
    "src/site/core/http.hpp",
    "src/site/core/multipart.cpp",
    "src/site/core/multipart.hpp",
    "src/site/core/names_categories.cpp",
    "src/site/core/names_categories.hpp",
    "src/site/core/net.cpp",
    "src/site/core/net.hpp",
    "src/site/core/route.hpp",
    "src/site/core/state.cpp",
    "src/site/core/state.hpp",
    "src/site/core/text.cpp",
    "src/site/core/text.hpp",
    "src/site/features/admin/admin.cpp",
    "src/site/features/admin/admin.hpp",
    "src/site/features/admin/admin_routes.cpp",
    "src/site/features/admin/admin_routes.hpp",
    "src/site/features/assets/assets_routes.cpp",
    "src/site/features/assets/assets_routes.hpp",
    "src/site/features/auth/auth_routes.cpp",
    "src/site/features/auth/auth_routes.hpp",
    "src/site/features/auth/signup.cpp",
    "src/site/features/auth/signup.hpp",
    "src/site/features/canvas/canvas.cpp",
    "src/site/features/canvas/canvas.hpp",
    "src/site/features/canvas/canvas_routes.cpp",
    "src/site/features/canvas/canvas_routes.hpp",
    "src/site/features/communities/communities_routes.cpp",
    "src/site/features/communities/communities_routes.hpp",
    "src/site/features/feed/composer.cpp",
    "src/site/features/feed/composer.hpp",
    "src/site/features/feed/feed_routes.cpp",
    "src/site/features/feed/feed_routes.hpp",
    "src/site/features/feed/post_block.cpp",
    "src/site/features/feed/post_block.hpp",
    "src/site/features/jukebox/jukebox_browse.cpp",
    "src/site/features/jukebox/jukebox_browse.hpp",
    "src/site/features/jukebox/jukebox_personal.cpp",
    "src/site/features/jukebox/jukebox_personal.hpp",
    "src/site/features/jukebox/jukebox_routes.cpp",
    "src/site/features/jukebox/jukebox_routes.hpp",
    "src/site/features/jukebox/jukebox_script.cpp",
    "src/site/features/jukebox/jukebox_script.hpp",
    "src/site/features/jukebox/jukebox_widget.cpp",
    "src/site/features/jukebox/jukebox_widget.hpp",
    "src/site/features/live/live_routes.cpp",
    "src/site/features/live/live_routes.hpp",
    "src/site/features/live/live_sessions.cpp",
    "src/site/features/live/live_sessions.hpp",
    "src/site/features/mail/mail_attachments.cpp",
    "src/site/features/mail/mail_attachments.hpp",
    "src/site/features/mail/mail_pages.cpp",
    "src/site/features/mail/mail_pages.hpp",
    "src/site/features/mail/mail_routes.cpp",
    "src/site/features/mail/mail_routes.hpp",
    "src/site/features/map/map_graph.cpp",
    "src/site/features/map/map_graph.hpp",
    "src/site/features/map/map_page.cpp",
    "src/site/features/map/map_page.hpp",
    "src/site/features/map/map_routes.cpp",
    "src/site/features/map/map_routes.hpp",
    "src/site/features/map/map_rules.cpp",
    "src/site/features/map/map_rules.hpp",
    "src/site/features/map/map_svg.cpp",
    "src/site/features/map/map_svg.hpp",
    "src/site/features/notices/notices.cpp",
    "src/site/features/notices/notices.hpp",
    "src/site/features/notices/notices_routes.cpp",
    "src/site/features/notices/notices_routes.hpp",
    "src/site/features/pages/genesis.cpp",
    "src/site/features/pages/genesis.hpp",
    "src/site/features/pages/home.cpp",
    "src/site/features/pages/home.hpp",
    "src/site/features/pages/pages_routes.cpp",
    "src/site/features/pages/pages_routes.hpp",
    "src/site/features/pages/static_pages.cpp",
    "src/site/features/pages/static_pages.hpp",
    "src/site/features/pains/pains.cpp",
    "src/site/features/pains/pains.hpp",
    "src/site/features/people/people.cpp",
    "src/site/features/people/people.hpp",
    "src/site/features/people/people_routes.cpp",
    "src/site/features/people/people_routes.hpp",
    "src/site/features/profile/portrait.cpp",
    "src/site/features/profile/portrait.hpp",
    "src/site/features/profile/profile_css.cpp",
    "src/site/features/profile/profile_css.hpp",
    "src/site/features/profile/profile_routes.cpp",
    "src/site/features/profile/profile_routes.hpp",
    "src/site/features/proposals/proposals.cpp",
    "src/site/features/proposals/proposals.hpp",
    "src/site/features/proposals/proposals_routes.cpp",
    "src/site/features/proposals/proposals_routes.hpp",
    "src/site/features/push/push_routes.cpp",
    "src/site/features/push/push_routes.hpp",
    "src/site/features/push/push_subs.cpp",
    "src/site/features/push/push_subs.hpp",
    "src/site/features/settings/settings_routes.cpp",
    "src/site/features/settings/settings_routes.hpp",
    "src/site/features/system/system_routes.cpp",
    "src/site/features/system/system_routes.hpp",
    "src/site/ui/assets.cpp",
    "src/site/ui/assets.hpp",
    "src/site/ui/favicon_cookies.cpp",
    "src/site/ui/favicon_cookies.hpp",
    "src/site/ui/shell.cpp",
    "src/site/ui/shell.hpp",
    "src/site/ui/site_css.cpp",
    "src/site/ui/site_css.hpp",
    "src/site/ui/wrap.cpp",
    "src/site/ui/wrap.hpp",
    "tests/browser/canvas_layout_check.py",
    "tests/browser/juketube_overlap_check.py",
    "tests/browser/map_live_check.py",
    "tests/browser/me_autosave_check.py",
    "tests/browser/me_autosave_sections_check.py",
    "tests/browser/me_autosave_slow_check.py",
    "tests/browser/messenger_layout_check.py",
    "tests/browser/server_source.py",
    "tests/browser/site_hidden_check.py",
    "tests/browser/site_spelling_check.py",
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
SYNCLOG="${TMPDIR:-/tmp}/modular1-sync.txt"
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
journal="${TMPDIR:-/tmp}/modular1-restart-journal.txt"
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
  echo "RECORD ctx log: python C:/Users/Brandon/shared-context/tools/ctx.py log <who> deploy \"liquidgov.us site server split into modules (no behavior change) (modular1)\" --result ok --ref $REC_COMMIT"
  echo "RECORD notes.md: - **$(TZ=America/Los_Angeles date '+%Y-%m-%d %H:%M') PT -- site server split into modules (no behavior change) (modular1) LIVE (commit \`$REC_COMMIT\`, PID $REC_PID, public checks FAILS 0).**"
fi
