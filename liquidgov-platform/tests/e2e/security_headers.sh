#!/usr/bin/env bash
# Security headers on EVERY response, not only framed loads (Brandon 2026-11-13, decisions.md; security gate run
# 43503116478 found no CSP and no anti-framing header on normal pages, and the username in the failed-login URL).
#   - every page, redirect and asset: Content-Security-Policy with frame-ancestors 'self', object-src 'none',
#     base-uri 'self'; X-Frame-Options: SAMEORIGIN; each header exactly once (framed loads included)
#   - a failed login redirects to /login?err=login without the username (it would sit in logs and history)
# Throwaway store, port 18131. Never touches the live site or port 8080.
# Usage: bash security_headers.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-security-headers; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18131; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
PAL=$(sid pal)
[ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

SERVER=""
start() {
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$BIN") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
start

# headers_ok <label> <header file>: the three CSP parts and X-Frame-Options, each header line exactly once
headers_ok() {
    local f=$2
    check "$1: CSP has frame-ancestors 'self'" "grep -i '^Content-Security-Policy:' $f | grep -q \"frame-ancestors 'self'\"" "$(grep -i '^content-security' $f)"
    check "$1: CSP has object-src 'none'" "grep -i '^Content-Security-Policy:' $f | grep -q \"object-src 'none'\"" "$(grep -i '^content-security' $f)"
    check "$1: CSP has base-uri 'self'" "grep -i '^Content-Security-Policy:' $f | grep -q \"base-uri 'self'\"" "$(grep -i '^content-security' $f)"
    check "$1: X-Frame-Options SAMEORIGIN" "grep -qi '^X-Frame-Options: SAMEORIGIN' $f" "$(cat $f)"
    check "$1: one X-Frame-Options line" "[ \"\$(grep -ci '^X-Frame-Options:' $f)\" = 1 ]" "$(grep -ci '^X-Frame-Options:' $f) lines"
    check "$1: one site CSP line" "[ \"\$(grep -i '^Content-Security-Policy:' $f | grep -c \"base-uri 'self'\")\" = 1 ]" "$(grep -ci '^Content-Security-Policy:' $f) CSP lines"
}

curl -s -D "$W/h1.txt" -o /dev/null "$URL/"
headers_ok "signed-out /" "$W/h1.txt"
curl -s -D "$W/h2.txt" -o /dev/null "$URL/login"
headers_ok "/login" "$W/h2.txt"
curl -s -D "$W/h3.txt" -o /dev/null -b "lg_session=$PAL" -H "Sec-Fetch-Dest: document" "$URL/feed"
headers_ok "signed-in /feed (normal load)" "$W/h3.txt"
curl -s -D "$W/h4.txt" -o /dev/null -b "lg_session=$PAL" -H "Sec-Fetch-Dest: iframe" "$URL/feed"
headers_ok "signed-in /feed (framed load)" "$W/h4.txt"
curl -s -D "$W/h5.txt" -o /dev/null "$URL/me"
check "signed-out /me is a redirect (precondition)" "grep -q '^HTTP/1.1 303' $W/h5.txt" "$(head -1 $W/h5.txt)"
headers_ok "a 303 redirect" "$W/h5.txt"
curl -s -D "$W/h6.txt" -o /dev/null "$URL/no-such-page-xyz"
headers_ok "a 404 page" "$W/h6.txt"

# Failed login: the error code stays, the username does not ride along in the URL.
curl -s -D "$W/h7.txt" -o /dev/null -d "username=pal&password=wrong-password-123&next=%2Fme" "$URL/login"
loc=$(grep -i '^Location:' "$W/h7.txt" | tr -d '\r')
check "failed login redirects to /login with err=login (precondition)" '[[ "$loc" == *"/login?err=login"* ]]' "$loc"
check "failed login URL has no username" '[[ "$loc" != *username=* ]] && [[ "$loc" != *pal* ]]' "$loc"
check "failed login URL keeps the safe next path" '[[ "$loc" == *"next=%2Fme"* ]]' "$loc"
# should-not-fire: a quoted mention of the word "username" in a normal page is not a URL leak
check "the login page still has its username field" 'curl -s "$URL/login" | grep -q "name=\"username\""' "no username field"

check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate called" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
