#!/usr/bin/env bash
# What a REAL nginx does with the www -> apex redirect (BUILD www apex). Self-contained: it starts its own nginx (its own prefix, loopback
# ports 18443/18080, never the system config) in front of a tiny stub upstream, once with the UNtransformed live config (the control: www used
# to serve the whole site) and once with the transformed one.
#   nginx_www_behavior.sh <nginx binary> <live-shaped conf> <transformed conf> <zone file> <work dir>
# Prints "ok   ..." / "FAIL ..." lines and a final "behavior: N checks, F failed"; exit 1 if any failed.
NGINX="$1"; LIVE="$2"; NEW="$3"; ZONE="$4"; W="$5"
SP=18443; HP=18080; UP=18191
n=0; f=0
ok()  { n=$((n+1)); echo "  ok   $1"; }
bad() { n=$((n+1)); f=$((f+1)); echo "  FAIL $1"; [ -n "${2:-}" ] && echo "       $2"; }
eq()  { if [ "$2" = "$3" ]; then ok "$1"; else bad "$1" "expected: $3 | got: $2"; fi; }
mkdir -p "$W/tmp" "$W/site"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$W/k.pem" -out "$W/c.pem" -subj /CN=liquidgov.us -days 2 >/dev/null 2>&1 || { echo "FATAL: openssl could not make a test certificate"; exit 2; }
echo apex-site > "$W/site/index.html"; echo laws-page > "$W/site/laws"
( cd "$W/site" && exec python3 -m http.server "$UP" --bind 127.0.0.1 ) >/dev/null 2>&1 &
STUB=$!
NGPID=""
stop_ng() { [ -f "$W/nginx.pid" ] && kill "$(cat "$W/nginx.pid")" 2>/dev/null; rm -f "$W/nginx.pid"; sleep 0.3; }
cleanup() { stop_ng; kill "$STUB" 2>/dev/null; }
trap cleanup EXIT
for _ in $(seq 1 30); do curl -s -o /dev/null "http://127.0.0.1:$UP/" && break; sleep 0.1; done

# the site config as this test can run it: loopback ports, a throwaway certificate, the stub as upstream (no IPv6, no certbot include files)
mkconf() {
  sed -e '/listen \[::\]:/d' \
      -e "s|listen 443 ssl; # managed by Certbot|listen 127.0.0.1:$SP ssl;|" \
      -e "s|listen 80;|listen 127.0.0.1:$HP;|" \
      -e "s|ssl_certificate /etc/letsencrypt/live/liquidgov.us/fullchain.pem;.*|ssl_certificate $W/c.pem;|" \
      -e "s|ssl_certificate_key /etc/letsencrypt/live/liquidgov.us/privkey.pem;.*|ssl_certificate_key $W/k.pem;|" \
      -e '/options-ssl-nginx.conf/d' -e '/ssl_dhparam/d' \
      -e "s|http://127.0.0.1:8080|http://127.0.0.1:$UP|" -e "s|http://127.0.0.1:8765|http://127.0.0.1:$UP|" "$1" > "$2"
}
start_ng() { # $1 = source conf
  mkconf "$1" "$W/site.conf"
  local asroot=""; [ "$(id -u)" = 0 ] && asroot="user root;"      # (workers must be able to write the temp dirs when run as root; CI runs as a normal user)
  cat > "$W/nginx.conf" <<EOF
$asroot
pid $W/nginx.pid;
error_log $W/error.log warn;
worker_processes 1;
events {}
http {
  access_log off;
  client_body_temp_path $W/tmp/body; proxy_temp_path $W/tmp/proxy; fastcgi_temp_path $W/tmp/fcgi;
  uwsgi_temp_path $W/tmp/uwsgi; scgi_temp_path $W/tmp/scgi;
  include $ZONE;
  include $W/site.conf;
}
EOF
  if ! "$NGINX" -p "$W/" -e "$W/error.log" -c "$W/nginx.conf" -t > "$W/t.log" 2>&1; then bad "nginx accepts the config" "$(cat "$W/t.log")"; return 1; fi
  "$NGINX" -p "$W/" -e "$W/error.log" -c "$W/nginx.conf" || { bad "nginx starts" "$(cat "$W/error.log")"; return 1; }
  for _ in $(seq 1 50); do curl -sk -o /dev/null "https://127.0.0.1:$SP/" && return 0; sleep 0.1; done
  bad "nginx answers" "$(cat "$W/error.log")"; return 1
}
# hs HOST PATH  -> "code redirect_url" over https ; hp HOST PATH -> over plain http (port 80's block)
hs() { curl -sk -o /dev/null -m 10 -w '%{http_code} %{redirect_url}' -H "Host: $1" "https://127.0.0.1:$SP$2"; }
hp() { curl -s  -o /dev/null -m 10 -w '%{http_code} %{redirect_url}' -H "Host: $1" "http://127.0.0.1:$HP$2"; }
body() { curl -sk -m 10 -H "Host: $1" "https://127.0.0.1:$SP$2"; }

echo "-- control: the UNtransformed live config (this is what production did before): www serves the whole site"
if start_ng "$LIVE"; then
  eq "control: https://www/ is 200 (a second copy of the site)" "$(hs www.liquidgov.us /)" "200 "
  eq "control: http://www/x is a 301 that stays on www (https://www...)" "$(hp www.liquidgov.us '/x?q=1')" "301 https://www.liquidgov.us/x?q=1"
fi
stop_ng

echo "-- the transformed config"
if start_ng "$NEW"; then
  eq "https://www/ -> the apex" "$(hs www.liquidgov.us /)" "301 https://liquidgov.us/"
  eq "the path and the query string survive" "$(hs www.liquidgov.us '/laws?x=1&y=2')" "301 https://liquidgov.us/laws?x=1&y=2"
  eq "a deep path" "$(hs www.liquidgov.us /u/pal/canvas/latest)" "301 https://liquidgov.us/u/pal/canvas/latest"
  eq "/map on www -> the apex /map" "$(hs www.liquidgov.us /map)" "301 https://liquidgov.us/map"
  eq "/internal/ on www is redirected too (the apex then 404s it as before)" "$(hs www.liquidgov.us /internal/blocked)" "301 https://liquidgov.us/internal/blocked"
  eq "an UPPER-CASE Host header is still www" "$(hs WWW.LIQUIDGOV.US /laws)" "301 https://liquidgov.us/laws"
  eq "a Host header with a port is still www" "$(hs www.liquidgov.us:$SP /x)" "301 https://liquidgov.us/x"
  eq "http://www/x?q=1 goes straight to the apex (one hop)" "$(hp www.liquidgov.us '/x?q=1')" "301 https://liquidgov.us/x?q=1"
  eq "http://apex/ is unchanged: https://apex/" "$(hp liquidgov.us /)" "301 https://liquidgov.us/"
  eq "http://apex/path?q keeps path and query as before" "$(hp liquidgov.us '/a/b?c=d')" "301 https://liquidgov.us/a/b?c=d"
  eq "an unknown host on port 80 is still a 404 (certbot's default)" "$(hp other.example /)" "404 "
  eq "the apex still serves the site (proxied to the upstream): /" "$(body liquidgov.us /)" "apex-site"
  eq "the apex still serves a path through the proxy: /laws" "$(body liquidgov.us /laws)" "laws-page"
  eq "https://apex/ is 200, not a redirect (no loop)" "$(hs liquidgov.us /)" "200 "
  n1="$(hs www.liquidgov.us /a)"; loc="${n1#* }"; eq "the redirect target is the apex host itself, never www (no loop)" "${loc#https://}" "liquidgov.us/a"
fi
echo "behavior: $n checks, $f failed"
[ "$f" = 0 ]
