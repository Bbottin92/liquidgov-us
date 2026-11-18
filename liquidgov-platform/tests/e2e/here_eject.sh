#!/usr/bin/env bash
# Spatial step 9 (spatial_evolution.md): an instance can carry an eject rule. The rule is the server's: LIQUIDGOV_HERE_EJECT
# = "<instance>:<x0>,<y0>,<x1>,<y1>" is a hazard region (the room's 0..1 page coordinates) in that instance. After every
# move the server puts a dot that is in that instance and inside the region back in its parent, the room's own instance,
# so the next presence snapshot shows it there and the others in the instance no longer see it.
# The one check: two people in the room /u/stranger, both claiming the instance "hazard-room", with a hazard in its
# lower right corner. Both outside it: they see each other (precondition). One walks into the hazard: the server ejects
# that dot to the parent (her own entry now names the room), she sees only herself, and the one still outside sees only
# himself. The one outside is never ejected.
# Usage: bash here_eject.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18133. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
E2E="$(cd "$(dirname "$0")" && pwd)"
W=/tmp/lg-here-eject; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18133; URL=http://127.0.0.1:$PORT
ROOM=/u/stranger
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
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_HERE_EJECT="hazard-room:0.8,0.8,1.0,1.0" LIQUIDGOV_STORE=$W/store.lgp \
    LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER $(jobs -p) 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
curl -s -o /dev/null --max-time 1 $URL/ || { echo "server never answered"; exit 1; }
for s in $ADMIN $PAL; do curl -s -o /dev/null -b "lg_session=$s" -d choice=1 -d back=/settings "$URL/here/choice"; done

# One person in the room for <seconds>, sending x, y and instanceId every 0.3 s; prints each dot of the LAST snapshot as
# <user>@<instanceId>, sorted, or REFUSED/CLOSED. Uses here_ws_client.py's frame code as it is.
visit() { # <session> <instance> <x> <y> <seconds>
python3 - "$E2E" "$PORT" "$ROOM" "$1" "$2" "$3" "$4" "$5" <<'EOF'
import base64, json, os, socket, sys, time
sys.path.insert(0, sys.argv[1])
import here_ws_client as c
port, room, session, inst, x, y, secs = sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], float(sys.argv[6]), float(sys.argv[7]), float(sys.argv[8])
s = socket.create_connection(("127.0.0.1", int(port)), timeout=5)
key = base64.b64encode(os.urandom(16)).decode()
s.sendall(("GET /here/ws?page=%s HTTP/1.1\r\nHost: 127.0.0.1:%s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nCookie: lg_session=%s\r\n\r\n" % (room, port, key, session)).encode())
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
msg = {"x": x, "y": y, "instanceId": inst}
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
print(" ".join(sorted("%s@%s" % (p["u"], p["instanceId"]) for p in json.loads(last)["people"])) if last else "CLOSED")
EOF
}
both() { # <pal x> <pal y>: both in the room at once, both claiming hazard-room; Bbottin stays at 0.5 0.5; sets $p and $a
    visit "$PAL" hazard-room "$1" "$2" 2.5 > "$W/pal.txt" & local v1=$!
    visit "$ADMIN" hazard-room 0.5 0.5 2.5 > "$W/admin.txt" & local v2=$!
    wait $v1 $v2   # the two visitors only: a bare wait would also wait for the server
    p=$(cat "$W/pal.txt"); a=$(cat "$W/admin.txt")
}

both 0.5 0.5
check "precondition: both outside the hazard in hazard-room see each other" \
    '[ "$p" = "Bbottin@hazard-room pal@hazard-room" ] && [ "$a" = "Bbottin@hazard-room pal@hazard-room" ]' "pal saw '$p', Bbottin saw '$a'"
both 0.9 0.9
check "pal walks into the hazard: the server ejects her to the parent (the room), and she sees only herself there" \
    '[ "$p" = "pal@$ROOM" ]' "pal saw '$p'"
check "the one still outside no longer sees her, is not ejected, and sees only himself in hazard-room" \
    '[ "$a" = "Bbottin@hazard-room" ]' "Bbottin saw '$a'"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
