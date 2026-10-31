#!/usr/bin/env bash
# The daemon deploy, in the order that has worked every time:
#
#   1 drift-check   the droplet must still hold what was last deployed (and the tree must be clean and pushed)
#   2 backup-db     an ONLINE copy of commons.sqlite (backup API), integrity-checked, row counts printed
#   3 sync          commons-daemon/ -> /opt/commons-daemon  (writes .deployed-rev)
#   4 build         cmake --build -j1; a reconfigure ONLY if CMakeLists.txt changed since the live revision
#   5 restart       systemctl restart. NOTE: this drops every in-RAM guest credential and all presence; guests
#                   re-mint through the existing 403 path; houses are durable (SQLite)
#   6 verify        verify.sh, with --compare-backup so lost rows fail the deploy
#   7 client        only with --with-client, and LAST: commons/ -> /opt/commons (the daemon it talks to is live)
#
# WITHOUT --go THIS ONLY PLANS: it runs the read-only drift check and prints what would happen. It is the
# script Brandon's literal `deploy` word starts; nothing in the repo runs it automatically, CI never does.
#
# Civic is NOT deployed here. If civic changed since the live revision this refuses --go until you have deployed
# civic first (deploy/sync-to-droplet.sh at the repo root) and say so with --civic-done: civic goes first when
# both change, because the daemon and the client depend on civic's routes.
#
# Usage: deploy-daemon.sh [--go] [--baseline REV] [--with-client] [--civic-done] [--guest-name NAME]
#                         [--label LABEL] [--allow-dirty] [--allow-unpushed]
#   --baseline REV  the revision live now; only needed until the droplet has a marker (first use: 60f6745)
#   --label LABEL   names the backup (default: the short HEAD sha)
# shellcheck disable=SC2034  # ALLOW_DIRTY / ALLOW_UNPUSHED are set here and read by need_clean_tree / need_pushed in lib.sh
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

GO=0; BASELINE=""; WITH_CLIENT=0; CIVIC_DONE=0; GUEST="deploy-check"; LABEL=""
pass=()   # flags forwarded to drift-check / sync
while [ $# -gt 0 ]; do
  case "$1" in
    --go) GO=1; shift;;
    --baseline) BASELINE="${2:?--baseline needs a revision}"; shift 2;;
    --with-client) WITH_CLIENT=1; shift;;
    --civic-done) CIVIC_DONE=1; shift;;
    --guest-name) GUEST="${2:?--guest-name needs a value}"; shift 2;;
    --label) LABEL="${2:?--label needs a value}"; shift 2;;
    --allow-dirty) ALLOW_DIRTY=1; pass+=(--allow-dirty); shift;;
    --allow-unpushed) ALLOW_UNPUSHED=1; pass+=(--allow-unpushed); shift;;
    -h|--help) sed -n '2,29p' "$0"; exit 0;;
    *) die "unknown argument: $1";;
  esac
done
[ -n "$LABEL" ] || LABEL="$(git -C "$REPO" rev-parse --short HEAD)"

trees=daemon; [ "$WITH_CLIENT" = 1 ] && trees=both
drift_args=(--tree "$trees" "${pass[@]}"); [ -z "$BASELINE" ] || drift_args+=(--baseline "$BASELINE")

say "##### 1/7 drift check"
"$DEPLOY_DIR/drift-check.sh" "${drift_args[@]}" || { rc=$?; say "stopping: the drift check did not pass (exit $rc)"; exit "$rc"; }

# What is live now, to decide about civic and about a reconfigure.
live="${BASELINE:-$(deployed_rev daemon)}"
live="${live%+dirty}"
needs_reconfigure=0; civic_changed=0
if git -C "$REPO" diff --quiet "$live" HEAD -- commons-daemon/CMakeLists.txt; then :; else needs_reconfigure=1; fi
if git -C "$REPO" diff --quiet "$live" HEAD -- liquidgov-platform; then :; else civic_changed=1; fi

say ""
say "PLAN (target $HOST, live revision ${live:0:7}, deploying $(git -C "$REPO" rev-parse --short HEAD)):"
say "  2 backup  -> $BACKUP_DIR_REMOTE/commons.sqlite.pre-$LABEL-<utc>   (online, integrity-checked)"
say "  3 sync    -> $DAEMON_DIR   (never build/tests/sqlite)"
say "  4 build   -> cmake --build -j1, $([ "$needs_reconfigure" = 1 ] && echo 'WITH a reconfigure: CMakeLists.txt changed' || echo 'no reconfigure')"
say "  5 restart -> $SERVICE  (drops in-RAM presence and guest credentials; houses are durable)"
say "  6 verify  -> verify.sh --compare-backup <the backup> --guest-name $GUEST"
[ "$WITH_CLIENT" = 1 ] && say "  7 client  -> $CLIENT_DIR  (last)" || say "  7 client  -> skipped (no --with-client)"
if [ "$civic_changed" = 1 ]; then
  say ""; say "CIVIC CHANGED since ${live:0:7} (liquidgov-platform/). Deploy civic FIRST (deploy/sync-to-droplet.sh at the repo root, then its"
  say "build + restart), then run this with --civic-done. This script never touches civic."
fi

if [ "$GO" != 1 ]; then say ""; say "Plan only (no --go): nothing was changed."; exit 0; fi
if [ "$civic_changed" = 1 ] && [ "$CIVIC_DONE" != 1 ]; then die "civic changed and --civic-done was not given"; fi

say ""; say "##### 2/7 backup"
out="$("$DEPLOY_DIR/backup-db.sh" "$LABEL")" || { say "$out"; die "backup failed: nothing was synced"; }
say "$out"
backup="$(sed -n 's/^BACKUP //p' <<<"$out" | tail -1)"
[ -n "$backup" ] || die "backup printed no path: nothing was synced"

say ""; say "##### 3/7 sync"
"$DEPLOY_DIR/sync-to-droplet.sh" "${pass[@]}"

say ""; say "##### 4/7 build"
if [ "$needs_reconfigure" = 1 ]; then "$DEPLOY_DIR/build-on-droplet.sh" --reconfigure; else "$DEPLOY_DIR/build-on-droplet.sh"; fi

say ""; say "##### 5/7 restart"
remote "systemctl restart $(q "$SERVICE")"
sleep 2

say ""; say "##### 6/7 verify"
if ! "$DEPLOY_DIR/verify.sh" --compare-backup "$backup" --guest-name "$GUEST"; then
  say ""; say "VERIFY FAILED. The new code is live. Roll back by deploying the previous revision (--baseline <new> after checking out the"
  say "old one) and, only if data is wrong, restoring $backup while the service is stopped."
  exit 1
fi

if [ "$WITH_CLIENT" = 1 ]; then
  say ""; say "##### 7/7 client (last)"
  "$DEPLOY_DIR/sync-client-to-droplet.sh" "${pass[@]}"
  code="$(curl -s -m 15 -o /dev/null -w '%{http_code}' "$PUBLIC_BASE$PREFIX/" || true)"
  [ "${DEPLOY_SKIP_SITE:-0}" = 1 ] || [ "$code" = 200 ] || die "the game page $PUBLIC_BASE$PREFIX/ answered $code after the client sync"
fi

say ""; say "== deployed $(git -C "$REPO" rev-parse --short HEAD). Backup kept at $backup"
