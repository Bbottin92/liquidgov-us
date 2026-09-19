#!/usr/bin/env bash
# The harness itself. CI's first run showed why this matters: test_guest starts the daemon under
# strace, stop_daemon killed only the strace wrapper, the traced daemon lived on holding :8765, and
# the NEXT test file's "new" daemon died on bind() while its health check passed against the old one --
# so every assertion ran against a stranger's database. Two rules pinned here:
#   1. stop_daemon kills what a wrapper launched, not just the wrapper.
#   2. start_daemon refuses (loudly) when something already answers on the port.
source "$(dirname "$0")/lib.sh"
start_civic

echo "-- a wrapper that forks its child (like strace -f) leaves nothing behind"
cat > "$WORK/wrap.sh" <<'W'
#!/usr/bin/env bash
"$@" & wait $!
W
DAEMON_WRAP="bash $WORK/wrap.sh" start_daemon
get "$D/health"; assert_code "the wrapped daemon is up" 200
[ "$(pgrep -x commons_daemon | wc -l)" = "1" ] && _ok "exactly one daemon process (the wrapper's child)" || _bad "exactly one daemon process" "$(pgrep -x commons_daemon | wc -l)"
stop_daemon
sleep 0.3
curl -s -o /dev/null "$D/health" && r=answering || r=silent
assert_eq "after stop_daemon the port is silent" "silent" "$r"
assert_eq "and no daemon process survived" "0" "$(pgrep -x commons_daemon | wc -l)"

echo "-- a plain daemon, then crash_daemon (kill -9) also leaves nothing"
start_daemon
crash_daemon
sleep 0.3
curl -s -o /dev/null "$D/health" && r=answering || r=silent
assert_eq "after crash_daemon the port is silent" "silent" "$r"

echo "-- start_daemon refuses when something already answers"
start_daemon
out=$(bash -c "source '$TESTS_DIR/lib.sh'; start_daemon" 2>&1); rc=$?
assert_eq "a second start_daemon exits non-zero" "1" "$rc"
assert_contains "and says why" "$out" "already listening"
get "$D/health"; assert_code "the original daemon is untouched" 200
stop_daemon
