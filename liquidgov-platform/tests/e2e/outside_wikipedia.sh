#!/usr/bin/env bash
# Outside content, first connector (Brandon 2026-11-16: "we need to figure out how to connect other websites into this
# system ... things could grow from wikipedia over time"): an interest of yours that has a Wikipedia article shows its
# short summary inside its pool, labelled "from Wikipedia". Checks, against a LOCAL stand-in for the Wikipedia service
# (LIQUIDGOV_WIKI_BASE; the real site is never contacted): a summary fills body/by/sub; a word with no article, a
# disambiguation page and a failing service show nothing and break nothing; a long summary is cut short; escapes
# (\", \n, \uXXXX) are decoded; the visitor's session never reaches the service (the stand-in records every request's
# headers); a second look is served from the cache (one request, not two); someone else's interest is still not
# available to you; signed out is refused. The interest list is filled all at once, so a slow service costs one time limit, not one per interest.
# Usage: bash outside_wikipedia.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, site on port 18151, the stand-in on 18152. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-outside-wikipedia; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18151; FAKE=18152; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

for p in $PORT $FAKE; do
    if curl -s -o /dev/null --max-time 2 http://127.0.0.1:$p/; then echo "port $p is busy; refusing"; exit 2; fi
done
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }

# the stand-in: /api/<Title> -> JSON; every request's path and headers are appended to requests.log
cat > "$W/fake.py" <<'PY'
import http.server, json, sys
LONG = ("Snowboarding is a recreational activity and Olympic and Paralympic sport that involves descending a snow-covered "
        "slope while standing on a snowboard attached to a rider's feet. ") * 8
PAGES = {
    "Snowboarding": {"type": "standard", "title": "Snowboarding", "extract": LONG},
    "Hand_tools": {"type": "standard", "title": "Hand tool", "extract": "A hand tool is any tool that is powered by hand rather than a motor.\nIt is \"simple\" élégant 😀 & cheap."},
    "Mercury": {"type": "disambiguation", "title": "Mercury", "extract": "Mercury may refer to:"},
}
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        with open(sys.argv[2], "a") as f:
            f.write(self.path + "|" + str(dict(self.headers)) + "\n")
        name = self.path.rsplit("/", 1)[-1]
        name = name[:1].upper() + name[1:]   # like the real service, the first letter is not case-sensitive
        if name == "Boom":
            self.send_response(500); self.end_headers(); return
        if name == "Slowone":
            import time; time.sleep(10)
        page = PAGES.get(name)
        if not page:
            body = json.dumps({"type": "https://mediawiki.org/wiki/HyperSwitch/errors/not_found", "title": "Not found."}).encode()
            self.send_response(404)
        else:
            body = json.dumps(page).encode(); self.send_response(200)
        self.send_header("Content-Type", "application/json"); self.end_headers(); self.wfile.write(body)
http.server.HTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
PY
python3 "$W/fake.py" $FAKE "$W/requests.log" > "$W/fake.out" 2>&1 &
FAKEPID=$!

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; kill $FAKEPID; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; kill $FAKEPID; exit 1; }
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_WIKI_BASE="http://127.0.0.1:$FAKE/api/" LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER $FAKEPID 2>/dev/null; wait $SERVER $FAKEPID 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
node_of() { curl -s -b "lg_session=$1" "$URL/map/node?n=$2"; }
j() { python3 -c 'import json,sys; d=json.loads(sys.stdin.read()); print(eval(sys.argv[1]))' "$1" 2>/dev/null; }

curl -s -o /dev/null -b "lg_session=$PAL" --data-urlencode "display_name=Pal" --data-urlencode "interests=snowboarding, hand tools, mercury, zzzqqq, boom, slowone" $URL/me/profile
t0=$(date +%s)
you=$(node_of "$PAL" "mine:interests")
t1=$(date +%s)
check "a slow service does not hold the interest list up (ten-second reply, three-second limit, all at once)" '[ $((t1 - t0)) -le 6 ]' "took $((t1 - t0)) s"
ids=$(echo "$you" | j '" ".join(l["node"]["id"] + "=" + l["node"]["label"] for l in d["links"] if l["node"]["kind"]=="interest")')
check "the interest boxes are on the map" '[ -n "$ids" ]' "$you"
idof() { for p in $ids; do [ "${p#*=}" = "$1" ] && echo "${p%%=*}"; done; }
SNOW=$(idof snowboarding); TOOLS=$(idof hand); MERC=$(idof mercury); NONE=$(idof zzzqqq); BOOM=$(idof boom)
check "...and the list carries the summary for the interests that have an article" 'echo "$you" | j "[l[\"node\"][\"by\"] for l in d[\"links\"] if l[\"node\"][\"label\"]==\"snowboarding\"][0]" | grep -qx Wikipedia' "$you"
check "...and nothing for the ones that do not" '[ "$(echo "$you" | j "[l[\"node\"].get(\"body\",\"\") for l in d[\"links\"] if l[\"node\"][\"label\"]==\"zzzqqq\"][0]")" = "" ]' "$you"

n=$(node_of "$PAL" "$SNOW")
check "an interest with an article carries its summary" '[ -n "$SNOW" ] && echo "$n" | j "d[\"node\"][\"body\"]" | grep -q "recreational activity"' "$n"
check "...labelled from Wikipedia" '[ "$(echo "$n" | j "d[\"node\"][\"by\"]")" = "Wikipedia" ] && [ "$(echo "$n" | j "d[\"node\"][\"sub\"]")" = "from Wikipedia" ]' "$n"
check "...cut short (a long article is not copied)" '[ "$(echo "$n" | j "len(d[\"node\"][\"body\"])")" -le 620 ]' "$(echo "$n" | j 'len(d["node"]["body"])')"
check "...it is still the same pool (kind interest)" '[ "$(echo "$n" | j "d[\"node\"][\"kind\"]")" = "interest" ]' "$n"

n=$(node_of "$PAL" "$TOOLS")
check "a redirect title shows the article's own name" '[ "$(echo "$n" | j "d[\"node\"][\"sub\"]")" = "from Wikipedia: Hand tool" ]' "$n"
check "escapes are decoded (quotes, accents, an emoji, a newline becomes a space)" 'echo "$n" | j "d[\"node\"][\"body\"]" | grep -q "\"simple\" élégant 😀 & cheap"' "$(echo "$n" | j 'd["node"]["body"]')"
n=$(node_of "$PAL" "$MERC")
check "a disambiguation page shows nothing" '[ "$(echo "$n" | j "d[\"node\"][\"body\"]")" = "" ]' "$n"
n=$(node_of "$PAL" "$NONE")
check "a word with no article shows nothing" '[ "$(echo "$n" | j "d[\"node\"][\"body\"]")" = "" ]' "$n"
n=$(node_of "$PAL" "$BOOM")
check "a failing service shows nothing and the pool still opens" '[ "$(echo "$n" | j "d[\"node\"][\"label\"]")" = "boom" ] && [ "$(echo "$n" | j "d[\"node\"][\"body\"]")" = "" ]' "$n"

before=$(grep -ci "snowboarding" "$W/requests.log")
node_of "$PAL" "$SNOW" >/dev/null; node_of "$PAL" "$SNOW" >/dev/null
after=$(grep -ci "snowboarding" "$W/requests.log")
check "a second look is served from the cache (no new request)" '[ "$before" = "$after" ] && [ "$before" = 1 ]' "before=$before after=$after"
check "the visitor's session never reaches the service" '! grep -qi "lg_session\|cookie" "$W/requests.log"' "$(grep -i cookie "$W/requests.log" | head -1)"
check "the client name is generic" 'grep -q "LiquidGov-Reader/1.0" "$W/requests.log" && ! grep -qi "pal\|bbottin" "$W/requests.log"' "$(head -1 "$W/requests.log")"

x=$(node_of "$STRANGER" "$SNOW")
check "someone else's interest is still not available to you" '[ "$(echo "$x" | j "d.get(\"error\",\"\")")" = "not found" ]' "$x"
code=$(curl -s -o /dev/null -w '%{http_code}' "$URL/map/node?n=$SNOW")
check "signed out: refused" '[ "$code" = 401 ]' "$code"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
