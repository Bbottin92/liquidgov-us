#!/usr/bin/env bash
# The nginx rate limit on guest minting (deploy/nginx-commons.conf + nginx-commons-http.conf).
#   part 1  what the config SAYS (text checks, no nginx needed): exactly one limit_req, in exactly the guest
#           location, never near /internal/, 429 not 503, no custom body, enforcing by default.
#   part 2  what it DOES: a real nginx (as this user, its own prefix and port, never the system config) in
#           front of the real daemon: a burst is absorbed, then 429s; only minting is limited; the limit is
#           per address; it recovers as tokens refill; the walking routes are never limited.
# In CI nginx is installed (a missing nginx there is a FAILURE, not a skip).
#
# Timing: the bucket refills one token per 3s, so every "is limited right now" check runs IMMEDIATELY after the
# burst (milliseconds), and the checks that take seconds (other routes, the refill) assert nothing that a
# refilled token could change.
source "$(dirname "$0")/lib.sh"

DEPLOY="$DAEMON_DIR/deploy"; FRAG="$DEPLOY/nginx-commons.conf"; ZONE="$DEPLOY/nginx-commons-http.conf"
active() { sed 's/#.*//' "$1" | grep -v '^[[:space:]]*$'; }     # a config file without its comments

echo "-- the config text"
zl="$(active "$ZONE" | grep -c 'limit_req_zone')"
assert_eq "exactly one limit_req_zone" "1" "$zl"
assert_contains "keyed on the real peer address" "$(active "$ZONE")" 'limit_req_zone $binary_remote_addr'
assert_contains "named commons_guest" "$(active "$ZONE")" 'zone=commons_guest:'
n="$(active "$FRAG" | grep -cE '^[[:space:]]*limit_req[[:space:]]+zone=')"
assert_eq "exactly ONE limit_req in the location fragment" "1" "$n"
loc="$(active "$FRAG" | awk '/^[[:space:]]*location[[:space:]]/ {l=$0} /^[[:space:]]*limit_req[[:space:]]+zone=/ {print l}')"
assert_contains "and it is inside the EXACT-match guest location" "$loc" 'location = /commons/presence/guest'
assert_not_contains "never inside the /commons/presence/ prefix" "$loc" 'location /commons/presence/ '
assert_not_contains "civic's /internal/ is never named by an active line" "$(active "$FRAG"; active "$ZONE")" "internal"
assert_contains "the status is 429 (503 reads as 'the daemon is down')" "$(active "$FRAG")" 'limit_req_status   429'
assert_not_contains "no custom error page/body (a JSON body would be read as credentials by the client)" "$(active "$FRAG")" 'error_page'
assert_not_contains "enforcing by default (dry run stays commented out)" "$(active "$FRAG")" 'limit_req_dry_run'
burst="$(active "$FRAG" | sed -n 's/.*limit_req[[:space:]]*zone=commons_guest.*burst=\([0-9]*\).*/\1/p')"
[ "${burst:-0}" -ge 20 ] && _ok "the burst ($burst) absorbs a shared address re-minting after a restart" || _bad "burst >= 20" "burst=$burst"
assert_contains "nodelay: nobody is slowed inside the burst" "$(active "$FRAG")" 'nodelay'

NGINX="$(command -v nginx || true)"; [ -n "$NGINX" ] || { [ -x /usr/sbin/nginx ] && NGINX=/usr/sbin/nginx; }
if [ -z "$NGINX" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "nginx is installed in CI" "no nginx binary"; else echo "  skip: no nginx binary here (the behavioural half runs in CI)"; fi
  exit 0
fi

echo "-- a real nginx in front of the real daemon ($("$NGINX" -v 2>&1))"
start_civic; start_daemon
PORT=18090; NG="$WORK/ng"; mkdir -p "$NG/tmp"; NGX="http://127.0.0.1:$PORT"
cat > "$NG/nginx.conf" <<EOF
pid $NG/nginx.pid;
error_log $NG/error.log warn;
worker_processes 1;
events {}
http {
  access_log off;
  client_body_temp_path $NG/tmp/body; proxy_temp_path $NG/tmp/proxy; fastcgi_temp_path $NG/tmp/fcgi;
  uwsgi_temp_path $NG/tmp/uwsgi; scgi_temp_path $NG/tmp/scgi;
  map \$http_upgrade \$connection_upgrade { default upgrade; '' close; }
  include $ZONE;
  server { listen 127.0.0.1:$PORT; include $FRAG; }
}
EOF
ngx() { "$NGINX" -p "$NG/" -e "$NG/error.log" -c "$NG/nginx.conf" "$@"; }
_ng_stop() { [ -f "$NG/nginx.pid" ] && kill "$(cat "$NG/nginx.pid")" 2>/dev/null; return 0; }
_ng_exit() { local rc=$?; _ng_stop; return $rc; }
trap '_ng_exit; _on_exit' EXIT

if ngx -t > "$WORK/ngt.log" 2>&1; then _ok "nginx accepts the fragment together with the zone file"; else _bad "nginx -t" "$(cat "$WORK/ngt.log")"; exit 1; fi
ngx || { echo "FATAL: nginx did not start"; cat "$NG/error.log"; exit 1; }
for _ in $(seq 1 50); do curl -s -o /dev/null "$NGX/commons/presence/lots" && break; sleep 0.1; done
mint_code() { curl -s -o /dev/null -w '%{http_code}' "$@" -X POST "$NGX/commons/presence/guest"; }

echo "-- a mint through nginx reaches the daemon (the exact-match proxy_pass rewrites the path)"
post "$NGX/commons/presence/guest" ""
assert_code "mint is 200" 200
gid="$(jget id)"; gsec="$(jget secret)"
[[ "$gid" =~ ^g_[0-9a-f]{16}$ ]] && _ok "with a real credential (g_ + 16 hex)" || _bad "a real credential" "$BODY"

echo "-- a burst: absorbed, then 429"
ok=1; lim=0; other=0; first429=0; n=1     # the mint above is request 1
for _ in $(seq 1 60); do
  c="$(mint_code)"; n=$((n+1))
  case "$c" in 200) ok=$((ok+1));; 429) lim=$((lim+1)); [ "$first429" = 0 ] && first429=$n;; *) other=$((other+1));; esac
done
assert_between "accepted out of 61 rapid mints (burst 30 + the first, plus a little refill)" 30 34 "$ok"
assert_between "refused with 429" 26 31 "$lim"
assert_eq "nothing else came back (no 502/503/500)" "0" "$other"
[ "$first429" -gt 28 ] && _ok "the first 429 came only after the burst (request $first429)" || _bad "the first 429 came only after the burst" "request $first429"

echo "-- right now, while limited (milliseconds after the burst)"
_run -X POST "$NGX/commons/presence/guest"
assert_code "a limited mint answers 429" 429
python3 -c 'import json,sys; json.loads(sys.stdin.read())' <<<"$BODY" 2>/dev/null && r=json || r=not-json
assert_eq "with nginx's default page, NOT JSON (the client would read JSON as credentials)" "not-json" "$r"
assert_eq "another address (127.0.0.2) mints fine: the limit is per address" "200" "$(mint_code --interface 127.0.0.2)"
assert_eq "while 127.0.0.1 is still limited" "429" "$(mint_code)"
assert_contains "the refusals are logged" "$(cat "$NG/error.log")" "limiting requests"

echo "-- only minting is limited"
allok=1; for _ in $(seq 1 30); do [ "$(curl -s -o /dev/null -w '%{http_code}' "$NGX/commons/presence/lots")" = 200 ] || allok=0; done
assert_eq "30 lots polls in a row are all 200" "1" "$allok"
allok=1; for _ in $(seq 1 30); do [ "$(curl -s -o /dev/null -w '%{http_code}' "$NGX/commons/presence/1,1")" = 200 ] || allok=0; done
assert_eq "30 region polls in a row are all 200" "1" "$allok"
post "$NGX/commons/presence/join" "{\"id\":\"$gid\",\"secret\":\"$gsec\",\"region\":\"1,1\",\"x\":5,\"y\":5,\"facing\":0}"
assert_contains "join with a credential minted earlier works" "$BODY" '"ok":true'
post "$NGX/commons/presence/move" "{\"id\":\"$gid\",\"secret\":\"$gsec\",\"region\":\"1,1\",\"x\":6,\"y\":6,\"facing\":1}"
assert_contains "move works" "$BODY" '"ok":true'
post "$NGX/commons/presence/leave" "{\"id\":\"$gid\",\"secret\":\"$gsec\",\"region\":\"1,1\"}"
assert_contains "leave works" "$BODY" '"ok":true'
post "$NGX/commons/presence/guests" ""
[ "$CODE" != 429 ] && _ok "a different path under the prefix (/guests) is not the limited route (answered $CODE)" || _bad "/guests must not be limited" "$CODE"
[ "$(curl -s -o /dev/null -w '%{http_code}' "$D/health")" = 200 ] && _ok "the daemon itself is untouched" || _bad "the daemon is untouched"

echo "-- it is a rate, not a ban: tokens refill (one per 3s)"
sleep 3.5
assert_eq "after the refill a mint is accepted again" "200" "$(mint_code)"
again=0; for i in $(seq 1 8); do [ "$(mint_code)" = 429 ] && { again=$i; break; }; done
[ "$again" -gt 0 ] && _ok "and the limit re-engages within a few requests (429 on the $again-th)" || _bad "the limit re-engages" "no 429 in 8 mints after the refill"
