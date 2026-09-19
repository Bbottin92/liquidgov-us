#!/usr/bin/env bash
# The orphan watch/reap: game rows of accounts civic says are GONE are deleted (so a re-registered
# username cannot inherit a house). Deleting is guarded: an explicit exists:false from civic, two
# passes in a row, cap 3 per pass, an automatic online backup first ("no backup, no reap"), one last
# look, never the online, and a SUSPECT guard. A 1s test tick makes a "pass" about a second.
source "$(dirname "$0")/lib.sh"
start_civic
start_daemon COMMONS_TEST_TICK_MS=1000
LOG="$WORK/daemon.log"; BK="$WORK/run/data/backups"
reaped() { grep -c "REAPED '$1'" "$LOG"; }
present() { dbq "select count(*) from players where lg_username='$1'"; }
for u in keepr usera userb userc userd usere userf; do su $u; setup_house $u; done

echo "-- precondition: seven players, each with a house, inventory and a wall item"
assert_eq "players" 7 "$(dbq "select count(*) from players")"
assert_eq "inventory rows" 14 "$(dbq "select count(*) from inventory")"
assert_eq "wall items" 7 "$(dbq "select count(*) from placed")"

echo "-- everyone exists: passes run, nothing is reaped, no backup is taken"
sleep 4
assert_eq "no REAPED lines" 0 "$(grep -c REAPED "$LOG")"
assert_eq "no backups taken" 0 "$( [ -d "$BK" ] && ls "$BK" | wc -l || echo 0 )"

echo "-- four accounts deleted: cap 3 => three reaped, the fourth on the next pass"
KEEP_BEFORE="$(dbq "select count(*) from inventory where player_id=(select player_id from players where lg_username='keepr')")"
for u in usera userb userc userd; do delete_account $u; done
sleep 9
for u in usera userb userc; do assert_eq "$u reaped" 1 "$(reaped $u)"; done
assert_eq "userd reaped (a later pass)" 1 "$(reaped userd)"
assert_contains "the cap deferred one" "$(cat "$LOG")" 'deferred by the cap of 3'
for u in usera userb userc userd; do assert_eq "$u: players row gone" 0 "$(present $u)"; done
assert_eq "their wall items are gone" 3 "$(dbq "select count(*) from placed")"
assert_eq "their inventory is gone" 6 "$(dbq "select count(*) from inventory")"
assert_eq "their house instances are gone" 3 "$(dbq "select count(*) from map_instances where owner_player_id is not null")"
for u in keepr usere userf; do assert_eq "living account $u untouched" 1 "$(present $u)"; done
assert_eq "keepr's inventory unchanged" "$KEEP_BEFORE" "$(dbq "select count(*) from inventory where player_id=(select player_id from players where lg_username='keepr')")"
assert_eq "integrity_check" ok "$(dbq "pragma integrity_check")"
assert_eq "no dangling rows (foreign_key_check)" 0 "$(python3 -c 'import sqlite3,os; print(len(sqlite3.connect(os.environ["DB_PATH"]).execute("pragma foreign_key_check").fetchall()))')"
nb=$(ls "$BK" | wc -l)
assert_between "one backup per reaping pass (2 passes reaped)" 2 3 "$nb"
first=$(ls "$BK" | sort | head -1)
assert_eq "the FIRST backup is a valid DB holding the pre-reap state (7 players)" "ok 7" "$(python3 - "$BK/$first" <<'PY'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1]); print(c.execute("pragma integrity_check").fetchone()[0], c.execute("select count(*) from players").fetchone()[0])
PY
)"

echo "-- the inheritance hole: a NEW owner registers a reaped name"
su usera
bjoin usera "$(house usera)" >/dev/null
get "$D/placed/$(house usera)";   assert_eq "the new owner's wall is empty" '{"item":""}' "$BODY"
get "$D/presence/inventory" "$(jar usera)"; assert_eq "and inventory is empty" '{"items":[]}' "$BODY"
bleave usera "$(house usera)" >/dev/null

echo "-- civic frozen right after a delete: 'don't know' must never reap"
delete_account usere
freeze_civic
sleep 6
assert_eq "usere's rows survive a silent civic" 1 "$(present usere)"
assert_eq "and were not reaped" 0 "$(reaped usere)"
thaw_civic
sleep 6
assert_eq "civic back: two fresh passes later it is reaped" 1 "$(reaped usere)"

echo "-- no backup, no reap"
if [ "$(id -u)" = 0 ]; then echo "  skip (running as root: chmod cannot make the backup dir unwritable)"; else
  chmod 555 "$BK"
  delete_account userf
  sleep 5
  assert_contains "the pass reports the failed backup" "$(cat "$LOG")" 'BACKUP FAILED'
  assert_eq "userf's rows survive" 1 "$(present userf)"
  chmod 755 "$BK"
  sleep 4
  assert_eq "with the backup dir writable again it is reaped" 1 "$(reaped userf)"
fi

echo "-- suspect: every known account gone counts nothing"
delete_account keepr; delete_account usera
sleep 5
assert_contains "SUSPECT is logged" "$(cat "$LOG")" 'SUSPECT'
assert_eq "keepr still has its row" 1 "$(present keepr)"
assert_eq "the new usera still has its row" 1 "$(present usera)"
stop_daemon

echo "-- kill switch: COMMONS_ORPHAN_REAP=0 is log-only"
: > "$LOG"
start_daemon COMMONS_TEST_TICK_MS=1000 COMMONS_ORPHAN_REAP=0     # the daemon must be UP before the houses are made
for u in swa swb; do su $u; setup_house $u; done
assert_eq "precondition: swa has a players row and a house" 1 "$(dbq "select count(*) from players where lg_username='swa' and house_instance_id is not null")"
assert_eq "precondition: swb too (so 'all known gone' cannot trigger)" 1 "$(present swb)"
delete_account swa
sleep 5
assert_contains "starts in LOG-ONLY" "$(cat "$LOG")" 'LOG-ONLY (kill switch)'
assert_contains "it logs would-reap" "$(cat "$LOG")" "would reap 'swa'"
assert_eq "but deletes nothing" 1 "$(present swa)"
assert_eq "no REAPED line" 0 "$(grep -c REAPED "$LOG")"
stop_daemon

echo "-- a living player who is ONLINE is skipped, and does not cause a false SUSPECT"
stop_daemon
: > "$LOG"
start_daemon COMMONS_TEST_TICK_MS=1000
for u in tha thb live; do su $u; setup_house $u; done
assert_eq "precondition: tha, thb and live all have a players row" 3 "$(dbq "select count(*) from players where lg_username in ('tha','thb','live')")"
bjoin live 1,1 5 5 >/dev/null
assert_contains "precondition: live is ONLINE (so it is skipped, not asked about)" "$(occupants 1,1; echo "$BODY")" 'u:live'
delete_account tha; delete_account thb
for _ in 1 2 3 4 5 6; do bmove live 1,1 6 6 >/dev/null; sleep 1; done
assert_eq "tha reaped" 1 "$(reaped tha)"
assert_eq "thb reaped" 1 "$(reaped thb)"
assert_eq "no false SUSPECT" 0 "$(grep -c SUSPECT "$LOG")"
assert_eq "the online player was never touched" 1 "$(present live)"
