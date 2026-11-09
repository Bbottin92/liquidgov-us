#!/usr/bin/env bash
# curl_get says why it failed (site sweep 2026-11-09 item 18, Brandon OK'd): curl's exit status was thrown away, so a
# timeout or a refused connection looked like an empty answer, and Google sign-in said "couldn't verify". Now
# curl_get returns curl's exit status and Google sign-in says Google could not be reached. curl's own error line
# already reaches the server log (stderr), so no second log line is added. The YouTube check keeps answering "{}"
# (an unknown answer lets a pick through on purpose; the station's vote protects the room). A fake curl on PATH.
# Usage: bash curl_errors.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18101. Never touches the live site.
set -u
BIN=$1; SEED=$2
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-curl-errors; rm -rf "$W"; mkdir -p "$W/blobs" "$W/fakebin"
PORT=18101; URL=http://127.0.0.1:$PORT
TOKEN=SECRETTOKEN0123456789
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

REAL_CURL=$(command -v curl)
if "$REAL_CURL" -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }

# The server shells out to curl: oEmbed times out (28), Google refuses the connection (7) unless the token is the
# WRONGAUD one, which answers like Google for another app. Everything else goes to the real curl.
cat > "$W/fakebin/curl" <<EOF
#!/usr/bin/env bash
case "\$*" in
  *youtube.com/oembed*) echo "curl: (28) Operation timed out after 5001 milliseconds" >&2; exit 28 ;;
  *tokeninfo*WRONGAUD*) printf '{"aud":"someone-else","iss":"accounts.google.com","sub":"1","email_verified":"true"}' ;;
  *tokeninfo*) echo "curl: (7) Failed to connect to oauth2.googleapis.com port 443" >&2; exit 7 ;;
  *) exec "$REAL_CURL" "\$@" ;;
esac
EOF
chmod +x "$W/fakebin/curl"

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
A=$(awk '$1=="Bbottin"{print $2}' "$W/sessions.txt")
[ -n "$A" ] || { echo "seed gave no sessions"; exit 1; }

(cd "$HERE" && exec env PATH="$W/fakebin:$PATH" LIQUIDGOV_GOOGLE_CLIENT_ID=test-client.apps.googleusercontent.com \
    LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    "$REAL_CURL" -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
"$REAL_CURL" -s -o /dev/null --max-time 1 $URL/ || { echo "server never answered"; exit 1; }

# YouTube check: the same answer on a timeout
r=$("$REAL_CURL" -s -b "lg_session=$A" "$URL/jukebox/oembed?id=TIMEOUTxxxx")
check "oEmbed answer unchanged on a timeout: {}" '[ "$r" = "{}" ]' "$r"

# Google sign-in: unreachable vs refused
loc=$("$REAL_CURL" -s -o /dev/null -w '%{redirect_url}' -d "credential=$TOKEN" "$URL/login/google")
check "precondition: Google sign-in is configured (it redirects back to /login)" '[[ "$loc" == *"/login?err="* ]]' "$loc"
check "Google unreachable: err=google-down" '[[ "$loc" == *"err=google-down" ]]' "$loc"
sleep 0.3
check "precondition: curl's own error line reached the server log" 'grep -q "Failed to connect to oauth2.googleapis.com" "$W/server.log"' "no curl line"
check "the sign-in token never reaches the log" '! grep -q "$TOKEN" "$W/server.log"' "token found"
page=$("$REAL_CURL" -s "$URL/login?err=google-down")
check "the login page says Google could not be reached" 'echo "$page" | grep -q "reach Google"' "$(echo "$page" | grep -o 'class="warn">[^<]*' | head -1)"
loc=$("$REAL_CURL" -s -o /dev/null -w '%{redirect_url}' -d "credential=WRONGAUD$TOKEN" "$URL/login/google")
check "Google answered but for another app: still err=google (unchanged)" '[[ "$loc" == *"err=google" ]]' "$loc"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
