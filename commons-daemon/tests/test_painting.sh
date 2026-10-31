#!/usr/bin/env bash
# BUILD studio 2: the portrait. Cost never refunds, a locked index is rejected,
# a visit read is not the public /placed route, open-policy strangers stay out.
source "$(dirname "$0")/lib.sh"
start_civic
start_daemon COMMONS_TEST_CLOCK_FILE="$CLOCK_FILE"
T0=1700000000000
set_clock "$T0"
for u in alice bob carol; do su $u; done

blank() { python3 -c 'print("[" + ",".join(["0"]*1024) + "]")'; }
paint() { # paint START COUNT VALUE  (the rest stay 0)
  python3 -c 'import sys; s,n,v=map(int,sys.argv[1:]); px=["0"]*1024
for i in range(s,s+n): px[i]=str(v)
print("["+",".join(px)+"]")' "$1" "$2" "$3"
}
bpaint() { post "$D/presence/painting/portrait" "{\"pixels\":$2}" "$(jar $1)"; }
bgetp() { get "$D/presence/painting/portrait" "$(jar $1)"; }
bsee() { get "$D/presence/painting/$2/portrait" "$(jar $1)"; }
qty() { dbq "select coalesce((select quantity from inventory inv join items i on i.item_id=inv.item_id join players p on p.player_id=inv.player_id where p.lg_username='$1' and i.slug='$2'),0)"; }

echo "-- unbound"
mint
get "$D/presence/painting/portrait"; assert_code "guest GET own portrait is 403" 403
post "$D/presence/painting/portrait" "{\"pixels\":$(blank)}"; assert_code "guest POST portrait is 403" 403
get "$D/presence/painting/alice/portrait"; assert_code "guest GET of someone else's portrait is 403" 403

echo "-- the stub is once, on first touch, and a stranger cannot read it"
# Carol is the stranger for the whole file. Bob is not read until after the
# friendship exists: the daemon's friends cache is 15s of wall clock, and a
# "not friends" answer would stick and hide the accept.
bsee carol alice; assert_code "carol is not a friend: 403" 403
assert_contains "not_welcome" "$BODY" not_welcome
assert_eq "reading alice did not create her stub" 0 "$(qty alice charcoal_black)"
bgetp alice; assert_code "alice's first GET: 200" 200
assert_contains "32 wide" "$BODY" '"w":32'
assert_eq "stub is 2 units of black (400 pixels)" 2 "$(qty alice charcoal_black)"
assert_eq "the three greys are not in the stub" 0 "$(qty alice charcoal_grey_1)"
bgetp alice; assert_code "second GET still 200" 200
assert_eq "the stub was not granted again" 2 "$(qty alice charcoal_black)"
befriend alice bob
bsee bob alice; assert_code "a friend can read it" 200
assert_contains "the friend gets pixels, not alice's stock" "$BODY" '"pixels":'
assert_not_contains "the friend does not get the palette (that is her stock)" "$BODY" palette

echo "-- wrong length, then a locked index, neither of which spends charcoal"
set_clock $((T0 + 1000))
bpaint alice "[0,0,0]"; assert_code "wrong length: 400" 400
assert_contains "bad_length" "$BODY" bad_length
assert_eq "a bad body did not spend the stub" 2 "$(qty alice charcoal_black)"
set_clock $((T0 + 7000))
bpaint alice "$(paint 0 1 5)"; assert_code "index 5 is not charcoal: 400" 400
assert_contains "locked_index" "$BODY" locked_index
assert_eq "a locked index did not spend charcoal" 2 "$(qty alice charcoal_black)"
assert_eq "and the stored portrait is still blank" "00" "$(dbq "select hex(substr(pixels,1,1)) from paintings pa join players p on p.player_id=pa.player_id where p.lg_username='alice'")"

echo "-- 200 new black pixels cost 1 unit; the same picture again costs nothing"
set_clock $((T0 + 13000))
bpaint alice "$(paint 0 200 1)"; assert_code "200 black pixels: 200" 200
assert_eq "2 units became 1" 1 "$(qty alice charcoal_black)"
set_clock $((T0 + 19000))
bpaint alice "$(paint 0 200 1)"; assert_code "the same pixels again: 200" 200
assert_eq "repainting the same colour costs nothing" 1 "$(qty alice charcoal_black)"

echo "-- moving those 200 pixels costs again (net count would have been free)"
set_clock $((T0 + 25000))
bpaint alice "$(paint 200 200 1)"; assert_code "the other 200 pixels: 200" 200
assert_eq "that spent the last unit" 0 "$(qty alice charcoal_black)"

echo "-- erasing refunds nothing, so painting them back is refused"
set_clock $((T0 + 31000))
bpaint alice "$(blank)"; assert_code "erase to blank: 200" 200
assert_eq "erasing did not give the unit back" 0 "$(qty alice charcoal_black)"
set_clock $((T0 + 37000))
bpaint alice "$(paint 0 200 1)"; assert_code "painting again with nothing left: 409" 409
assert_contains "insufficient" "$BODY" insufficient
assert_eq "still nothing in stock" 0 "$(qty alice charcoal_black)"

echo "-- a save that needs black AND a grey she does not hold rolls the black back"
python3 -c 'import os,sqlite3; c=sqlite3.connect(os.environ["DB_PATH"]); c.execute("update inventory set quantity=2 where item_id=(select item_id from items where slug=\"charcoal_black\") and player_id=(select player_id from players where lg_username=\"alice\")"); c.commit()'
assert_eq "restocked to 2 for the rollback case" 2 "$(qty alice charcoal_black)"
MIX="$(python3 -c 'px=["0"]*1024
for i in range(200): px[i]="1"
for i in range(200,400): px[i]="2"
print("["+",".join(px)+"]")')"
set_clock $((T0 + 43000))
bpaint alice "$MIX"; assert_code "black plus a grey she does not hold: 409" 409
assert_contains "insufficient" "$BODY" insufficient
assert_eq "the black deduction rolled back with the grey" 2 "$(qty alice charcoal_black)"

echo "-- too fast"
set_clock $((T0 + 49000))
bpaint alice "$(paint 0 1 1)"; assert_code "one pixel, she can afford it: 200" 200
assert_eq "one new pixel costs a whole unit (ceil, not a free remainder)" 1 "$(qty alice charcoal_black)"
bpaint alice "$(blank)"; assert_code "a second save inside 5s: 429" 429
assert_contains "too_fast" "$BODY" too_fast

echo "-- open lets a friend see it and not a stranger; locked lets nobody else"
bpolicy alice open; assert_code "policy open" 200
bsee carol alice; assert_code "an open-policy stranger is still refused" 403
bsee bob alice; assert_code "her friend still can, under open" 200
bpolicy alice locked; assert_code "policy locked" 200
bsee bob alice; assert_code "a friend is refused once it is locked" 403
bpolicy alice friends >/dev/null
bsee bob alice; assert_code "friends again" 200

echo "-- a client-sent timestamp is not a column the route reads"
# updated_ms in the body must not stick. The row's updated_ms is the server clock.
set_clock $((T0 + 60000))
post "$D/presence/painting/portrait" "{\"updated_ms\":1,\"pixels\":$(blank)}" "$(jar alice)"
assert_code "body with a fake timestamp still saves" 200
assert_eq "stored updated_ms is the server clock, not the client's 1" "$((T0 + 60000))" "$(dbq "select updated_ms from paintings where player_id=(select player_id from players where lg_username='alice')")"

echo "-- recolouring the same pixels costs the new colour only, and does not refund the old"
python3 -c 'import os,sqlite3; c=sqlite3.connect(os.environ["DB_PATH"]); c.execute("insert into inventory (player_id, item_id, quantity, updated_at) select player_id, (select item_id from items where slug=\"charcoal_grey_1\"), 1, 0 from players where lg_username=\"alice\""); c.commit()'
assert_eq "granted 1 grey for the recolour" 1 "$(qty alice charcoal_grey_1)"
set_clock $((T0 + 66000))
bpaint alice "$(paint 0 200 1)"; assert_code "200 black, her last unit: 200" 200
assert_eq "black is now 0" 0 "$(qty alice charcoal_black)"
set_clock $((T0 + 72000))
bpaint alice "$(paint 0 200 2)"; assert_code "those same 200 pixels, now grey: 200" 200
assert_eq "black was not refunded" 0 "$(qty alice charcoal_black)"
assert_eq "the grey was charged" 0 "$(qty alice charcoal_grey_1)"

echo "-- one locked index rejects the whole save, including pixels she could have paid for"
python3 -c 'import os,sqlite3; c=sqlite3.connect(os.environ["DB_PATH"]); c.execute("update inventory set quantity=2 where item_id=(select item_id from items where slug=\"charcoal_black\") and player_id=(select player_id from players where lg_username=\"alice\")"); c.commit()'
LOCKED="$(python3 -c 'px=["0"]*1024
for i in range(200): px[i]="1"
px[200]="5"
print("["+",".join(px)+"]")')"
set_clock $((T0 + 78000))
bpaint alice "$LOCKED"; assert_code "200 black plus one locked index: 400" 400
assert_contains "locked_index" "$BODY" locked_index
assert_eq "the affordable black was not charged either" 2 "$(qty alice charcoal_black)"

echo "-- a username with a dot is still a visit-checked read, not a 400"
su a.b
befriend a.b alice
bsee a.b alice; assert_code "friend a.b can read alice" 200
assert_contains "pixels" "$BODY" '"pixels":'

# ============================================================================
# BUILD studio 4 (CLAUDE_STUDIO_4.md): the rest of the ladder + the second canvas.
# Same accounts as above; alice already has a portrait row, no painting row yet.
# ============================================================================
bpaintPainting() { post "$D/presence/painting/painting" "{\"pixels\":$2}" "$(jar $1)"; }
bgetPainting()   { get "$D/presence/painting/painting" "$(jar $1)"; }
bseePainting()   { get "$D/presence/painting/$2/painting" "$(jar $1)"; }
discover() { # discover USERNAME RECIPE_SLUG -- direct DB seed, same style as the inventory patches above
  python3 -c 'import os,sqlite3,sys
c=sqlite3.connect(os.environ["DB_PATH"])
c.execute("insert into discoveries (player_id, recipe_id, first_ms) select p.player_id, r.recipe_id, 0 from players p, recipes r where p.lg_username=? and r.slug=?", (sys.argv[1], sys.argv[2]))
c.commit()' "$1" "$2"
}
setqty() { # setqty USERNAME ITEM_SLUG QTY
  python3 -c 'import os,sqlite3,sys
c=sqlite3.connect(os.environ["DB_PATH"])
c.execute("insert into inventory (player_id, item_id, quantity, updated_at) select player_id, (select item_id from items where slug=?), ?, 0 from players where lg_username=? on conflict(player_id, item_id) do update set quantity=excluded.quantity", (sys.argv[2], sys.argv[3], sys.argv[1]))
c.commit()' "$1" "$2" "$3"
}
paintingCount() { dbq "select count(*) from paintings pa join players p on p.player_id=pa.player_id where p.lg_username='$1' and pa.kind='painting'"; }
portraitBlob() { dbq "select hex(pixels) from paintings pa join players p on p.player_id=pa.player_id where p.lg_username='$1' and pa.kind='portrait'"; }

echo "-- unbound is 403 on both new painting routes"
mint
get "$D/presence/painting/painting"; assert_code "guest GET own painting is 403" 403
post "$D/presence/painting/painting" "{\"pixels\":$(blank)}"; assert_code "guest POST painting is 403" 403

echo "-- own GET with no row: 1024 zeros and the palette, no insert, no stub, no grant"
set_clock $((T0 + 100000))
assert_eq "no painting row yet" 0 "$(paintingCount alice)"
BLACK_BEFORE="$(qty alice charcoal_black)"  # whatever her portrait spending left it at, above -- not necessarily 0
bgetPainting alice; assert_code "alice's first painting GET: 200" 200
assert_contains "1024 zeros" "$BODY" '"pixels":[0,0,0'
assert_eq "still no row (own GET must not insert)" 0 "$(paintingCount alice)"
assert_eq "no charcoal_black grant from touching the painting route" "$BLACK_BEFORE" "$(qty alice charcoal_black)"

echo "-- index 14 is locked on either canvas (never in the fixed table)"
set_clock $((T0 + 106000))
bpaintPainting alice "$(paint 0 1 14)"; assert_code "index 14 on the painting: 400" 400
assert_contains "locked_index" "$BODY" locked_index
assert_eq "a locked POST inserts nothing" 0 "$(paintingCount alice)"
set_clock $((T0 + 112000))
bpaint alice "$(paint 0 1 14)"; assert_code "index 14 on the portrait too: 400" 400
assert_contains "locked_index" "$BODY" locked_index

echo "-- index 5 (shell white) is locked_index with no discovery, even though it is in the table now"
set_clock $((T0 + 118000))
bpaintPainting alice "$(paint 0 1 5)"; assert_code "shell white, undiscovered: 400" 400
assert_contains "locked_index" "$BODY" locked_index
assert_eq "still no row" 0 "$(paintingCount alice)"

echo "-- after discovering shell_white, it costs shell_white and never touches charcoal"
discover alice shell_white
setqty alice shell_white 0
set_clock $((T0 + 124000))
bpaintPainting alice "$(paint 0 1 5)"; assert_code "discovered but qty 0: 409 insufficient, not locked" 409
assert_contains "insufficient" "$BODY" insufficient
assert_eq "still no row (a rejected save inserts nothing)" 0 "$(paintingCount alice)"
setqty alice shell_white 1
set_clock $((T0 + 130000))
bpaintPainting alice "$(paint 0 200 5)"; assert_code "200 shell-white pixels, now affordable: 200" 200
assert_eq "the first ACCEPTED save is what inserts the row" 1 "$(paintingCount alice)"
assert_eq "shell_white spent" 0 "$(qty alice shell_white)"
assert_eq "charcoal_black untouched by a painting save" "$BLACK_BEFORE" "$(qty alice charcoal_black)"

echo "-- the painting save did not touch the portrait blob, and the reverse"
BEFORE_PORTRAIT="$(portraitBlob alice)"
setqty alice shell_white 1
set_clock $((T0 + 136000))
bpaintPainting alice "$(paint 200 200 5)"; assert_code "another 200 shell-white pixels: 200" 200
assert_eq "the portrait blob is unchanged by a painting save" "$BEFORE_PORTRAIT" "$(portraitBlob alice)"
BEFORE_PAINTING="$(dbq "select hex(pixels) from paintings pa join players p on p.player_id=pa.player_id where p.lg_username='alice' and pa.kind='painting'")"
setqty alice charcoal_black 1
set_clock $((T0 + 142000))
bpaint alice "$(paint 500 1 1)"; assert_code "a portrait save: 200" 200
assert_eq "the painting blob is unchanged by a portrait save" "$BEFORE_PAINTING" "$(dbq "select hex(pixels) from paintings pa join players p on p.player_id=pa.player_id where p.lg_username='alice' and pa.kind='painting'")"

echo "-- last_paint_ms is shared: a portrait save blocks a painting save within 5s, and the reverse"
set_clock $((T0 + 142500))
bpaintPainting alice "$(blank)"; assert_code "painting save right after a portrait save: 429" 429
assert_contains "too_fast" "$BODY" too_fast
set_clock $((T0 + 148000))
setqty alice shell_white 1
bpaintPainting alice "$(paint 0 1 5)"; assert_code "5s later, painting save: 200" 200
set_clock $((T0 + 148500))
setqty alice charcoal_black 1
bpaint alice "$(paint 999 1 1)"; assert_code "portrait save right after a painting save: 429" 429
assert_contains "too_fast" "$BODY" too_fast

echo "-- visit: friend yes, stranger no, open-stranger no, open-friend yes, locked-friend no"
bpolicy alice friends >/dev/null
bseePainting carol alice; assert_code "stranger refused" 403
assert_contains "not_welcome" "$BODY" not_welcome
bseePainting bob alice; assert_code "friend can read the painting" 200
assert_contains "the friend gets pixels, not the palette" "$BODY" '"pixels":'
assert_not_contains "no palette on a visit read" "$BODY" palette
bpolicy alice open; assert_code "policy open" 200
bseePainting carol alice; assert_code "an open-policy stranger is still refused" 403
bseePainting bob alice; assert_code "her friend still can, under open" 200
bpolicy alice locked; assert_code "policy locked" 200
bseePainting bob alice; assert_code "a friend is refused once it is locked" 403
bpolicy alice friends >/dev/null

echo "-- a GET is not a stay: civic frozen fails closed, same as the portrait already does"
# A never-before-queried viewer: bob's own friends-list answer for alice is already hot in the 15s
# cache from the checks just above, so freezing civic now would prove nothing about him -- the daemon
# would just reuse the cached "yes". dana is fresh: her first-ever visit check is what actually asks.
su dana
befriend dana alice >/dev/null
freeze_civic
bseePainting dana alice; assert_code "civic silent, never asked before: refused, not shown" 403
thaw_civic
bseePainting dana alice; assert_code "civic back: the friend reads it now" 200

echo "-- a friend who never touched the second canvas: 404, and that GET does not insert"
su empty_friend
befriend empty_friend alice >/dev/null
bseePainting alice empty_friend; assert_code "alice has never painted empty_friend's canvas: 404" 404
assert_contains "no_painting" "$BODY" no_painting
assert_eq "the visit GET did not create a row for empty_friend" 0 "$(paintingCount empty_friend)"
