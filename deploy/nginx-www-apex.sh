#!/usr/bin/env bash
# Make www.liquidgov.us redirect to the apex on the production droplet (BUILD www apex). nginx only: civic and the daemon are not restarted.
#
#   deploy/nginx-www-apex.sh          PLAN ONLY: what file, what changes (a diff), what is live now. Changes nothing.
#   deploy/nginx-www-apex.sh --go     apply: backup, edit, `nginx -t`, graceful reload, verify, and roll back automatically on any regression.
#
# The edit itself is deploy/nginx-www-apex.py (a pure, idempotent, refuse-if-unsure transform; tests/test_nginx_www.sh covers it).
# The file to edit is the one nginx actually LOADED (`nginx -T` names it), never a grep of /etc/nginx (which also matches old .bak copies).
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; PY="$HERE/nginx-www-apex.py"
KEY="${LIQUIDGOV_DEPLOY_KEY:-$HOME/.ssh/liquidgov_deploy_key}"
HOSTSSH="root@143.198.159.58"; U=https://liquidgov.us; W=https://www.liquidgov.us
GO=0; [ "${1:-}" = "--go" ] && GO=1
R() { ssh -i "$KEY" -o ConnectTimeout=15 "$HOSTSSH" "$@"; }
ct()  { curl -s -o /dev/null -m 20 -w '%{content_type}' "$1"; }
red() { curl -s -o /dev/null -m 20 -w '%{http_code} %{redirect_url}' "$@"; }
die() { echo "ABORT: $*"; exit 1; }

echo "== www -> apex: $([ $GO = 1 ] && echo APPLY || echo 'PLAN ONLY (nothing changes; pass --go to apply)')"
FILES=$(R 'nginx -T 2>/dev/null | awk "/^# configuration file /{f=\$4; sub(/:\$/,\"\",f)} /server_name liquidgov.us www.liquidgov.us;/{print f}" | while read f; do readlink -f "$f"; done | sort -u')
[ "$(echo "$FILES" | grep -c .)" = "1" ] || die "expected exactly one loaded config file naming both hosts, got: [$FILES]"
CONF="$FILES"; echo "loaded config that names both hosts: $CONF"
R 'nginx -t 2>&1 | tail -1' | grep -q successful || die "nginx -t already fails before any change"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
R "cat '$CONF'" > "$TMP/live.conf"
python3 "$PY" --check "$TMP/live.conf"; rc=$?
[ $rc = 0 ] && { echo "already applied: nothing to do"; exit 0; }
python3 "$PY" "$TMP/live.conf" > "$TMP/new.conf" || die "the transform refused this config (see above); it will not guess"
echo "--- the change (live -> new):"; diff "$TMP/live.conf" "$TMP/new.conf"
echo "--- live now (before):"
declare -A BEFORE; PATHS="/commons/ /commons/style.css /commons/js/main.js /commons/tiles.png /commons/icon-192.png /commons/manifest.webmanifest"
for p in $PATHS; do BEFORE[$p]="$(ct $U$p)"; done
echo "   $W/           -> $(red $W/)"
echo "   http://www/   -> $(red http://www.liquidgov.us/x?q=1)"
echo "   $U/           -> $(red $U/)"
[ $GO = 1 ] || { echo; echo "PLAN ONLY. Apply with: deploy/nginx-www-apex.sh --go"; exit 0; }

TS=$(date -u +%Y%m%dT%H%M%SZ); BK=/var/backups/nginx-www-apex-$TS
R "mkdir -p $BK && cp -a '$CONF' $BK/ && echo backup: $BK/$(basename $CONF)" || die "no backup"
echo "== apply"
R "cat > '$CONF.new-$TS'" < "$TMP/new.conf" || die "upload failed"
R "cat '$CONF.new-$TS' > '$CONF' && rm -f '$CONF.new-$TS'" || die "could not write the config"
rollback() { echo "ROLLING BACK to $BK/$(basename $CONF)"; R "cat $BK/$(basename $CONF) > '$CONF' && nginx -t 2>&1 | tail -1 && systemctl reload nginx"; echo "ROLLED BACK"; exit 1; }
R 'nginx -t 2>&1' | tail -2 | grep -q successful || { echo "nginx -t FAILED after the edit"; rollback; }
R 'systemctl reload nginx && sleep 1 && systemctl is-active nginx' || rollback
echo "== verify (auto-rollback on any regression)"
bad=0
chk() { # description  got  want-regex
  if [[ "$2" =~ $3 ]]; then echo "   ok   $1: $2"; else echo "   FAIL $1: $2   (wanted $3)"; bad=1; fi; }
chk "https://www/                 " "$(red $W/)" '^301 https://liquidgov\.us/$'
chk "https://www/laws?x=1&y=2     " "$(red "$W/laws?x=1&y=2")" '^301 https://liquidgov\.us/laws\?x=1&y=2$'
chk "https://www/commons/         " "$(red $W/commons/)" '^301 https://liquidgov\.us/commons/$'
chk "https://www/internal/blocked (redirects; the apex then 404s it)" "$(red $W/internal/blocked)" '^301 https://liquidgov\.us/internal/blocked$'
chk "http://www/x?q=1 (one hop)   " "$(red 'http://www.liquidgov.us/x?q=1')" '^301 https://liquidgov\.us/x\?q=1$'
chk "http://apex/ (unchanged)     " "$(red http://liquidgov.us/)" '^301 https://liquidgov\.us/$'
chk "UPPER-CASE Host on www       " "$(red -H 'Host: WWW.LIQUIDGOV.US' $U/laws)" '^301 https://liquidgov\.us/laws$'
chk "the whole chain ends on the apex 200" "$(curl -sL -o /dev/null -m 30 -w '%{http_code} %{url_effective}' $W/laws)" '^200 https://liquidgov\.us/laws$'
chk "apex /                        " "$(curl -s -o /dev/null -m 20 -w '%{http_code}' $U/)" '^200$'
chk "apex /world (unchanged)       " "$(red $U/world)" '^303 https://liquidgov\.us/commons/$'
chk "apex /internal/session-user   " "$(curl -s -o /dev/null -m 20 -w '%{http_code}' $U/internal/session-user)" '^404$'
for p in $PATHS; do now="$(ct $U$p)"; if [ "$now" = "${BEFORE[$p]}" ]; then echo "   ok   $p type unchanged: $now"; else echo "   FAIL $p type ${BEFORE[$p]} -> $now"; bad=1; fi; done
chk "manifest type                 " "$(ct $U/commons/manifest.webmanifest)" '^application/manifest\+json'
chk "social manifest (civic)       " "$(ct $U/manifest-social.webmanifest)" '^application/manifest\+json'
[ "$bad" = 0 ] || rollback
R 'echo "nginx: $(systemctl is-active nginx)  liquidgov: $(systemctl is-active liquidgov) NRestarts=$(systemctl show liquidgov -p NRestarts --value)  commons-daemon: $(systemctl is-active commons-daemon) NRestarts=$(systemctl show commons-daemon -p NRestarts --value)"; echo "nginx error log (last 3):"; tail -3 /var/log/nginx/error.log | cut -c1-150'
echo "DONE  backup: $BK/$(basename $CONF)  config: $CONF"
