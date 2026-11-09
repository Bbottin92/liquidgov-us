#!/usr/bin/env bash
# Test server for the phone browser check (tests/browser/phone_check.py; run both through run_phone_check.py).
# Moved into the repo 2026-11-09 from scratch/2026-11-09-claude-site-sweep/serve2.sh. Throwaway store in
# /tmp/lg-phone-check, sample data (groups, cities, departments, proposals, posts, map settings) so the map has a
# column of boxes, then the server stays in the foreground until killed.
# Usage (WSL): bash phone_serve.sh <liquidgov_http> <liquidgov_e2e_seed> [<liquidgov-platform dir to serve assets from>]
#   The third argument lets the same check run against another checkout's page files (the old-build comparison).
# Prints "READY <url> <work dir>" when the sample data is in; the work dir holds sessions.txt and server.pid.
# Port 18140: outside naming.toml's e2e range (18080-18119), so scripts/next_port.py never gives it to a suite.
set -u
BIN=${1:-}; SEED=${2:-}
ASSETS_ROOT=${3:-$(cd "$(dirname "$0")/../.." && pwd)}
W=/tmp/lg-phone-check
PORT=18140; URL=http://127.0.0.1:$PORT

[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "PHONE_SERVE_FAIL missing binaries: '$BIN' '$SEED'"; exit 2; }
[ -f "$ASSETS_ROOT/assets/lgmap.js" ] || { echo "PHONE_SERVE_FAIL no assets/lgmap.js under $ASSETS_ROOT"; exit 2; }
if curl -s -o /dev/null --max-time 2 $URL/; then echo "PHONE_SERVE_FAIL port $PORT is busy; refusing"; exit 2; fi
rm -rf "$W"; mkdir -p "$W/blobs"
"$SEED" "$W/store.lgp" > "$W/sessions.txt" || { echo "PHONE_SERVE_FAIL seed failed"; exit 1; }
sid() { awk -v n="$1" '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "PHONE_SERVE_FAIL seed gave no sessions"; exit 1; }

(cd "$ASSETS_ROOT" && exec env -u LIQUIDGOV_OPERATOR LIQUIDGOV_OPERATORS=Bbottin LIQUIDGOV_STORE=$W/store.lgp \
    LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
S=$!
echo $S > "$W/server.pid"
trap 'kill $S 2>/dev/null' EXIT INT TERM
for i in $(seq 50); do
    kill -0 $S 2>/dev/null || { echo "PHONE_SERVE_FAIL server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
curl -s -o /dev/null --max-time 1 $URL/ || { echo "PHONE_SERVE_FAIL server never answered on $PORT"; exit 1; }

warns=0
p() { local s=$1 path=$2; shift 2; local r; r=$(curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$path")
      if [[ "$r" == *err=* || "$r" == 5* || "$r" == 404* ]]; then echo "WARN $path -> $r"; warns=$((warns+1)); fi; }
grp() { p "$1" /communities -d "slug=$2" --data-urlencode "name=$3" -d "category=$4" --data-urlencode "subcategory=$5" --data-urlencode "about=$6" ${7:+--data-urlencode "city=$7"}; }

grp $PAL fresno-neighbors "Fresno Neighbors" 0 "Neighborhood" "Neighbors in Fresno getting to know each other." "Fresno, CA"
grp $ADMIN tower-gardens "Tower District Gardens" 0 "Community Gardens" "Shared garden beds in the Tower District." "fresno ca"
grp $STRANGER fresno-runners "Fresno Runners" 4 "Running" "Saturday morning runs along the canal." "Fresno, CA"
grp $STRANGER oakland-bikes "Oakland Bikes" 0 "Transit" "Safer streets for bikes in Oakland." "Oakland, CA"
grp $ADMIN synth-heads "Synth Heads" 3 "Music" "Anyone making music with synths." ""
p $STRANGER /c/fresno-neighbors/join -d ""
p $ADMIN /c/fresno-neighbors/join -d ""
p $PAL /c/tower-gardens/join -d ""
p $PAL /city/fresno-ca/list -d "on=1"
p $ADMIN /city/fresno-ca/list -d "on=1"
for t in "Parks Department" "City Council" "Public Works" "Fresno Area Express (buses)"; do p $PAL /city/fresno-ca/tag --data-urlencode "tag=$t"; done
p $PAL /proposals -d "title=Plant shade trees on Blackstone Ave" -d "body=Blackstone is a heat island in summer. A row of shade trees from Shields to Ashlan would make it walkable." -d "kind=0" --data-urlencode "city=Fresno, CA" --data-urlencode "who=Parks Department and City Council" --data-urlencode "tag=Parks Department" --data-urlencode "tag=City Council"
p $ADMIN /proposals -d "title=Fix the potholes on Olive Ave" -d "body=The stretch by Roeding Park is rough on bikes and buses." -d "kind=0" --data-urlencode "city=Fresno, CA" --data-urlencode "who=Public Works" --data-urlencode "tag=Public Works" --data-urlencode "tag=Fresno Area Express (buses)"
p $STRANGER /proposals -d "title=Late-night buses on weekends" -d "body=Route 1 stops too early on Fridays and Saturdays." -d "kind=0" --data-urlencode "city=Fresno, CA"
p $PAL /proposals -d "title=Sitewide: should the Groups page show cities first?" -d "body=A sitewide straw poll, not about any one city." -d "kind=0"
ids=$(awk -F'\t' '$1=="prop"{print $2}' "$W/store.lgp")
first=$(echo "$ids" | sed -n 1p); second=$(echo "$ids" | sed -n 2p)
p $STRANGER /proposals/reply -d "id=$first" --data-urlencode "text=Yes please. The bus stops there have no shade at all."
p $ADMIN /proposals/reply -d "id=$first" --data-urlencode "text=Would the neighborhood council help water them the first year?"
p $PAL /proposals/reply -d "id=$second" --data-urlencode "text=The worst part is right by the zoo entrance."
p $STRANGER /proposals/react -d "id=$first" -d "kind=support"
p $ADMIN /proposals/react -d "id=$first" -d "kind=question"
p $PAL /proposals/react -d "id=$second" -d "kind=support"
p $STRANGER /proposals/vote -d "id=$first" -d "choice=0"
p $ADMIN /proposals/vote -d "id=$first" -d "choice=0"
p $PAL /c/fresno-neighbors -F "text=Block party on Saturday at the park on Shields. Bring a chair!"
p $STRANGER /c/fresno-neighbors -F "text=Anyone know a good bike shop near Tower?"
p $ADMIN /c/tower-gardens -F "text=Tomatoes are in. Come grab some from bed 4."
p $PAL /feed -F "text=Just moved to Fresno. Looking for neighbors who care about shade trees." -F "anybody=1"
p $ADMIN /feed -F "text=Working on the LiquidGov map today." -F "anybody=1"
# pal and Bbottin both show their friendship on the map
p $PAL /settings/map -d show=1 -d groups=1 -d posts=1 -d comments=1 -d friends=1 -d pains=1
p $ADMIN /settings/map -d show=1 -d groups=1 -d posts=1 -d comments=1 -d friends=1 -d pains=1
[ "$warns" -eq 0 ] || echo "PHONE_SERVE_WARNINGS $warns (sample data incomplete; the checks' preconditions will say what is missing)"
echo "READY $URL $W"
wait $S
