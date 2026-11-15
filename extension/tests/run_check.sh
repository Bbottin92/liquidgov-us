#!/usr/bin/env bash
# The extension check (2026-11-15): a test copy pointed at a throwaway local server (port 18137) and a plain page
# (port 18138); headless Chromium through Playwright (node). Never touches the live site.
# Usage: bash run_check.sh <liquidgov_http> <liquidgov_e2e_seed>
set -u
[ -x "${1:-}" ] && [ -x "${2:-}" ] || { echo "usage: run_check.sh <liquidgov_http> <liquidgov_e2e_seed>"; exit 2; }
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
S=$(cd "$(dirname "$0")" && pwd)
P=$(cd "$S/../../liquidgov-platform" && pwd)
W=/tmp/lg-ext-check; rm -rf $W; mkdir -p $W/blobs $W/page
cp -r "$S/.." $W/ext; rm -rf $W/ext/tests
sed -i "s#var LG_SITE = 'https://liquidgov.us';#var LG_SITE = 'http://127.0.0.1:18137';#; s#return u.protocol === 'https:' \&\&#return u.hostname === 'localhost' ||#" $W/ext/common.js
sed -i "s#mode: 'closed'#mode: 'open'#" $W/ext/content.js
python3 - $W/ext/manifest.json <<'PY'
import json,sys; m=json.load(open(sys.argv[1])); m["host_permissions"]+= ["http://127.0.0.1/*","http://localhost/*"]; json.dump(m,open(sys.argv[1],"w"))
PY
echo '<!doctype html><title>t</title><p>a page</p>' > $W/page/a.html
"$SEED" $W/store.lgp > $W/sessions.txt
(cd $P && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=18137 "$BIN") > $W/server.log 2>&1 &
SV=$!
(cd $W/page && exec python3 -m http.server 18138 --bind 127.0.0.1) > /dev/null 2>&1 &
PG=$!
trap 'kill $SV $PG 2>/dev/null' EXIT
for i in $(seq 50); do curl -s -o /dev/null --max-time 1 http://127.0.0.1:18137/ && break; sleep 0.2; done
ADMIN=$(awk '$1=="Bbottin"{print $2}' $W/sessions.txt); PAL=$(awk '$1=="pal"{print $2}' $W/sessions.txt)
for s in $ADMIN $PAL; do curl -s -o /dev/null -b "lg_session=$s" -d choice=1 -d back=/settings http://127.0.0.1:18137/here/choice; done
cd /tmp && node $S/check.js $W/ext $ADMIN $PAL
