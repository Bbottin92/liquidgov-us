#!/usr/bin/env bash
# Shared harness for the commons-daemon regression tests. Source it from a test_*.sh.
#
#   CIVIC_MODE=stub  (default)  civic_stub.py on :8080 -- what CI uses (civic can't be built there)
#   CIVIC_MODE=real             the real liquidgov_http binary; the SAME tests must pass on both,
#                               which is how the stub is kept honest (see civic_stub.py)
#   COMMONS_DAEMON_BIN          the daemon binary (default ../build/commons_daemon)
#
# Ports are fixed (the daemon hardcodes civic at :8080), so tests run one at a time.
set -u

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAEMON_DIR="$(cd "$TESTS_DIR/.." && pwd)"
REPO="$(cd "$DAEMON_DIR/.." && pwd)"
BIN="${COMMONS_DAEMON_BIN:-$DAEMON_DIR/build/commons_daemon}"
CIVIC_MODE="${CIVIC_MODE:-stub}"
CIVIC_BIN="${CIVIC_BIN:-$REPO/liquidgov-platform/build/liquidgov_http}"
D=http://127.0.0.1:8765
V=http://127.0.0.1:8080
TEST_NAME="$(basename "${0:-test}" .sh)"
WORK="$(mktemp -d /tmp/commons-test.XXXXXX)"
DB="$WORK/run/data/commons.sqlite"
CIVIC_PID=""; DAEMON_PID=""
PASS=0; FAIL=0
BODY=""; CODE=""; TIME=""

# ------------------------------------------------------------------ lifecycle
_on_exit() {
  local rc=$?
  [ -n "$DAEMON_PID" ] && kill -CONT "$DAEMON_PID" 2>/dev/null
  [ -n "$CIVIC_PID" ] && kill -CONT "$CIVIC_PID" 2>/dev/null
  _kill_daemon TERM
  [ -n "$CIVIC_PID" ] && kill "$CIVIC_PID" 2>/dev/null
  sleep 0.3
  chmod -R u+w "$WORK" 2>/dev/null
  if [ "$FAIL" -ne 0 ] && [ -f "$WORK/daemon.log" ]; then echo "-- daemon log --"; tail -30 "$WORK/daemon.log"; fi
  rm -rf "$WORK"
  echo "== $TEST_NAME: $PASS passed, $FAIL failed"
  if [ "$rc" -ne 0 ]; then exit "$rc"; fi   # a FATAL exit keeps its own status
  [ "$FAIL" -eq 0 ] || exit 1
  exit 0
}
trap _on_exit EXIT
trap 'exit 143' TERM       # so a `timeout` kill still runs _on_exit and cleans up the processes
trap 'exit 130' INT

start_civic() {
  if [ "$CIVIC_MODE" = "real" ]; then
    mkdir -p "$WORK/blobs"
    ( cd "$(dirname "$CIVIC_BIN")" && LIQUIDGOV_STORE="$WORK/store.lgp" LIQUIDGOV_BLOBS="$WORK/blobs" \
        exec "$CIVIC_BIN" ) > "$WORK/civic.log" 2>&1 &
  else
    python3 "$TESTS_DIR/civic_stub.py" --port 8080 > "$WORK/civic.log" 2>&1 &
  fi
  CIVIC_PID=$!
  for _ in $(seq 1 50); do
    curl -s -o /dev/null "$V/internal/session-user" 2>/dev/null && return 0
    sleep 0.2
  done
  echo "FATAL: civic ($CIVIC_MODE) did not come up"; cat "$WORK/civic.log"; exit 1
}
stop_civic() { [ -n "$CIVIC_PID" ] && { kill "$CIVIC_PID" 2>/dev/null; wait "$CIVIC_PID" 2>/dev/null; }; CIVIC_PID=""; }

# start_daemon [VAR=value ...]  -- extra environment for the daemon (e.g. COMMONS_TEST_TICK_MS=1000)
start_daemon() {
  # a daemon already answering means a leftover (a previous test, or a wrapper's orphaned child):
  # our new one would die on bind() while the health check below happily passed against the old one
  if curl -s -o /dev/null "$D/health" 2>/dev/null; then
    echo "FATAL: something is already listening on $D before start_daemon (a leftover daemon)"; exit 1
  fi
  mkdir -p "$WORK/run/data"
  cp "$DAEMON_DIR/schema.sql" "$DAEMON_DIR/seed.sql" "$DAEMON_DIR/catalog.sql" "$WORK/run/"
  # DAEMON_WRAP (optional, e.g. "strace -f -e trace=getrandom -o file") runs the daemon under a tool
  ( cd "$WORK/run" && exec env "$@" ${DAEMON_WRAP:-} "$BIN" data/commons.sqlite ) >> "$WORK/daemon.log" 2>&1 &
  DAEMON_PID=$!
  for _ in $(seq 1 50); do
    if ! kill -0 "$DAEMON_PID" 2>/dev/null; then echo "FATAL: daemon exited during start"; cat "$WORK/daemon.log"; exit 1; fi
    curl -s -o /dev/null "$D/health" 2>/dev/null && return 0
    sleep 0.2
  done
  echo "FATAL: daemon did not come up"; cat "$WORK/daemon.log"; exit 1
}
# Kills the daemon AND anything it launched: under DAEMON_WRAP (strace) DAEMON_PID is the wrapper, and
# killing only the wrapper leaves the traced daemon alive, still holding the port.
_kill_daemon() { # $1 = signal
  [ -n "$DAEMON_PID" ] || return 0
  pkill -"$1" -P "$DAEMON_PID" 2>/dev/null
  kill -"$1" "$DAEMON_PID" 2>/dev/null
}
stop_daemon() {
  [ -n "$DAEMON_PID" ] || return 0
  _kill_daemon TERM; wait "$DAEMON_PID" 2>/dev/null; DAEMON_PID=""
  for _ in $(seq 1 50); do curl -s -o /dev/null "$D/health" 2>/dev/null || return 0; sleep 0.1; done
  echo "FATAL: a daemon is still answering on $D after stop_daemon"; exit 1
}
crash_daemon() { [ -n "$DAEMON_PID" ] && { _kill_daemon 9; wait "$DAEMON_PID" 2>/dev/null; }; DAEMON_PID=""; }
freeze_civic() { kill -STOP "$CIVIC_PID"; }
thaw_civic() { kill -CONT "$CIVIC_PID"; sleep 0.3; }

# ------------------------------------------------------------------ assertions
_ok() { PASS=$((PASS+1)); echo "  ok   $1"; }
_bad() { FAIL=$((FAIL+1)); echo "  FAIL $1"; [ -n "${2:-}" ] && echo "       $2"; }
assert_eq() { if [ "$2" = "$3" ]; then _ok "$1"; else _bad "$1" "expected: $2 | got: $3"; fi; }
assert_contains() { case "$2" in *"$3"*) _ok "$1";; *) _bad "$1" "expected to contain: $3 | got: $2";; esac; }
assert_not_contains() { case "$2" in *"$3"*) _bad "$1" "must NOT contain: $3 | got: $2";; *) _ok "$1";; esac; }
# assert_between NAME LOW HIGH VALUE   (numbers, seconds etc.)
assert_between() { if awk -v v="$4" -v lo="$2" -v hi="$3" 'BEGIN{exit !(v>=lo && v<=hi)}'; then _ok "$1 ($4)"; else _bad "$1" "expected $2..$3, got $4"; fi; }
assert_code() { assert_eq "$1" "$2" "$CODE"; }

# ------------------------------------------------------------------ http
# post URL DATA [JAR] / get URL [JAR]  -> BODY, CODE, TIME
post() { local jar=(); [ -n "${3:-}" ] && jar=(-b "$3"); _run "${jar[@]}" -X POST --data-binary "$2" "$1"; }
get() { local jar=(); [ -n "${2:-}" ] && jar=(-b "$2"); _run "${jar[@]}" "$1"; }
_run() { local out; out=$(curl -s -o "$WORK/.body" -w '%{http_code} %{time_total}' "$@"); CODE="${out% *}"; TIME="${out#* }"; BODY="$(cat "$WORK/.body")"; }
jget() { python3 -c 'import json,sys; d=json.loads(sys.stdin.read()); v=d.get(sys.argv[1]); print("" if v is None else v)' "$1" <<<"$BODY"; }
export DB_PATH="$DB"
# dbq "SQL" -> the first column of the first row ("" if none), read from the test daemon's database
dbq() { python3 -c 'import sqlite3,sys,os; c=sqlite3.connect(os.environ["DB_PATH"]); r=c.execute(sys.argv[1]).fetchone(); print("" if r is None or r[0] is None else r[0])' "$1"; }
now() { python3 -c 'import time; print(time.time())'; }

# ------------------------------------------------------------------ accounts and rooms
_phone=0
su() { # signup NAME -> $WORK/NAME.jar
  _phone=$((_phone+1))
  curl -s -c "$WORK/$1.jar" -o /dev/null -X POST --data-urlencode "username=$1" --data-urlencode "password=Testpass123" \
    --data-urlencode "email=$1@example.com" --data-urlencode "phone=5550100$(printf %03d $_phone)" \
    --data-urlencode "display_name=$1" "$V/signup"
  grep -q lg_session "$WORK/$1.jar" || { echo "FATAL: signup failed for $1"; exit 1; }
}
jar() { echo "$WORK/$1.jar"; }
befriend() { curl -s -b "$(jar $1)" -o /dev/null -X POST --data-urlencode "who=$2" "$V/friends"
             curl -s -b "$(jar $2)" -o /dev/null -X POST --data-urlencode "who=$1" "$V/friends/accept"; }
block() { curl -s -b "$(jar $1)" -o /dev/null -X POST --data-urlencode "who=$2" "$V/friends/block"; }
unfriend() { curl -s -b "$(jar $1)" -o /dev/null -X POST --data-urlencode "who=$2" "$V/friends/unfriend"; }
delete_account() { curl -s -b "$(jar $1)" -o /dev/null -X POST --data-urlencode "confirm=$1" "$V/settings/delete"; }

# bound (cookie) requests ignore id/secret; the daemon derives the id from the session.
bjoin() { post "$D/presence/join" "{\"id\":\"g_x\",\"secret\":\"w\",\"region\":\"$2\",\"x\":${3:-7},\"y\":${4:-14},\"facing\":0}" "$(jar $1)"; }
bmove() { post "$D/presence/move" "{\"id\":\"g_x\",\"secret\":\"w\",\"region\":\"$2\",\"x\":${3:-3},\"y\":${4:-3},\"facing\":${5:-1}}" "$(jar $1)"; }
bleave() { post "$D/presence/leave" "{\"id\":\"g_x\",\"secret\":\"w\",\"region\":\"$2\"}" "$(jar $1)"; }
bpickup() { post "$D/presence/pickup" "{\"item\":\"$2\"}" "$(jar $1)"; }
bplace() { post "$D/placed/$2" "{\"id\":\"g_x\",\"secret\":\"w\",\"item\":\"$3\"}" "$(jar $1)"; }
bpolicy() { post "$D/presence/house-policy" "{\"policy\":\"$2\"}" "$(jar $1)"; }
blots() { get "$D/presence/lots" "$(jar $1)"; }
occupants() { get "$D/presence/$1"; }
house() { echo "instance:house:$1"; }
# a normal owner setup: enter own house, two lookups, a wall item, leave
setup_house() { bjoin $1 "$(house $1)" >/dev/null; bpickup $1 chalk_block; bpickup $1 iron_gall_ink_page
                bplace $1 "$(house $1)" chalk_block; bleave $1 "$(house $1)"; }
# guests
GID=""; GSEC=""
mint() { post "$D/presence/guest" ""; GID="$(jget id)"; GSEC="$(jget secret)"; }
gjoin() { post "$D/presence/join" "{\"id\":\"$1\",\"secret\":\"$2\",\"region\":\"${3:-1,1}\",\"x\":${4:-5},\"y\":${5:-5},\"facing\":0,\"display_name\":\"${6:-guest}\"}"; }
gmove() { post "$D/presence/move" "{\"id\":\"$1\",\"secret\":\"$2\",\"region\":\"${3:-1,1}\",\"x\":${4:-6},\"y\":${5:-6},\"facing\":1}"; }
gleave() { post "$D/presence/leave" "{\"id\":\"$1\",\"secret\":\"$2\",\"region\":\"${3:-1,1}\"}"; }
