#!/usr/bin/env bash
# Post-deploy checks: the list the hardening deploy was verified with, as a script. Read-mostly: the only writes
# are one throwaway guest (mint, join, move, leave -- a RAM occupant for about a second, the same guest the
# public site mints for any visitor). No flooding: one guest, a handful of requests.
#
#   on the droplet   the service is active, NRestarts=0, civic still active, the daemon answers on loopback,
#                    the running daemon's /health commit equals the deployed revision (.deployed-rev),
#                    the journal since it started has no error lines
#   through nginx    civic `/` 200, the game page 200, lots 200, and the public site does NOT serve /internal/*
#   a real guest     mint (id g_+16 hex, secret 32 hex) -> join -> visible in /presence/1,1 -> move 200 ->
#                    a bad secret is 403 -> leave -> gone
#   the database     with --compare-backup PATH: no table has FEWER rows than the pre-deploy backup
#                    (more is normal; fewer means data went missing)
#
# Usage: verify.sh [--guest-name NAME] [--compare-backup PATH]
#   --guest-name  join under this name (default "deploy-check"); use x to check the reqjson fix in the real Square
# Environment: DEPLOY_SKIP_SITE=1 skips the civic/static/public-/internal checks (the tests talk to the daemon alone);
#              DEPLOY_SKIP_REV_CHECK=1 skips the running-revision comparison (the tests' fake droplet has no matching build).
# Exit 0 = every check passed.
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

NAME="deploy-check"; BACKUP=""
while [ $# -gt 0 ]; do
  case "$1" in
    --guest-name) NAME="${2:?--guest-name needs a value}"; shift 2;;
    --compare-backup) BACKUP="${2:?--compare-backup needs a path}"; shift 2;;
    -h|--help) sed -n '2,22p' "$0"; exit 0;;
    *) die "unknown argument: $1";;
  esac
done
[[ "$NAME" =~ ^[A-Za-z0-9._-]{1,24}$ ]] || die "--guest-name: 1-24 of letters, digits, . _ -"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok()  { PASS=$((PASS+1)); say "  ok   $1"; }
bad() { FAIL=$((FAIL+1)); say "  FAIL $1"; [ -n "${2:-}" ] && say "       $2"; return 0; }
check() { if [ "$2" = "$3" ]; then ok "$1"; else bad "$1" "expected: $3 | got: $2"; fi; }

# code URL [curl args...] -> prints the HTTP status; the body is left in $TMP/body
code() { curl -s -m 15 -o "$TMP/body" -w '%{http_code}' "$@" || true; }
jfield() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); v=d.get(sys.argv[2]); print("" if v is None else v)' "$TMP/body" "$1" 2>/dev/null || true; }
BASE="$PUBLIC_BASE$PREFIX"

say "== verify $HOST"
say "-- on the droplet"
check "$SERVICE is active" "$(remote "systemctl is-active $(q "$SERVICE")" || true)" active
check "$CIVIC_SERVICE (civic) is still active" "$(remote "systemctl is-active $(q "$CIVIC_SERVICE")" || true)" active
check "$SERVICE has not restarted on its own (NRestarts)" "$(remote "systemctl show -p NRestarts --value $(q "$SERVICE")" || true)" 0
check "the daemon answers on loopback" "$(remote "curl -s -m 10 -o /dev/null -w '%{http_code}' http://127.0.0.1:8765/health" || true)" 200
# The RUNNING code is the DEPLOYED code: /health's commit (baked in at build time from the marker the sync wrote) against
# that marker. Catches "synced but not rebuilt" and "rebuilt but not restarted". Only compared when both sides exist.
if [ "${DEPLOY_SKIP_REV_CHECK:-0}" != 1 ]; then
  running="$(remote "curl -s -m 10 http://127.0.0.1:8765/health" 2>/dev/null | sed -n 's/.*"commit":"\([^"]*\)".*/\1/p' || true)"
  deployed="$(deployed_rev daemon || true)"
  if [ -z "$deployed" ]; then say "  note the droplet has no .deployed-rev marker: the running revision ($running) cannot be compared"
  elif [ -z "$running" ]; then say "  note /health reports no commit (a daemon older than this check): cannot compare with the deployed revision $deployed"
  elif [ "$running" = "$deployed" ]; then ok "the running daemon IS the deployed revision (${running:0:12})"
  else bad "the running daemon is the deployed revision" "running $running, deployed $deployed: the source was synced but the binary is stale (not rebuilt, or not restarted)"
  fi
fi
since="$(remote "systemctl show -p ActiveEnterTimestamp --value $(q "$SERVICE")" || true)"
journal="$(remote "journalctl -u $(q "$SERVICE") --since $(q "$since") --no-pager -o cat" || true)"
if grep -Eiq 'listening' <<<"$journal"; then ok "the journal shows it listening"; else bad "the journal shows it listening" "no 'listening' line since $since"; fi
errs="$(grep -Ei 'terminate called|what\(\)|aborted|core dumped|error|fatal|failed|sigsegv' <<<"$journal" || true)"
if [ -z "$errs" ]; then ok "the journal since start has no error lines"; else bad "the journal since start has no error lines" "$errs"; fi

if [ "${DEPLOY_SKIP_SITE:-0}" != 1 ]; then
  say "-- through nginx ($PUBLIC_BASE)"
  check "civic $PUBLIC_BASE/ is 200" "$(code "$PUBLIC_BASE/")" 200
  check "the game page $BASE/ is 200" "$(code "$BASE/")" 200
  check "the public site does NOT serve civic's /internal/ routes (404)" "$(code "$PUBLIC_BASE/internal/user-exists/x")" 404
fi

say "-- the daemon through $BASE"
check "lots (no cookie) is 200" "$(code "$BASE/presence/lots")" 200
check "region 1,1 lists occupants" "$(code "$BASE/presence/1,1")" 200

say "-- a real guest, named '$NAME'"
check "mint is 200" "$(code -X POST "$BASE/presence/guest")" 200
gid="$(jfield id)"; gsec="$(jfield secret)"
if [[ "$gid" =~ ^g_[0-9a-f]{16}$ ]]; then ok "the id has the g_ + 16 hex shape"; else bad "the id has the g_ + 16 hex shape" "$gid"; fi
if [[ "$gsec" =~ ^[0-9a-f]{32}$ ]]; then ok "the secret is 32 hex"; else bad "the secret is 32 hex" "(not shown)"; fi
body() { printf '{"display_name":"%s","id":"%s","secret":"%s","region":"1,1","x":%s,"y":%s,"facing":0}' "$NAME" "$gid" "${1:-$gsec}" "$2" "$3"; }
check "join is 200" "$(code -X POST --data-binary "$(body "$gsec" 5 5)" "$BASE/presence/join")" 200
check "join ok:true (under the name given)" "$(jfield ok)/$(jfield display_name)" "True/$NAME"
code "$BASE/presence/1,1" >/dev/null
if grep -q "\"id\":\"$gid\"" "$TMP/body"; then ok "the guest is listed in 1,1"; else bad "the guest is listed in 1,1"; fi
if grep -q "\"display_name\":\"$NAME\",\"x\":5,\"y\":5" "$TMP/body"; then ok "at the coordinates sent, under its name"; else bad "at the coordinates sent, under its name" "$(head -c 300 "$TMP/body")"; fi
check "move is 200" "$(code -X POST --data-binary "$(body "$gsec" 6 6)" "$BASE/presence/move")" 200
check "a bad secret is 403" "$(code -X POST --data-binary "$(body 00000000000000000000000000000000 6 6)" "$BASE/presence/move")" 403
check "leave is 200" "$(code -X POST --data-binary "{\"id\":\"$gid\",\"secret\":\"$gsec\",\"region\":\"1,1\"}" "$BASE/presence/leave")" 200
code "$BASE/presence/1,1" >/dev/null
if grep -q "\"id\":\"$gid\"" "$TMP/body"; then bad "the guest is gone after leave"; else ok "the guest is gone after leave"; fi

if [ -n "$BACKUP" ]; then
  say "-- the database against the pre-deploy backup"
  counts() { remote "python3 - $(q "$1")" <<'PY'
import sqlite3, sys
c = sqlite3.connect("file:%s?mode=ro" % sys.argv[1], uri=True)
for (t,) in c.execute("select name from sqlite_master where type='table' and name not like 'sqlite_%' order by name").fetchall():
    print(t, c.execute('select count(*) from "%s"' % t).fetchone()[0])
PY
  }
  counts "$BACKUP" > "$TMP/before" || bad "read the backup" "$BACKUP"
  counts "$DB_REMOTE" > "$TMP/after" || bad "read the live database" "$DB_REMOTE"
  ic="$(remote "python3 -c \"import sqlite3;print(sqlite3.connect('file:$DB_REMOTE?mode=ro',uri=True).execute('pragma integrity_check').fetchone()[0])\"" || true)"
  check "the live database passes integrity_check" "$ic" ok
  while read -r t n; do
    m="$(awk -v t="$t" '$1==t{print $2}' "$TMP/after")"
    if [ -z "$m" ]; then bad "table $t still exists" "it is in the backup ($n rows) and missing now"
    elif [ "$m" -lt "$n" ]; then bad "table $t lost rows" "backup $n, live $m (the orphan reap logs its own backup first; check the journal)"
    elif [ "$m" -gt "$n" ]; then ok "table $t: $n -> $m (grew)"
    else ok "table $t: $n rows, unchanged"; fi
  done < "$TMP/before"
fi

say ""
say "== $PASS passed, $FAIL failed"
[ "$FAIL" = 0 ]
