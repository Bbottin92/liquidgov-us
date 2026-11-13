#!/usr/bin/env bash
# Live connections are capped (Brandon 2026-11-13: "build the websocket caps"). Every WebSocket (/community/ws,
# /here/ws, a group's /c/<slug>/jukebox/ws) holds a thread and a socket for as long as it is open; before this, one
# address could open them without limit. Now each upgrade takes a slot from core/ws_limits: at most
# LIQUIDGOV_WS_PER_IP per client address and LIQUIDGOV_WS_MAX in all (defaults 20 and 200); over either it answers
# 503 and closes; a session that ends gives its slot back. The client address is the LAST X-Forwarded-For entry (the
# one nginx appends) when the peer is loopback, so a client cannot dodge the cap by sending its own header.
# Here: per-IP 3, total 5. Usage: bash ws_caps.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18130. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-ws-caps; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18130; URL=http://127.0.0.1:$PORT
if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
PAL=$(awk '$1=="pal"{print $2}' "$W/sessions.txt")
[ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
    LIQUIDGOV_WS_PER_IP=3 LIQUIDGOV_WS_MAX=5 "$BIN") >> "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
curl -s -o /dev/null -b "lg_session=$PAL" -d choice=1 -d back=/settings $URL/here/choice
curl -s -o /dev/null -b "lg_session=$PAL" -d slug=capgroup -d name=Caps -d category=10 -d subcategory=test $URL/communities

python3 - "$PORT" "$PAL" "$URL" <<'PY'
import base64, os, socket, sys, time, urllib.request
port, sid, url = int(sys.argv[1]), sys.argv[2], sys.argv[3]
fails = passes = 0
def check(ok, what, got):
    global fails, passes
    print(("PASS " if ok else "FAIL ") + what + ("" if ok else "  (got %r)" % (got,)))
    if ok: passes += 1
    else: fails += 1
def up(path, xff):
    """(status code or '' , socket) for one upgrade attempt."""
    s = socket.create_connection(("127.0.0.1", port), timeout=3)
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall(("GET %s HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
               "Sec-WebSocket-Key: %s\r\nCookie: lg_session=%s\r\nX-Forwarded-For: %s\r\n\r\n" % (path, key, sid, xff)).encode())
    buf = b""
    try:
        while b"\r\n" not in buf:
            d = s.recv(4096)
            if not d: break
            buf += d
    except socket.timeout:
        pass
    parts = buf.split(b" ")
    return (parts[1].decode() if len(parts) > 1 else ""), s
def until_101(path, xff, secs=6):
    end = time.time() + secs
    while True:
        st, s = up(path, xff)
        if st == "101" or time.time() > end: return st, s
        s.close(); time.sleep(0.3)
CW, HW, JW = "/community/ws", "/here/ws?page=/u/stranger", "/c/capgroup/jukebox/ws"
held = []
for i in range(3):
    st, s = up(CW, "1.1.1.1"); held.append(s)
    check(st == "101", "address A, connection %d of 3 (the per-address cap) upgrades" % (i + 1), st)
st, s = up(CW, "1.1.1.1"); s.close()
check(st == "503", "address A, a 4th connection is refused with 503", st)
st, s = up(CW, "9.9.9.9, 1.1.1.1"); s.close()
check(st == "503", "address A with a made-up first X-Forwarded-For entry is still A (refused)", st)
st, s = up(CW, "1.1.1.1, 2.2.2.2"); held.append(s)
check(st == "101", "control: another address (B, the last entry) upgrades while A is full", st)
st, here_b = up(HW, "2.2.2.2")
check(st == "101", "address B joins /here/ws (5 open in all: the total cap)", st)
st, s = up(CW, "3.3.3.3"); s.close()
check(st == "503", "address C, a new address, is refused when the total is full (/community/ws)", st)
st, s = up(HW, "3.3.3.3"); s.close()
check(st == "503", "...and on /here/ws", st)
st, s = up(JW, "3.3.3.3"); s.close()
check(st == "503", "...and on a group's jukebox socket", st)
try:
    code = urllib.request.urlopen(url + "/", timeout=5).status
except Exception as e:
    code = str(e)
check(code == 200, "ordinary pages still load while the sockets are full", code)
held[0].close()
st, c1 = until_101(JW, "3.3.3.3")
check(st == "101", "closing one of A's connections frees its slot: C joins the group jukebox", st)
here_b.close()
st, c2 = until_101(HW, "3.3.3.3")
check(st == "101", "leaving /here/ws frees its slot too: C joins /here/ws", st)
st, s = up(CW, "4.4.4.4"); s.close()
check(st == "503", "and the total is full again (5 open)", st)
for s in held + [c1, c2]: s.close()
print("ws_caps: %d passed, %d failed" % (passes, fails))
sys.exit(1 if fails else 0)
PY
rc=$?
grep -q "ws cap" "$W/server.log" && echo "PASS the server logs its refusals" || { [ $rc -ne 0 ] || rc=1; echo "FAIL the server logs its refusals"; }
exit $rc
