#!/usr/bin/env bash
# A client that connects and sends nothing must not hold up everyone else (2026-11-14, wip/77).
# The site answers ordinary requests on one thread. It used to accept a connection and then wait up to 30 s for its
# request, so one silent connection stalled every request behind it. Chrome makes such connections by itself (a spare
# "preconnect" socket per page load); when the sandboxed /styles/<id>/preview page could not use its spare socket,
# the page's stylesheet waited behind it and the browser check "VERDICT stylespreview-ci" failed with
# "still loading after 20 s" (platform runs 43507933879 and 43513526202, attempt 1 of each).
# Cases: a page is answered quickly while 1 and while 5 silent connections are open; a silent connection that sends
# its request late is still answered; a connection that closes without sending is harmless; a connection that
# stays silent is closed by the server after about 30 s (so they cannot pile up).
# Usage: bash silent_connection.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18131. Never touches the live site.
set -u
BIN=$1; SEED=$2
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-silent-connection; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18131; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
(cd "$HERE" && exec env LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
curl -s -o /dev/null --max-time 1 $URL/ || { echo "server never answered"; exit 1; }

# One python client holds the silent connections and times a plain GET made while they are open.
# Each line it prints is "<case> <value>"; the checks below read them.
python3 - "$PORT" > "$W/client.txt" 2> "$W/client.err" <<'EOF'
import socket, sys, time
port = int(sys.argv[1])

def connect():
    return socket.create_connection(("127.0.0.1", port), timeout=5)

def get(sock=None, limit=5.0):
    """(status line, seconds) of GET /login; ("", limit) when no answer came in time."""
    t0 = time.time()
    s = sock or connect()
    s.settimeout(limit)
    try:
        s.sendall(b"GET /login HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
        data = b""
        while b"\r\n" not in data:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
        return data.split(b"\r\n", 1)[0].decode("latin-1"), time.time() - t0
    except OSError:
        return "", time.time() - t0
    finally:
        s.close()

status, secs = get()
print("alone_status", status.replace(" ", "_")); print("alone_secs %.2f" % secs)

first = connect()            # connected, sends nothing
time.sleep(0.3)              # the server has accepted it by now
opened = time.time()
status, secs = get()
print("one_silent_status", status.replace(" ", "_")); print("one_silent_secs %.2f" % secs)

more = [connect() for _ in range(4)]
time.sleep(0.3)
status, secs = get()
print("five_silent_status", status.replace(" ", "_")); print("five_silent_secs %.2f" % secs)

status, secs = get(more.pop())   # a silent connection that speaks late is still served
print("late_status", status.replace(" ", "_"))

for s in more[:2]:
    s.close()                # connected and left without a word
time.sleep(0.3)
status, secs = get()
print("after_close_status", status.replace(" ", "_")); print("after_close_secs %.2f" % secs)

# the first connection never sends: the server closes it (recv returns b"") about 30 s after it connected
first.settimeout(40)
try:
    gone = first.recv(16) == b""
except OSError:
    gone = False
print("silent_closed", "yes" if gone else "no"); print("silent_closed_after %.1f" % (time.time() - opened))
status, secs = get()
print("end_status", status.replace(" ", "_")); print("end_secs %.2f" % secs)
EOF
val() { awk -v k="$1" '$1==k{print $2}' "$W/client.txt"; }
quick() { awk -v s="$(val "$1")" 'BEGIN{exit !(s != "" && s + 0 < 3.0)}'; }

check "precondition: the client ran to its end" 'grep -q "^end_secs " "$W/client.txt"' "$(tail -3 "$W/client.err")"
check "precondition: with no silent connection /login answers 200" '[ "$(val alone_status)" = "HTTP/1.1_200_OK" ]' "$(val alone_status)"
check "one silent connection open: /login still answers 200" '[ "$(val one_silent_status)" = "HTTP/1.1_200_OK" ]' "status '$(val one_silent_status)' after $(val one_silent_secs) s"
check "one silent connection open: the answer comes in under 3 s" 'quick one_silent_secs' "$(val one_silent_secs) s"
check "five silent connections open: /login still answers 200" '[ "$(val five_silent_status)" = "HTTP/1.1_200_OK" ]' "status '$(val five_silent_status)' after $(val five_silent_secs) s"
check "five silent connections open: the answer comes in under 3 s" 'quick five_silent_secs' "$(val five_silent_secs) s"
check "a silent connection that sends its request late is answered 200" '[ "$(val late_status)" = "HTTP/1.1_200_OK" ]' "$(val late_status)"
check "connections closed without a request: /login answers 200 in under 3 s" '[ "$(val after_close_status)" = "HTTP/1.1_200_OK" ] && quick after_close_secs' "status '$(val after_close_status)' after $(val after_close_secs) s"
check "a connection that stays silent is closed by the server" '[ "$(val silent_closed)" = "yes" ]' "$(val silent_closed)"
check "it is closed after about 30 s (25 to 36)" 'awk -v s="$(val silent_closed_after)" "BEGIN{exit !(s + 0 >= 25 && s + 0 <= 36)}"' "$(val silent_closed_after) s"
check "afterwards /login answers 200 in under 3 s" '[ "$(val end_status)" = "HTTP/1.1_200_OK" ] && quick end_secs' "status '$(val end_status)' after $(val end_secs) s"
check "the server is still running" 'kill -0 $SERVER 2>/dev/null' "server gone"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
