#!/usr/bin/env bash
# Prunes old database backups in /var/lib/commons/backups (deploy backups `pre-<label>-<stamp>` AND the daemon's
# own `pre-reap-<stamp>` are one naming scheme and one policy; see prune-backups.py for the exact rules).
#
#   A backup is deleted ONLY if it is BOTH outside the newest 14 AND older than 30 days.
#   The newest 14 stay however old; anything within 30 days stays however many. The newest backup and any backup
#   younger than 24h are never deleted. Files that are not backups are never touched.
#
# DRY RUN BY DEFAULT: it prints every file's verdict and what it WOULD delete. Only --delete deletes.
# It refuses (deletes nothing) if the listing fails or the clock/files look wrong (a backup dated in the future).
# Not scheduled anywhere and not run by deploy-daemon.sh: retention is a separate, deliberate step.
#
# Usage: prune-backups.sh [--delete] [--keep N] [--days D]
#   --keep/--days override the locked policy (14 / 30); a more aggressive value prints a warning.
# Environment: DEPLOY_NOW (epoch seconds) replaces the clock, for the tests.
# Exit: 0 ok (dry run or deleted) / 1 error / 3 anomaly (nothing deleted)
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

DO_DELETE=0; KEEP=14; DAYS=30
while [ $# -gt 0 ]; do
  case "$1" in
    --delete) DO_DELETE=1; shift;;
    --keep) KEEP="${2:?--keep needs a number}"; shift 2;;
    --days) DAYS="${2:?--days needs a number}"; shift 2;;
    -h|--help) sed -n '2,17p' "$0"; exit 0;;
    *) die "unknown argument: $1";;
  esac
done
[[ "$KEEP" =~ ^[0-9]+$ ]] || die "--keep must be a whole number"
[[ "$DAYS" =~ ^[0-9]+$ ]] || die "--days must be a whole number"
NOW="${DEPLOY_NOW:-$(date +%s)}"
NAME_RE='^commons\.sqlite\.pre-[A-Za-z0-9._-]+-[0-9]{8}T[0-9]{6}Z$'

say "== backup retention on $HOST:$BACKUP_DIR_REMOTE"
say "   policy: delete only if outside the newest $KEEP AND older than $DAYS days; never the newest; never younger than 24h"
{ [ "$KEEP" -lt 14 ] || [ "$DAYS" -lt 30 ]; } && say "   WARNING: more aggressive than the locked policy (14 / 30 days)"
[ "$DO_DELETE" = 1 ] && say "   mode: --delete (files WILL be removed)" || say "   mode: dry run (nothing is deleted)"

# "<mtime> <size> <name>" per regular file; a failing listing (missing directory, ssh down) is an error, not "empty"
listing="$(remote "find $(q "$BACKUP_DIR_REMOTE") -maxdepth 1 -type f -printf '%T@ %s %f\n'")" \
  || die "could not list $BACKUP_DIR_REMOTE on $HOST: nothing was deleted"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
printf '%s\n' "$listing" | sed -E 's/^([0-9.]+) [0-9]+ /\1 /' > "$TMP/in"                   # mtime name
printf '%s\n' "$listing" | sed -nE 's/^[0-9.]+ ([0-9]+) (.*)$/\2\t\1/p' > "$TMP/sizes"      # name size

rc=0
python3 "$DEPLOY_DIR/prune-backups.py" --keep "$KEEP" --days "$DAYS" --now "$NOW" < "$TMP/in" > "$TMP/out" || rc=$?
[ "$rc" = 0 ] || [ "$rc" = 3 ] || die "the decision step failed (exit $rc): nothing was deleted"

awk -F'\t' '{printf "  %-7s %-58s %s\n", $1, $3, $2}' "$TMP/out"
n_keep="$(grep -c '^KEEP' "$TMP/out" || true)"; n_del="$(grep -c '^DELETE' "$TMP/out" || true)"; n_ign="$(grep -c '^IGNORE' "$TMP/out" || true)"
freed=0
while IFS=$'\t' read -r verb _ name; do
  [ "$verb" = DELETE ] || continue
  sz="$(awk -F'\t' -v n="$name" '$1==n{print $2}' "$TMP/sizes")"; freed=$((freed + ${sz:-0}))
done < "$TMP/out"
say ""
say "== $((n_keep + n_del)) backups: keep $n_keep, delete $n_del (frees $freed bytes), ignore $n_ign other file(s)"

if [ "$rc" = 3 ]; then say "ANOMALY: $(sed -n 's/^ANOMALY\t\([^\t]*\)\t.*/\1/p' "$TMP/out")"; exit 3; fi
if [ "$n_del" = 0 ]; then say "nothing to delete."; exit 0; fi
if [ "$DO_DELETE" != 1 ]; then say "DRY RUN: nothing was deleted. Re-run with --delete to delete the $n_del file(s) marked DELETE."; exit 0; fi

deleted=0
while IFS=$'\t' read -r verb _ name; do
  [ "$verb" = DELETE ] || continue
  [[ "$name" =~ $NAME_RE ]] || die "refusing to delete '$name': not a backup name (deleted $deleted so far)"
  remote "rm -- $(q "$BACKUP_DIR_REMOTE/$name")"
  deleted=$((deleted + 1))
done < "$TMP/out"

# afterwards: everything marked KEEP must still be there
after="$(remote "find $(q "$BACKUP_DIR_REMOTE") -maxdepth 1 -type f -printf '%f\n'")" || die "could not re-list after deleting"
missing=0
while IFS=$'\t' read -r verb _ name; do
  [ "$verb" = KEEP ] || continue
  grep -qxF "$name" <<<"$after" || { say "MISSING after the prune: $name"; missing=1; }
done < "$TMP/out"
[ "$missing" = 0 ] || die "a backup marked KEEP is missing after the prune"
say "deleted $deleted file(s); every backup marked KEEP is still present."
