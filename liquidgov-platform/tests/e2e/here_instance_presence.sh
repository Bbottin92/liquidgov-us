#!/usr/bin/env bash
# Spatial step 5 (SPATIAL_EVOLUTION.md): presence is scoped to the instance. In a /here/ws room, a person only gets
# the dots whose instanceId equals their own. A client names its instance with "instanceId" in the move message it
# already sends; one that sends none stays in the room's own instance (how every page behaves today).
# The one check: two people in the same room. Neither names an instance -> they see each other (precondition).
# They name different instances -> neither sees the other. They name the same instance -> they see each other.
# Usage: bash here_instance_presence.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18132. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
E2E="$(cd "$(dirname "$0")" && pwd)"
W=/tmp/lg-here-instance-presence; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18132; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] && [ -f "$E2E/here_ws_client.py" ] || { echo "missing binaries or client"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal)
[ -n "$ADMIN" ] && [ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER $(jobs -p) 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
curl -s -o /dev/null --max-time 1 $URL/ || { echo "server never answered"; exit 1; }
for s in $ADMIN $PAL; do curl -s -o /dev/null -b "lg_session=$s" -d choice=1 -d back=/settings "$URL/here/choice"; done

# One person in the room /u/stranger for <seconds>, sending x, y and (unless "-") instanceId every 0.3 s; prints who
# is in the LAST snapshot it got, sorted, or REFUSED/CLOSED. Uses here_ws_client.py's frame code as it is.
visit() { # <session> <instance|-> <seconds>
python3 - "$E2E" "$PORT" "$1" "$2" "$3" <<'EOF'
import base64, json, os, socket, sys, time
sys.path.insert(0, sys.argv[1])
import here_ws_client as c
port, session, inst, secs = sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5])
s = socket.create_connection(("127.0.0.1", int(port)), timeout=5)
key = base64.b64encode(os.urandom(16)).decode()
s.sendall(("GET /here/ws?page=/u/stranger HTTP/1.1\r\nHost: 127.0.0.1:%s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nCookie: lg_session=%s\r\n\r\n" % (port, key, session)).encode())
head = b""
while b"\r\n\r\n" not in head:
    chunk = s.recv(4096)
    if not chunk:
        break
    head += chunk
if b" 101 " not in head.split(b"\r\n", 1)[0] + b" ":
    print("REFUSED"); sys.exit()
buf, last, end, nxt = head.split(b"\r\n\r\n", 1)[1], None, time.time() + secs, 0.0
s.settimeout(0.1)
msg = {"x": 0.5, "y": 0.5}
if inst != "-":
    msg["instanceId"] = inst
while time.time() < end:
    if time.time() >= nxt:
        s.sendall(c.frame(json.dumps(msg))); nxt = time.time() + 0.3
    try:
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
    except socket.timeout:
        continue
    texts, buf = c.read_frames(buf)
    for t in texts:
        if t is not None:
            last = t
print(" ".join(sorted(p["u"] for p in json.loads(last)["people"])) if last else "CLOSED")
EOF
}
both() { # <pal's instance|-> <Bbottin's instance|->: both in the room at once; sets $p and $a to what each saw
    visit "$PAL" "$1" 2.5 > "$W/pal.txt" & local v1=$!
    visit "$ADMIN" "$2" 2.5 > "$W/admin.txt" & local v2=$!
    wait $v1 $v2   # the two visitors only: a bare wait would also wait for the server
    p=$(cat "$W/pal.txt"); a=$(cat "$W/admin.txt")
}

both - -
check "precondition: no instance named, pal and Bbottin in one room see each other" '[ "$p" = "Bbottin pal" ] && [ "$a" = "Bbottin pal" ]' "pal saw '$p', Bbottin saw '$a'"
both user:pal mainstream
check "different instances: pal sees only herself and Bbottin only himself" '[ "$p" = "pal" ] && [ "$a" = "Bbottin" ]' "pal saw '$p', Bbottin saw '$a'"
both mine:friends mine:friends
check "the same instance: they see each other" '[ "$p" = "Bbottin pal" ] && [ "$a" = "Bbottin pal" ]' "pal saw '$p', Bbottin saw '$a'"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
