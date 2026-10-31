#!/usr/bin/env bash
# reqjson: request fields are found as KEYS, not as the first quoted token that spells a field name.
# The old finder matched the text "x" anywhere, so a guest whose name is x (or y, id, secret, region, ...)
# sent {"display_name":"x",...,"x":5,...} and the name was read as the key of the x coordinate: 400,
# never joins. The shipped client sends display_name LAST, which hid it -- by luck of JS key order.
#   part 1: unit cases against src/reqjson.hpp (compiled here, no daemon)
#   part 2: the same bodies through the real daemon, name FIRST
source "$(dirname "$0")/lib.sh"

echo "-- unit cases (reqjson_unit.cpp against src/reqjson.hpp)"
if ! "${CXX:-g++}" -std=c++17 -Wall -Wextra -I"$DAEMON_DIR/src" -o "$WORK/reqjson_unit" "$TESTS_DIR/reqjson_unit.cpp" 2> "$WORK/cc.log"; then
  echo "FATAL: reqjson_unit.cpp did not compile"; cat "$WORK/cc.log"; exit 1
fi
[ -s "$WORK/cc.log" ] && { echo "compiler warnings:"; cat "$WORK/cc.log"; }
unit_out="$("$WORK/reqjson_unit" 2>&1)"; unit_rc=$?
echo "$unit_out" | sed 's/^/     /'
assert_eq "no unit case failed" "0" "$unit_rc"
assert_contains "and the unit run really ran cases" "$unit_out" "reqjson_unit:"
n=$(echo "$unit_out" | sed -n 's/^reqjson_unit: \([0-9]*\) checks.*/\1/p')
[ "${n:-0}" -ge 100 ] && _ok "at least 100 checks executed ($n)" || _bad "at least 100 checks executed" "$n"

start_civic; start_daemon

echo "-- a guest named after a field joins, moves, leaves: display_name FIRST in every body"
for nm in x y id secret region facing from_region to_region item policy username display_name; do
  mint
  post "$D/presence/join" "{\"display_name\":\"$nm\",\"id\":\"$GID\",\"secret\":\"$GSEC\",\"region\":\"1,1\",\"x\":5,\"y\":6,\"facing\":2}"
  assert_code "join as '$nm'" 200
  assert_eq "  under its own name (no fallback to the id, no '(2)')" "$nm" "$(jget display_name)"
  occupants 1,1
  assert_contains "  at the coordinates sent" "$BODY" "\"display_name\":\"$nm\",\"x\":5,\"y\":6,\"facing\":2"
  post "$D/presence/move" "{\"display_name\":\"$nm\",\"id\":\"$GID\",\"secret\":\"$GSEC\",\"region\":\"1,1\",\"x\":6,\"y\":7,\"facing\":1}"
  assert_contains "  move as '$nm'" "$BODY" '"ok":true'
  occupants 1,1
  assert_contains "  and the position moved" "$BODY" "\"display_name\":\"$nm\",\"x\":6,\"y\":7,\"facing\":1"
  post "$D/presence/leave" "{\"region\":\"1,1\",\"id\":\"$GID\",\"secret\":\"$GSEC\"}"
  assert_contains "  leave" "$BODY" '"ok":true'
  occupants 1,1
  assert_not_contains "  and is gone" "$BODY" "\"display_name\":\"$nm\""
done

echo "-- rekey with the name first"
mint
post "$D/presence/join" "{\"display_name\":\"x\",\"id\":\"$GID\",\"secret\":\"$GSEC\",\"region\":\"1,1\",\"x\":5,\"y\":6,\"facing\":0}"
post "$D/presence/rekey" "{\"display_name\":\"x\",\"id\":\"$GID\",\"secret\":\"$GSEC\",\"from_region\":\"1,1\",\"to_region\":\"1,2\",\"x\":9,\"y\":9,\"facing\":3}"
assert_contains "rekey as 'x' ok" "$BODY" '"ok":true'
occupants 1,2
assert_contains "in the new region at the sent coordinates" "$BODY" '"display_name":"x","x":9,"y":9,"facing":3'
occupants 1,1
assert_not_contains "and out of the old one" "$BODY" '"display_name":"x"'
post "$D/presence/leave" "{\"region\":\"1,2\",\"id\":\"$GID\",\"secret\":\"$GSEC\"}"

echo "-- field VALUES that spell other field names"
mint
post "$D/presence/join" "{\"id\":\"secret\",\"secret\":\"$GSEC\",\"region\":\"1,1\",\"x\":5,\"y\":6,\"facing\":0}"
assert_code "id 'secret' is refused as a bad credential, not misparsed" 403
assert_contains "with the credential reason" "$BODY" '"reason":"bad_credential"'
mint
post "$D/presence/join" "{\"id\":\"$GID\",\"secret\":\"$GSEC\",\"region\":\"x\",\"x\":5,\"y\":6,\"facing\":0}"
assert_code "region named 'x' still reads x as the coordinate" 200
occupants x
assert_contains "at (5,6)" "$BODY" '"x":5,"y":6'

echo "-- a name that carries forged coordinates stays a name"
mint
post "$D/presence/join" "{\"display_name\":\"a\\\",\\\"x\\\":999,\\\"y\\\":999,\\\"z\\\":\\\"b\",\"id\":\"$GID\",\"secret\":\"$GSEC\",\"region\":\"1,1\",\"x\":7,\"y\":8,\"facing\":0}"
assert_code "join" 200
occupants 1,1
assert_contains "the real coordinates were used" "$BODY" '"x":7,"y":8,"facing":0'
assert_not_contains "the forged ones were not" "$BODY" '"x":999'

echo "-- a body that is not a JSON object is 'missing', not a crash"
mint
post "$D/presence/join" "not json at all";  assert_code "garbage body" 400
post "$D/presence/join" "";                 assert_code "empty body" 400
post "$D/presence/join" "[\"id\",\"x\"]";   assert_code "an array" 400
get "$D/health"; assert_code "the daemon is alive" 200
assert_not_contains "no error lines in the daemon log" "$(cat "$WORK/daemon.log")" "terminate called"
