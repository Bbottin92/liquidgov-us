#!/usr/bin/env bash
# Map globes, G1 (Brandon 2026-11-20: "a globe in the middle of the friends pool, the events pool, the market pool, and
# one in the middle of the social pool ... opens a fullscreen map page with a stacked list"; answers: real street map,
# all four now, nearest first).
#   - the You ring has Market and Social (always, "None yet"), next to Events
#   - Friends, Events, Market and Social pools lead to /globe/<kind> (the pool's page, drawn as a globe by lgmap.js)
#   - /globe/friends lists your friends nearest first by "Where you live now" (city points from a local stand-in for
#     OpenStreetMap's Nominatim, LIQUIDGOV_GEO_BASE; the real service is never contacted), with the map's points
#   - a friend whose profile is only for themselves shows no city and no point
#   - /globe/events, /globe/market, /globe/social answer with their empty states; /globe/other is 404; signed out: login
#   - the street-map files are served (leaflet.js, leaflet.css, globe.js)
# Usage: bash map_globes.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, site on port 18153, the stand-in on 18154. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-globes; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18153; FAKE=18154; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

for p in $PORT $FAKE; do if curl -s -o /dev/null --max-time 2 http://127.0.0.1:$p/; then echo "port $p is busy; refusing"; exit 2; fi; done
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
# seed: Bbottin<->pal and pal<->stranger are friends
ME=$(sid Bbottin); PAL=$(sid pal); STR=$(sid stranger)
[ -n "$ME" ] && [ -n "$PAL" ] && [ -n "$STR" ] || { echo "seed gave no sessions"; exit 1; }

# The stand-in for Nominatim: a few cities, logged; anything else is "not found" (an empty list).
cat > "$W/fake.py" <<'PY'
import http.server, json, sys, urllib.parse
CITIES = {"fresno, ca": ("36.7378", "-119.7871"), "clovis, ca": ("36.8252", "-119.7029"),
          "boise": ("43.6150", "-116.2023"), "secret town": ("10.0000", "10.0000")}
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query).get("q", [""])[0]
        open(sys.argv[2], "a").write(q + "\n")
        c = CITIES.get(q.lower())
        body = json.dumps([{"lat": c[0], "lon": c[1], "display_name": q}] if c else []).encode()
        self.send_response(200); self.send_header("Content-Type", "application/json"); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass
http.server.ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
PY
python3 "$W/fake.py" $FAKE "$W/requests.log" > "$W/fake.out" 2>&1 &
FAKEPID=$!
SERVER=""
start() {
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_GEO_BASE="http://127.0.0.1:$FAKE/search" LIQUIDGOV_STORE=$W/store.lgp \
        LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; kill $FAKEPID 2>/dev/null; }
trap stop EXIT
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
node() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -G --data-urlencode "n=$2" "$URL/map/node"; }
ids()  { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(l["node"]["id"] for l in d["links"]))' "$W/$1" 2>/dev/null; }
field() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next((l["node"].get(sys.argv[3],"") for l in d["links"] if l["node"]["id"]==sys.argv[2]), ""))' "$W/$1" "$2" "$3" 2>/dev/null; }
rows()  { python3 -c 'import re,sys; print(" ".join(re.findall(r"<li class=\"globerow\" data-name=\"([^\"]+)\"", open(sys.argv[1]).read())))' "$W/$1"; }
points() { python3 -c '
import re,sys,html,json
m=re.search(r"data-points=\"([^\"]*)\"", open(sys.argv[1]).read())
print(" ".join(p["name"] for p in json.loads(html.unescape(m.group(1)))) if m else "none")' "$W/$1"; }

start
r=$(node $ME you you.json)
check "the You ring answers 200" '[ "$r" = 200 ]' "$r"
for k in market social; do
    check "the You ring has mine:$k" 'ids you.json | grep -qw "mine:'$k'"' "$(ids you.json)"
    check "... with the second line None yet" '[ "$(field you.json mine:'$k' sub)" = "None yet" ]' "$(field you.json mine:$k sub)"
done
check "Events is still on the ring" 'ids you.json | grep -qw "mine:events"' "$(ids you.json)"
for k in friends events market social; do
    check "the $k pool leads to /globe/$k" '[ "$(field you.json mine:'$k' page)" = "/globe/'$k'" ]' "page=[$(field you.json mine:$k page)]"
done

# where people live: me Fresno, pal Boise (far); stranger becomes a friend in Clovis (near) below
post $ME  /me/profile -d "display_name=Brandon&living=Fresno%2C+CA" >/dev/null
post $PAL /me/profile -d "display_name=Pal+Person&living=Boise" >/dev/null
r=$(get $ME /globe/friends f1.html)
check "/globe/friends answers 200" '[ "$r" = 200 ]' "$r"
check "it lists my friend pal (and not stranger, who is not my friend)" '[ "$(rows f1.html)" = "pal" ]' "$(rows f1.html)"
check "with pal's city and a distance in miles" 'grep -q "Boise &middot; about [0-9]* mi" "$W/f1.html"' "$(grep -o 'Boise[^<]*' "$W/f1.html" | head -1)"
check "the map gets pal's point, named by the name pal shows" '[ "$(points f1.html)" = "Pal Person" ]' "$(points f1.html)"
check "the map is centred on my city" 'grep -q "data-center=\"36.73780,-119.78710\"" "$W/f1.html"' "$(grep -o 'data-center="[^"]*"' "$W/f1.html")"
check "it loads the street map files" 'grep -q "/assets/leaflet.js" "$W/f1.html" && grep -q "/assets/globe.js" "$W/f1.html" && grep -q "/assets/leaflet.css" "$W/f1.html"' "missing"
check "only the cities were sent to the stand-in" '[ "$(sort -u "$W/requests.log" | tr "\n" "|")" = "Boise|Fresno, CA|" ]' "$(tr '\n' '|' < "$W/requests.log")"

# a second friend, nearer than Boise: the list is nearest first
post $STR /friends -d "who=Bbottin" >/dev/null
post $ME /friends/accept -d "who=stranger" >/dev/null
post $STR /me/profile -d "display_name=Stranger&living=Clovis%2C+CA" >/dev/null
r=$(get $ME /globe/friends f2.html)
check "nearest first: stranger (Clovis, near) before pal (Boise, far)" '[ "$(rows f2.html)" = "stranger pal" ]' "$(rows f2.html)"

# a friend whose profile is only for themselves: no city and no point
post $PAL /me/profile -d "display_name=Pal+Person&living=Secret+Town" >/dev/null
post $PAL /settings/privacy -d "profile_visibility=3&posts_visibility=0&findability=1" >/dev/null
r=$(get $ME /globe/friends f3.html)
check "a friend whose profile is only for themselves: still listed, but their city is not shown" \
    'rows f3.html | grep -qw pal && ! grep -q "Secret Town" "$W/f3.html"' "$(rows f3.html)"
check "... and they are not on the map" '! points f3.html | grep -q "Pal Person"' "$(points f3.html)"

for k in events market social; do
    r=$(get $ME /globe/$k $k.html)
    check "/globe/$k answers 200 with its empty state and the map" \
        '[ "$r" = 200 ] && grep -q "class=\"empty\"" "$W/'$k'.html" && grep -q "id=\"globemap\"" "$W/'$k'.html"' "$r"
done
r=$(get $ME /globe/other o.html)
check "/globe/other is 404" '[ "$r" = 404 ]' "$r"
r=$(curl -s -o /dev/null -w '%{http_code}' "$URL/globe/friends")
check "signed out, a globe asks you to log in" '[ "$r" = 302 ] || [ "$r" = 303 ]' "$r"
for f in leaflet.js leaflet.css globe.js; do
    r=$(curl -s -o /dev/null -w '%{http_code}' "$URL/assets/$f")
    check "/assets/$f is served" '[ "$r" = 200 ]' "$r"
done

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
