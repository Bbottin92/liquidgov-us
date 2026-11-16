#!/usr/bin/env bash
# The site's link preview, edited by the operator (Brandon 2026-11-16: "i just want to be able to change the preview label
# thing myself from the admin controls page"). Checks: only the operator may open or change /admin/preview; a title and
# description set there are what the og: and twitter: tags say on every page (and are escaped); a JPEG or PNG upload
# becomes the og:image at /preview-image (public, served with its own type); a file that is not a JPEG or PNG is refused;
# removing the picture and resetting bring back the built-in preview; and with nothing set the page is exactly as before.
# Usage: bash admin_preview.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18139. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-admin-preview; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18139; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal)
[ -n "$ADMIN" ] && [ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
tag() { curl -s "$URL/" | grep -o "<meta [^>]*$1[^>]*>" | head -1; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }

# a tiny real PNG and JPEG (a PNG header + data; a JPEG header), and a text file pretending
python3 - "$W" <<'EOF'
import struct, sys, zlib
w = sys.argv[1]
def png():
    raw = b"\x00\xff\x00\x00" * 4
    def ch(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw * 2)) + ch(b"IEND", b"")
open(w + "/a.png", "wb").write(png())
open(w + "/b.jpg", "wb").write(b"\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xd9")
open(w + "/c.txt", "wb").write(b"not a picture\n")
open(w + "/big.png", "wb").write(png() + b"\x00" * (2 * 1024 * 1024 + 10))
EOF

check "nothing set: the og:image is the built-in one" 'tag "og:image\"" | grep -q "assets/og-image-v4.jpg"' "$(tag 'og:image"')"
check "nothing set: the og:title is the page's own title" 'tag "og:title" | grep -q "LiquidGov.US"' "$(tag og:title)"
check "a visitor cannot open /admin/preview" '[ "$(post "" /admin/preview)" = 303 ] || [ "$(post "" /admin/preview)" = 302 ] || [ "$(post "" /admin/preview)" = 401 ]' "$(post "" /admin/preview)"
check "a signed-in non-operator gets a plain 404" '[ "$(post "$PAL" /admin/preview)" = 404 ]' "$(post "$PAL" /admin/preview)"
check "...and cannot change it" '[ "$(post "$PAL" /admin/preview -d title=Hacked)" = 404 ]' "$(post "$PAL" /admin/preview -d title=Hacked)"
check "the operator can open the page" '[ "$(post "$ADMIN" /admin/preview)" = 200 ]' "$(post "$ADMIN" /admin/preview)"
check "...and it is a tab of the operator tools" 'curl -s -b "lg_session=$ADMIN" $URL/admin/preview | grep -q "href=\"/admin/preview\""' "no tab"

curl -s -o /dev/null -b "lg_session=$ADMIN" --data-urlencode 'title=Our own <b>title</b> & more' --data-urlencode 'description=A line we chose "ourselves"' $URL/admin/preview
check "the title is what og:title and twitter:title say, escaped" '[ "$(curl -s $URL/ | grep -o "Our own &lt;b&gt;title&lt;/b&gt; &amp; more" | wc -l)" -ge 2 ]' "$(tag og:title)"
check "the description is what og:description says, escaped" 'tag og:description | grep -q "A line we chose &quot;ourselves&quot;"' "$(tag og:description)"
check "it is the same on every page (a public one)" 'curl -s $URL/login | grep -q "Our own &lt;b&gt;title"' "login page"
check "the browser tab title is still each page's own" 'curl -s $URL/ | grep -o "<title>[^<]*</title>" | grep -q "LiquidGov.US"' "$(curl -s $URL/ | grep -o '<title>[^<]*</title>')"

code=$(post "$ADMIN" /admin/preview/image -F "image=@$W/a.png")
check "a PNG upload is accepted" '[ "$code" = 303 ]' "$code"
check "og:image is now the uploaded picture" 'tag "og:image\"" | grep -q "https://liquidgov.us/preview-image?v="' "$(tag 'og:image"')"
curl -s -D "$W/img.h" -o "$W/img.bin" "$URL/preview-image"
check "/preview-image is public and serves the picture with its own type" 'grep -qi "^content-type: image/png" "$W/img.h" && cmp -s "$W/img.bin" "$W/a.png"' "$(head -3 "$W/img.h")"
check "...with a cache time" 'grep -qi "^cache-control: public" "$W/img.h"' "$(grep -i cache "$W/img.h")"
v1=$(tag 'og:image"' | grep -o "v=[^\"]*")
post "$ADMIN" /admin/preview/image -F "image=@$W/b.jpg" >/dev/null
check "a JPEG replaces it, and the address changes so crawlers fetch it again" '[ "$(tag "og:image\"" | grep -o "v=[^\"]*")" != "$v1" ] && curl -s -D - -o /dev/null "$URL/preview-image" | grep -qi "content-type: image/jpeg"' "$(tag 'og:image"')"
before=$(curl -s "$URL/preview-image" | md5sum)
r=$(curl -s -b "lg_session=$ADMIN" -L -F "image=@$W/c.txt" $URL/admin/preview/image)
check "a file that is not a picture is refused, with a message" 'echo "$r" | grep -q "not a JPEG or PNG"' "no message"
check "...and the picture is unchanged" '[ "$(curl -s "$URL/preview-image" | md5sum)" = "$before" ]' "changed"
r=$(curl -s -b "lg_session=$ADMIN" -L -F "image=@$W/big.png" $URL/admin/preview/image)
check "a picture over 2 MB is refused" 'echo "$r" | grep -q "over 2 MB"' "no message"
check "a visitor cannot upload" '[ "$(post "" /admin/preview/image -F "image=@$W/a.png")" != 200 ] && [ "$(post "$PAL" /admin/preview/image -F "image=@$W/a.png")" = 404 ]' "uploaded"

post "$ADMIN" /admin/preview/image-remove -d x=1 >/dev/null
check "removing the picture brings the built-in one back (and /preview-image is gone)" 'tag "og:image\"" | grep -q "assets/og-image-v4.jpg" && [ "$(curl -s -o /dev/null -w "%{http_code}" $URL/preview-image)" = 404 ]' "$(tag 'og:image"')"
check "...the words stay" 'tag og:description | grep -q "ourselves"' "$(tag og:description)"
post "$ADMIN" /admin/preview/reset -d x=1 >/dev/null
check "reset brings back the built-in words too" '! tag og:description | grep -q "ourselves" && tag og:description | grep -q "This is YOUR government"' "$(tag og:description)"
long=$(python3 -c "print('x'*400)")
curl -s -o /dev/null -b "lg_session=$ADMIN" --data-urlencode "title=$long" --data-urlencode "description=$long" $URL/admin/preview
check "a title is cut to 120 and a description to 300 characters" '[ "$(tag og:title | grep -o "x*" | head -1 | wc -c)" -le 122 ] && [ "$(tag og:description | grep -o "x*" | head -1 | wc -c)" -le 302 ]' "$(tag og:title | wc -c)"
sleep 1.2   # the site caches the settings for a second
echo "passes=$passes fails=$fails"
[ $fails = 0 ]
