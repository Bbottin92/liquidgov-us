#!/usr/bin/env bash
# Crash recovery: the periodic snapshot writes a bound player's position while they stand in their
# OWN house, so a kill -9 (no leave, no rekey) resumes them there. The control proves the test can tell.
source "$(dirname "$0")/lib.sh"
start_civic
su alice
H=$(house alice)

echo "-- control: default 30s tick, kill -9 long before any tick"
start_daemon
bjoin alice $H >/dev/null
bmove alice $H 3 9 2
crash_daemon
start_daemon
bjoin alice $H
assert_not_contains "nothing was ever snapshotted: no resume position" "$BODY" 'resume_x'
assert_eq "and the row has no position" "" "$(dbq "select last_x from players where lg_username='alice'")"
stop_daemon

echo "-- with a 1s tick: walk, keep moving a few seconds, kill -9 (no leave, no rekey)"
start_daemon COMMONS_TEST_TICK_MS=1000 COMMONS_ORPHAN_REAP=0
bjoin alice $H >/dev/null
for _ in 1 2 3 4; do bmove alice $H 5 4 1 >/dev/null; sleep 1; done
crash_daemon
assert_eq "the tick wrote the position (5)" 5 "$(dbq "select last_x from players where lg_username='alice'")"
start_daemon COMMONS_TEST_TICK_MS=1000 COMMONS_ORPHAN_REAP=0
bjoin alice $H
assert_contains "resumes at x" "$BODY" '"resume_x":5'
assert_contains "resumes at y" "$BODY" '"resume_y":4'
assert_contains "and facing" "$BODY" '"resume_facing":1'

echo "-- guard: standing in the shared demo house must not write the house row"
bleave alice $H >/dev/null
bjoin alice instance:demo 9 9 >/dev/null
for _ in 1 2 3 4; do bmove alice instance:demo 9 9 3 >/dev/null; sleep 1; done
crash_daemon
assert_eq "the house position is still (5), not the demo house's (9)" 5 "$(dbq "select last_x from players where lg_username='alice'")"
