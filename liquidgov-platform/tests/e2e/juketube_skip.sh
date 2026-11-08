#!/usr/bin/env bash
# Juketube fix (Brandon 2026-11-08, "do a and b"; repro in scratch/2026-11-08-claude-juketube): (A) on a shared station
# a player error or a bare/stale/early "ended" from one listener no longer skips the room's track; it is skipped when
# at least 25% of connected listeners report it unplayable (juketube_skip_client.py). (B) /jukebox/oembed says when YouTube
# refuses embedding (401) or the video is gone (404), checked here with a fake curl so no network is needed. Plus static
# checks that the served juketube.js uses all of it.
# Usage: bash juketube_skip.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18096. Never touches the live site.
set -u
BIN=$1; SEED=$2
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-juketube-skip; rm -rf "$W"; mkdir -p "$W/blobs" "$W/fakebin"
PORT=18096; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
[ -f "$HERE/tests/e2e/juketube_skip_client.py" ] || { echo "missing juketube_skip_client.py next to this script"; exit 2; }

# The server's curl_get shells out to curl; this one answers YouTube's oEmbed like YouTube does, and anything else
# goes to the real curl.
REAL_CURL=$(command -v curl)
cat > "$W/fakebin/curl" <<EOF
#!/usr/bin/env bash
case "\$*" in
  *oembed*BLOCKEDxxxx*) printf 'Unauthorized' ;;
  *oembed*MISSINGxxxx*) printf 'Not Found' ;;
  *oembed*EMBEDOKxxxx*) printf '{"title":"A fine song","author_name":"Someone"}' ;;
  *) exec "$REAL_CURL" "\$@" ;;
esac
EOF
chmod +x "$W/fakebin/curl"

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
A=$(sid Bbottin); B=$(sid pal)
[ -n "$A" ] && [ -n "$B" ] || { echo "seed gave no sessions"; exit 1; }

SERVER=""
start() {
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS PATH="$W/fakebin:$PATH" LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs \
        LIQUIDGOV_PORT=$PORT "$BIN") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        "$REAL_CURL" -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; cat "$W/server.log"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
start

# (A) the station over its websocket
if python3 "$HERE/tests/e2e/juketube_skip_client.py" $PORT "$A" "$B" > "$W/ws.out" 2>&1; then
    ok "websocket checks ($(tail -1 "$W/ws.out"))"
else
    bad "websocket checks"; grep -E '^FAIL|Error|error' "$W/ws.out" | head -15; tail -3 "$W/ws.out"
fi
check "the server log names the unplayable votes" 'grep -q "\[jukebox\] unplayable" "$W/server.log"' "no unplayable log line"

# (B) the embed check
oe() { "$REAL_CURL" -s -b "lg_session=$A" "$URL/jukebox/oembed?id=$1"; }
r=$(oe BLOCKEDxxxx); check "a video whose owner blocks embedding answers embeddable:false" '[[ "$r" == "{\"embeddable\":false}" ]]' "$r"
r=$(oe MISSINGxxxx); check "a removed video answers embeddable:false with missing" '[[ "$r" == *"\"embeddable\":false"* && "$r" == *"\"missing\":true"* ]]' "$r"
r=$(oe EMBEDOKxxxx); check "a normal video answers its title and embeddable:true" '[[ "$r" == *"A fine song"* && "$r" == *"\"embeddable\":true"* ]]' "$r"
r=$(oe UNKNOWNxxxx); check "an unknown answer (network) stays {} so a pick is not blocked" '[[ "$r" == "{}" ]]' "$r"

# the browser code that uses it, as served
js=$("$REAL_CURL" -s "$URL/assets/juketube.js")
check "juketube.js is served" '[ ${#js} -gt 1000 ]' "${#js} bytes"
check "juketube.js names the track when it reports an end" 'grep -q "ended: endedId" <<<"$js" && ! grep -q "ended: true" <<<"$js"' "bare ended still sent"
check "juketube.js votes unplayable on a shared station" 'grep -q "unplayable: id" <<<"$js" && grep -q "sendUnplayable(lastId)" <<<"$js"' "no unplayable vote"
check "juketube.js checks embeddability before telling the station" 'grep -q "checkEmbeddable(id" <<<"$js" && grep -q "/jukebox/oembed?id=" <<<"$js"' "no check"
check "juketube.js stops the idle player only when something is loaded" 'grep -q "if (lastId) {" <<<"$js"' "loop guard missing"

stop
echo "passes=$passes fails=$fails"
[ "$fails" = 0 ]
