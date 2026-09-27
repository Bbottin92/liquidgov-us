#!/usr/bin/env bash
# The orphan reap learns the Studio's tables too (BUILD studio 1, studio.md: "so civic account-deletion
# cannot leak them the way house rows once did"). Its own file, its own civic+daemon, two accounts never
# used anywhere else -- test_orphans.sh already has strict absolute row counts across its whole file
# (shared with the `inventory` table Studio also writes to), so growing it in place is the wrong move;
# a fresh, small, self-contained file is (see notes.md, this build).
source "$(dirname "$0")/lib.sh"
start_civic
start_daemon COMMONS_TEST_TICK_MS=1000
# Real wall clock on purpose (no COMMONS_TEST_CLOCK_FILE): the orphan pass's own timing
# (now_ms() - last_pass_ms) reads the SAME global seam, so freezing it for studio setup below would
# also freeze the orphan watch -- it would never fire during the sleep further down. Studio's own
# batch/cooldown timestamps do not matter to this test, only that the rows exist and get reaped.
LOG="$WORK/daemon.log"
reaped() { grep -c "REAPED '$1'" "$LOG"; }
present() { dbq "select count(*) from players where lg_username='$1'"; }
studio_rows() { dbq "select (select count(*) from batches where player_id=p) + (select count(*) from discoveries where player_id=p) + (select count(*) from gather_cooldowns where player_id=p) from (select player_id as p from players where lg_username='$1')"; }

su gonestudio; su keptstudio
for u in gonestudio keptstudio; do
  bgather $u north_woods_twigs >/dev/null
  bbench $u charcoal >/dev/null
done

echo "-- precondition: both accounts have a players row and studio data (a batch + a cooldown)"
assert_eq "gonestudio has a players row" 1 "$(present gonestudio)"
assert_eq "gonestudio: 1 batch + 1 cooldown" 2 "$(studio_rows gonestudio)"
assert_eq "keptstudio: same" 2 "$(studio_rows keptstudio)"
assert_eq "no house was ever entered by either (no map_instances owned)" 0 "$(dbq "select count(*) from map_instances where owner_player_id is not null")"

echo "-- gonestudio's account is deleted; two passes later, the reap takes their studio rows"
delete_account gonestudio
sleep 9   # 2 consecutive "gone" passes (1s tick) + the backup + one more look, same margin test_orphans.sh uses
assert_eq "gonestudio reaped" 1 "$(reaped gonestudio)"
assert_eq "gonestudio's players row is gone" 0 "$(present gonestudio)"
assert_eq "no batch or cooldown anywhere points at a player_id with no players row (the ordering: studio's reap ran BEFORE house deleted the players row, or this would be nonzero)" \
  0 "$(dbq "select (select count(*) from batches b where not exists (select 1 from players p where p.player_id=b.player_id)) + (select count(*) from gather_cooldowns g where not exists (select 1 from players p where p.player_id=g.player_id))")"
assert_eq "keptstudio's studio rows are untouched" 2 "$(studio_rows keptstudio)"
assert_eq "keptstudio still has a players row" 1 "$(present keptstudio)"
assert_eq "integrity_check" ok "$(dbq "pragma integrity_check")"

stop_daemon
echo "== test_studio_orphans: done"
