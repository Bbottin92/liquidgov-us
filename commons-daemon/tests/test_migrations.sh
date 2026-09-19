#!/usr/bin/env bash
# Startup migrations on a database made by an OLDER daemon (the shape production actually has).
# The old schemas are frozen fixtures from git history; using HEAD's schema would test nothing.
source "$(dirname "$0")/lib.sh"

make_old_db() {   # fixture-file  -> $DB built from that schema + seed, with some pre-existing rows
  mkdir -p "$WORK/run/data"; rm -f "$DB" "$DB-wal" "$DB-shm"
  python3 - "$1" "$DAEMON_DIR/seed.sql" <<'PY'
import os, sqlite3, sys
c = sqlite3.connect(os.environ["DB_PATH"])
c.executescript(open(sys.argv[1]).read())
c.executescript(open(sys.argv[2]).read())
now = 1700000000
c.execute("insert into map_instances (map_id, owner_player_id, created_at) values ((select map_id from maps where slug='house'), 1, ?)", (now,))
c.execute("insert into players (lg_username, display_name, is_guest, house_instance_id, created_at) values ('olduser','olduser',0,1,?)", (now,))
c.execute("insert into players (lg_username, display_name, is_guest, created_at) values ('otheruser','otheruser',0,?)", (now,))
item = c.execute("select item_id from items order by item_id limit 1").fetchone()[0]
cols = [r[1] for r in c.execute("pragma table_info(inventory)")]
if "updated_at" in cols:
    c.execute("insert into inventory (player_id, item_id, quantity, updated_at) values (1, ?, 3, 5)", (item,))
else:
    c.execute("insert into inventory (player_id, item_id, quantity) values (1, ?, 3)", (item,))
c.commit()
PY
}
cols() { dbq "select group_concat(name) from pragma_table_info('$1')"; }

for fixture in schema.pre-visit-policy schema.pre-inventory-recency; do
  echo "-- migrating a database built from $fixture"
  make_old_db "$TESTS_DIR/fixtures/$fixture.sql"
  assert_not_contains "precondition: the old players table has no visit_policy" "$(cols players)" visit_policy
  [ "$fixture" = schema.pre-inventory-recency ] && assert_not_contains "precondition: the old inventory table has no updated_at" "$(cols inventory)" updated_at
  assert_eq "precondition: two existing players" 2 "$(dbq "select count(*) from players")"
  start_daemon
  assert_contains "players gained visit_policy" "$(cols players)" visit_policy
  assert_contains "inventory has updated_at" "$(cols inventory)" updated_at
  assert_eq "existing players are all intact" 2 "$(dbq "select count(*) from players")"
  assert_eq "every existing house is still friends-only (the DEFAULT)" 2 "$(dbq "select count(*) from players where visit_policy='friends'")"
  assert_eq "the existing inventory row is intact" 3 "$(dbq "select quantity from inventory where player_id=1")"
  assert_eq "the existing house instance is intact" 1 "$(dbq "select count(*) from map_instances where owner_player_id=1")"
  assert_eq "the database passes integrity_check" ok "$(dbq "pragma integrity_check")"
  assert_eq "catalog grew to 11 items (3 seed + 5 lab + 3 world)" 11 "$(dbq "select count(*) from items")"
  assert_eq "and 8 recipes (world pickups have none)" 8 "$(dbq "select count(*) from recipes")"
  assert_eq "no lime (pushback.md)" 0 "$(dbq "select count(*) from items where slug like '%lime%'" )"
  assert_eq "no limestone (GROK_OBJECTS_ANSWER.md D)" 0 "$(dbq "select count(*) from items where slug like '%limestone%'" )"
  assert_eq "vinegar_jar is present" 1 "$(dbq "select count(*) from items where slug='vinegar_jar'")"
  assert_eq "oak_gall_tannin is present" 1 "$(dbq "select count(*) from items where slug='oak_gall_tannin'")"
  assert_eq "lampblack_ink is present" 1 "$(dbq "select count(*) from items where slug='lampblack_ink'")"
  assert_eq "alum_crystal is present" 1 "$(dbq "select count(*) from items where slug='alum_crystal'")"
  assert_eq "gum_arabic is present" 1 "$(dbq "select count(*) from items where slug='gum_arabic'")"
  assert_eq "oak_gall is present" 1 "$(dbq "select count(*) from items where slug='oak_gall'")"
  assert_eq "seashell is present" 1 "$(dbq "select count(*) from items where slug='seashell'")"
  assert_eq "cider_apple is present" 1 "$(dbq "select count(*) from items where slug='cider_apple'")"
  get "$D/health"; assert_code "and the daemon serves" 200
  stop_daemon; start_daemon
  assert_eq "a second start is a no-op (idempotent), rows unchanged" 2 "$(dbq "select count(*) from players")"
  assert_not_contains "no error in the log across both starts" "$(cat "$WORK/daemon.log")" 'failed'
  stop_daemon
  : > "$WORK/daemon.log"
done

echo "-- a brand-new database is created at the current shape"
rm -rf "$WORK/run"; start_daemon
assert_contains "fresh DB has visit_policy" "$(cols players)" visit_policy
assert_contains "fresh DB has inventory.updated_at" "$(cols inventory)" updated_at
assert_eq "fresh DB is seeded (11 catalog items: 3 seed + 5 lab + 3 world)" 11 "$(dbq "select count(*) from items")"
assert_eq "fresh DB has no lime" 0 "$(dbq "select count(*) from items where slug like '%lime%'")"
assert_eq "fresh DB has no limestone" 0 "$(dbq "select count(*) from items where slug like '%limestone%'")"
assert_eq "fresh DB recipes stay at the 8 lab lookups" 8 "$(dbq "select count(*) from recipes")"
