#!/usr/bin/env bash
# Guest credentials and the raw HTTP layer: OS-random secrets, the 256 cap (evict, never refuse),
# bad-secret 403, and the socket timeouts (a stalled client can't wedge the single-threaded daemon).
source "$(dirname "$0")/lib.sh"
start_daemon

echo "-- mint and the ordinary guest flow"
mint; assert_code "mint answers 200" 200
assert_eq "id is g_ + 16 hex" 1 "$(echo "$GID" | grep -Ec '^g_[0-9a-f]{16}$')"
assert_eq "secret is 32 hex" 1 "$(echo "$GSEC" | grep -Ec '^[0-9a-f]{32}$')"
dups=$(for _ in $(seq 1 300); do curl -s -X POST "$D/presence/guest"; echo; done | grep -o '"secret":"[^"]*"' | sort | uniq -d | wc -l)
assert_eq "300 mints, no duplicate secret" 0 "$dups"
mint; A_ID=$GID; A_SEC=$GSEC
gjoin $A_ID $A_SEC 1,1 5 5 walker; assert_contains "guest join ok, not bound" "$BODY" '"bound":false'
gmove $A_ID $A_SEC 1,1 9 9;          assert_code "guest move with the right secret" 200
assert_contains "and it moved" "$BODY" '"ok":true'
occupants 1,1;                        assert_contains "the walker is listed at its new spot" "$BODY" '"x":9'
gmove $A_ID "not-the-secret" 1,1;     assert_code "bad secret is 403" 403
gjoin $A_ID "not-the-secret" 1,1;     assert_code "bad secret cannot re-join either" 403
post "$D/presence/join" '{"region":"1,1","x":1,"y":1}';  assert_code "no credential at all is refused (400 missing field)" 400
gleave $A_ID $A_SEC 1,1;              assert_contains "leave ok" "$BODY" '"ok":true'
occupants 1,1;                        assert_eq "region is empty after leave" "[]" "$BODY"

echo "-- registry cap (256): never refuses, evicts never-used junk first, keeps real guests"
mint; J0_ID=$GID; J0_SEC=$GSEC                       # never used: junk
mint; R_ID=$GID; R_SEC=$GSEC; gjoin $R_ID $R_SEC 1,1 3 3 real   # a real guest, verified
assert_contains "the real guest joined" "$BODY" '"ok":true'
bad=0; for _ in $(seq 1 400); do c=$(curl -s -o /dev/null -w '%{http_code}' -X POST "$D/presence/guest"); [ "$c" = 200 ] || bad=$((bad+1)); done
assert_eq "a 400-mint flood: zero non-200 answers (evict, never 503)" 0 "$bad"
gmove $R_ID $R_SEC 1,1 4 4;  assert_code "the real guest is still valid after the flood" 200
assert_contains "and still moves" "$BODY" '"ok":true'
gmove $J0_ID $J0_SEC 1,1;    assert_code "the oldest never-used junk credential was evicted (403)" 403
echo "   (all-used case: 300 guests that each really verified)"
FIRST_ID=""; FIRST_SEC=""; LAST_ID=""; LAST_SEC=""
for i in $(seq 1 300); do mint; gjoin $GID $GSEC 1,1 2 2 >/dev/null; [ -z "$FIRST_ID" ] && { FIRST_ID=$GID; FIRST_SEC=$GSEC; }; LAST_ID=$GID; LAST_SEC=$GSEC; done
gmove $FIRST_ID $FIRST_SEC 1,1; assert_code "the least recently used real guest is the one dropped (403)" 403
gmove $LAST_ID $LAST_SEC 1,1;   assert_code "the most recent is valid (its credential is accepted)" 200

echo "-- socket timeouts: a stalled client must not wedge the daemon"
get "$D/health"; assert_between "baseline /health latency is tiny" 0 0.5 "$TIME"
python3 - "$D" <<'PY' > "$WORK/timeouts.txt"
import socket, sys, threading, time, subprocess
port = 8765
def health():
    # --max-time: against a daemon WITHOUT the timeouts this would block forever; fail fast instead
    t = time.time(); subprocess.run(["curl", "-s", "--max-time", "20", "-o", "/dev/null", "http://127.0.0.1:%d/health" % port]); return time.time() - t
# (a) half a request, then silence
s = socket.create_connection(("127.0.0.1", port)); s.sendall(b"GET /health HTTP/1.1\r\nHost: x\r\n"); time.sleep(0.3)
print("half_request_wait=%.2f" % health()); s.close()
print("after_half=%.3f" % health())
# (b) a promised body that never arrives: closed, never dispatched
s = socket.create_connection(("127.0.0.1", port))
s.sendall(b"POST /presence/guest HTTP/1.1\r\nHost: x\r\nContent-Length: 200\r\n\r\n{\"id\":\"g_x\""); time.sleep(0.3)
print("stalled_body_wait=%.2f" % health())
s.settimeout(1)
try: got = len(s.recv(4096))
except Exception: got = 0
print("stalled_body_bytes_answered=%d" % got); s.close()
print("after_body=%.3f" % health())
# (c) a trickle: 1 byte / 3s must be cut off by the 10s whole-request deadline
s = socket.create_connection(("127.0.0.1", port))
def drip():
    try:
        for ch in b"GET /health HTTP/1.1\r\nHost: x\r\nX-Slow: " + b"y" * 40:
            s.sendall(bytes([ch])); time.sleep(3)
    except Exception: pass
threading.Thread(target=drip, daemon=True).start(); time.sleep(0.3)
print("trickle_wait=%.2f" % health()); s.close()
print("after_trickle=%.3f" % health())
PY
v() { grep "^$1=" "$WORK/timeouts.txt" | cut -d= -f2; }
assert_between "half a request then silence: the request behind it waits ~5s (before the fix: forever)" 3 9 "$(v half_request_wait)"
assert_between "the daemon is instantly responsive again" 0 0.5 "$(v after_half)"
assert_between "a stalled body holds it ~5s too" 3 9 "$(v stalled_body_wait)"
assert_eq "and the truncated request is closed, never answered or dispatched" 0 "$(v stalled_body_bytes_answered)"
assert_between "a 1-byte/3s trickle is bounded by the 10s deadline (worst case deadline + one 5s recv)" 3 16 "$(v trickle_wait)"
assert_between "responsive after the trickle" 0 0.5 "$(v after_trickle)"
get "$D/health"; assert_contains "daemon still healthy" "$BODY" '"ok"'
assert_eq "no error lines in the daemon log" 0 "$(grep -c -i 'failed\|abort' "$WORK/daemon.log")"

echo "-- guest secrets come from the OS (getrandom), not a seeded PRNG"
if command -v strace >/dev/null 2>&1 && strace -f -e trace=getrandom -o /dev/null true 2>/dev/null; then
  stop_daemon
  DAEMON_WRAP="strace -f -e trace=getrandom -o $WORK/strace.txt" start_daemon
  for _ in $(seq 1 40); do curl -s -o /dev/null -X POST "$D/presence/guest"; done
  n=$(grep -c getrandom "$WORK/strace.txt")
  assert_between "40 mints made at least 80 getrandom calls (an id and a secret each; a seeded PRNG makes ~1)" 80 100000 "$n"
else
  echo "  skip (strace missing or ptrace not permitted): the entropy source is only checked by the code path here"
fi
