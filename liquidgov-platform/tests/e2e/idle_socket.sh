#!/usr/bin/env bash
# An idle connection must not stall the site (2026-11-13). The accept loop used to read each request on its one
# thread with a 30 s receive timeout, so a socket that sent nothing (a browser preconnect, a never-reused keep-alive
# socket) held every other request behind it for up to 30 s: /assets/site.css measured 30.7 s behind one idle socket.
# Each connection now reads on its own thread; handle() stays one-at-a-time. Over kMaxConnections (64) idle sockets the
# server answers 503 at once instead of queueing, and serves again once they close.
# Usage: bash idle_socket.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18131. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-idle-socket; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18131; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }

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

# Holds N idle sockets open, fetches the stylesheet, prints "<code> <seconds>", then lets the sockets go.
# A slow CI runner gets 5 s where the old server took 30 s; the bar is "never waits for the idle socket's timeout".
# curl gives up at 8 s so the must-fail run on an old build takes under a minute, not 40 s per probe.
probe() {
    python3 - "$1" "$URL" <<'EOF'
import socket, subprocess, sys, time
n, url = int(sys.argv[1]), sys.argv[2]
host, port = url.split("//")[1].split(":")
socks = [socket.create_connection((host, int(port))) for _ in range(n)]
time.sleep(0.3)
t = time.time()
r = subprocess.run(["curl", "-s", "-o", "/dev/null", "--max-time", "8", "-w", "%{http_code}", url + "/assets/site.css"],
                   capture_output=True, text=True)
print(r.stdout.strip() or "000", "%.3f" % (time.time() - t))
for s in socks: s.close()
EOF
}
fast() { python3 -c 'import sys; sys.exit(0 if float(sys.argv[1]) < 5 else 1)' "$1"; }

read -r code secs < <(probe 1)
if [ "$code" = 200 ] && fast "$secs"; then ok "stylesheet answers behind 1 idle socket (${secs}s)"; else bad "stylesheet behind 1 idle socket: $code in ${secs}s"; fi

read -r code secs < <(probe 60)
if [ "$code" = 200 ] && fast "$secs"; then ok "stylesheet answers behind 60 idle sockets (${secs}s)"; else bad "stylesheet behind 60 idle sockets: $code in ${secs}s"; fi

read -r code secs < <(probe 100)
if [ "$code" = 503 ] && fast "$secs"; then ok "100 idle sockets (over the 64 cap): 503 at once (${secs}s), not a queue"; else bad "over the cap: $code in ${secs}s (want a fast 503)"; fi

sleep 0.5
read -r code secs < <(probe 0)
if [ "$code" = 200 ] && fast "$secs"; then ok "serves again once the idle sockets close (${secs}s)"; else bad "after the idle sockets close: $code in ${secs}s"; fi

# A real request behind the idle ones still works end to end: a signed-in page, not just a static file.
PAL=$(awk '$1=="pal"{print $2}' "$W/sessions.txt")
code=$(python3 - "$URL" "$PAL" <<'EOF'
import socket, subprocess, sys, time
url, sid = sys.argv[1], sys.argv[2]
host, port = url.split("//")[1].split(":")
socks = [socket.create_connection((host, int(port))) for _ in range(3)]
time.sleep(0.3)
r = subprocess.run(["curl", "-s", "-o", "/dev/null", "--max-time", "10", "-w", "%{http_code}", "-b", "lg_session=" + sid, url + "/me"],
                   capture_output=True, text=True)
print(r.stdout.strip() or "000")
for s in socks: s.close()
EOF
)
if [ "$code" = 200 ]; then ok "/me (signed in) answers behind 3 idle sockets"; else bad "/me behind idle sockets: $code"; fi

stop
echo "passes=$passes fails=$fails"
[ "$fails" = 0 ]
