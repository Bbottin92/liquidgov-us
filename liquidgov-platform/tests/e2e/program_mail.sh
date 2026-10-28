#!/bin/bash
# End-to-end check of programs in mail against the real liquidgov_http on a spare port (18080) with a
# throwaway store. Usage: bash e2e.sh <path to liquidgov_http> <path to lg-seed>
set -u
BIN=$1; SEED=$2
W=/tmp/lg-e2e; rm -rf $W; mkdir -p $W/blobs
PORT=18080; URL=http://127.0.0.1:$PORT
fails=0
ok()   { echo "PASS $1"; }
bad()  { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

$SEED $W/store.lgp > $W/sessions.txt || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' $W/sessions.txt; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)

start() {
    env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$@" \
        $BIN > $W/server.log 2>&1 &
    SERVER=$!
    for i in $(seq 50); do curl -s -o /dev/null $URL/ && return; sleep 0.2; done
    echo "server did not start"; cat $W/server.log; exit 1
}
stop() { kill $SERVER; wait $SERVER 2>/dev/null; }

# Test files (made with the real zipfile writer)
python3 - "$W" <<'EOF'
import sys, zipfile, os
w = sys.argv[1]
with zipfile.ZipFile(f"{w}/small.zip", "w") as z:
    z.writestr("Jammo/Jammo.exe", "pretend program")
    z.writestr("Jammo/readme.txt", "hello")
with zipfile.ZipFile(f"{w}/big.zip", "w", zipfile.ZIP_STORED) as z:
    z.writestr("Jammo/data.bin", os.urandom(30 * 1024 * 1024))
with zipfile.ZipFile(f"{w}/two_mb.zip", "w", zipfile.ZIP_STORED) as z:
    z.writestr("x.bin", os.urandom(2 * 1024 * 1024))
pe = bytearray(512); pe[0:2] = b"MZ"; pe[0x3C] = 0x80; pe[0x80:0x84] = b"PE\0\0"
open(f"{w}/tool.exe", "wb").write(pe)
open(f"{w}/tool.elf", "wb").write(b"\x7fELF" + bytes(60))
open(f"{w}/note.txt", "w").write("plain text still works\n")
EOF

send() { # session to file name [path, default /mail] [extra curl args...] -> "code location"
    local s=$1 to=$2 file=$3 name=$4 path=${5:-/mail}; shift 5 2>/dev/null || shift $#
    curl -s -o $W/resp.html -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" \
        -F "to=$to" -F "text=see attached" -F "attachment=@$file;filename=$name" $URL$path
}
upload_ok() { # session uri length -> the app's answer to nginx's auth_request
    curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$1" -H "X-Original-URI: $2" -H "X-Original-Length: $3" \
        $URL/internal/upload-ok
}
grant() { # admin-session target kinds... max_mb
    local target=$1 mb=$2; shift 2
    local args=(-d "username=$target" -d "max_mb=$mb")
    for k in "$@"; do args+=(-d "k_$k=on"); done
    curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$ADMIN" "${args[@]}" $URL/admin/accounts/programs
}

start

r=$(send $PAL Bbottin $W/small.zip "Jammo.zip")
check "a member without permission can't send a zip" '[[ "$r" == *"err=program"* ]]' "$r"
curl -s -b "lg_session=$PAL" "$URL/mail?with=Bbottin&err=program" > $W/pal_err.html
check "they're told to ask an admin" 'grep -q "needs an admin" $W/pal_err.html' "no message"

r=$(send $ADMIN pal $W/small.zip "../../Jammo Test<script>.zip")
check "the admin can send a zip" '[[ "$r" != *"err="* ]]' "$r"
curl -s -b "lg_session=$PAL" "$URL/mail?with=Bbottin" > $W/pal_thread.html
check "the friend sees the program card with a clean name" 'grep -q "Jammo Testscript.zip" $W/pal_thread.html' "no card"
check "the friend sees the warning" 'grep -q "A program from Bbottin" $W/pal_thread.html' "no warning"
blob=$(grep -o '/mail/attachment/[0-9a-f]*' $W/pal_thread.html | head -1)
curl -s -D $W/dl_headers.txt -o $W/dl.bin -b "lg_session=$PAL" "$URL$blob"
check "the download keeps its name" 'grep -qi "filename=\"Jammo Testscript.zip\"" $W/dl_headers.txt' "$(grep -i disposition $W/dl_headers.txt)"
check "the download is the same bytes" 'cmp -s $W/dl.bin $W/small.zip' "differs"
check "the download is typed as a zip" 'grep -qi "Content-Type: application/zip" $W/dl_headers.txt' "$(grep -i content-type $W/dl_headers.txt)"
code=$(curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$STRANGER" "$URL$blob")
check "someone outside the conversation can't download it" '[[ "$code" == 404 ]]' "$code"

curl -s -b "lg_session=$ADMIN" "$URL/mail?with=pal" > $W/admin_compose.html
check "a program sender's composer posts to plain /mail..." 'grep -q "action=\"/mail\"" $W/admin_compose.html' "no /mail"
check "...and switches to the big-upload door only for a big file" 'grep -q "/mail/big" $W/admin_compose.html' "no switch"
check "the composer shows the attached file and says Sending (2026-10-27: nothing showed)" 'grep -q "mailfile" $W/admin_compose.html && grep -q "Sending..." $W/admin_compose.html && grep -q "Attached: " $W/admin_compose.html' "missing"
check "a member's composer posts to plain /mail, with no big door" 'grep -q "action=\"/mail\"" $W/pal_thread.html && ! grep -q "/mail/big" $W/pal_thread.html' "wrong"

code=$(send $ADMIN pal $W/big.zip "Big.zip" | cut -d' ' -f1)
check "30 MB through the ordinary /mail is refused, even for the admin (413)" '[[ "$code" == 413 ]]' "$code"
r=$(send $ADMIN pal $W/big.zip "Big.zip" /mail/big)
check "the admin can send 30 MB through /mail/big" '[[ "$r" != *"err="* && "$r" == 30* ]]' "$r"

code=$(send $PAL Bbottin $W/big.zip "Big.zip" /mail/big | cut -d' ' -f1)
check "a member's 30 MB upload to /mail/big is refused before it is read (413)" '[[ "$code" == 413 ]]' "$code"
code=$(send $PAL Bbottin $W/big.zip "Big.zip" /mail/big -H "Expect:" | cut -d' ' -f1)
check "...also when the client sends the body at once (no Expect, like nginx): a 413, not a reset" '[[ "$code" == 413 ]]' "$code"
check "the server is still up after a 413" 'curl -s -o /dev/null $URL/' "down"

check "nginx's check: the admin may upload 30 MB to /mail/big" '[[ $(upload_ok $ADMIN /mail/big 31457280) == 204 ]]' "no"
check "nginx's check: ...and to a group's big door" '[[ $(upload_ok $ADMIN /mail/big/g/7/send 31457280) == 204 ]]' "no"
check "nginx's check: not over the 100 MB ceiling" '[[ $(upload_ok $ADMIN /mail/big 209715200) == 403 ]]' "yes"
check "nginx's check: not to any other path" '[[ $(upload_ok $ADMIN /feed 31457280) == 403 ]]' "yes"
check "nginx's check: a member without a grant may not" '[[ $(upload_ok $PAL /mail/big 31457280) == 403 ]]' "yes"
check "nginx's check: nobody signed out" '[[ $(upload_ok nosuchsession /mail/big 1000) == 403 ]]' "yes"
check "the refused-upload page says why" 'curl -s -b "lg_session=$PAL" $URL/mail/too-large | grep -q "Only an admin"' "no page"

curl -s -b "lg_session=$PAL" "$URL/mail?with=Bbottin" > $W/pal_thread2.html
bigblob=$(grep -o '/mail/attachment/[0-9a-f]*' $W/pal_thread2.html | tail -1)
code=$(curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$PAL" "$URL$bigblob")
check "without the nginx handoff a 30 MB download is refused (503), not held in memory" '[[ "$code" == 503 ]]' "$code"

r=$(send $ADMIN pal $W/tool.elf "invoice.pdf")
check "the admin can send a Linux program" '[[ "$r" != *"err="* ]]' "$r"
curl -s -b "lg_session=$PAL" "$URL/mail?with=Bbottin" > $W/pal_thread3.html
check "a Linux program can't pose as a PDF: it is named .bin" 'grep -q "invoice.pdf.bin" $W/pal_thread3.html' "no .bin"

check "the admin grants pal zips up to 1 MB" '[[ $(grant pal 1 zip) == 30* ]]' "post failed"
check "nginx's check: a small grant may still use the big door for an ordinary size" '[[ $(upload_ok $PAL /mail/big 1000) == 204 ]]' "no"
check "nginx's check: no length (chunked) is a no" '[[ $(upload_ok $PAL /mail/big "") == 403 ]]' "yes"
check "the busy page says to wait" 'curl -s -b "lg_session=$PAL" "$URL/mail/too-large?status=429" | grep -q "Wait a minute"' "no page"
check "a site error isn't called too large" 'curl -s -b "lg_session=$PAL" "$URL/mail/too-large?status=502" | grep -q "Try again in a moment"' "wrong words"
curl -s -b "lg_session=$ADMIN" "$URL/admin/accounts" > $W/admin.html
check "the accounts page shows pal's grant" 'grep -q "up to 1.0 MB" $W/admin.html' "not shown"
r=$(send $PAL stranger $W/small.zip "Pal.zip")
check "pal can now send a zip" '[[ "$r" != *"err="* ]]' "$r"
r=$(send $PAL stranger $W/tool.exe "tool.exe")
check "pal still can't send an exe" '[[ "$r" == *"err=program"* ]]' "$r"
r=$(send $PAL stranger $W/two_mb.zip "two.zip")
check "pal's 2 MB zip is over their 1 MB limit" '[[ "$r" == *"err=large"* ]]' "$r"
curl -s -b "lg_session=$PAL" "$URL/mail?with=stranger" > $W/pal_compose.html
check "pal's composer lists what they may send" 'grep -q "You can send programs: zip, up to 1.0 MB" $W/pal_compose.html' "no note"
check "pal's file picker offers .zip" 'grep -q "accept=\"[^\"]*\.zip" $W/pal_compose.html' "no .zip"
r=$(send $STRANGER pal $W/small.zip "S.zip")
check "a grant is per person (stranger still can't)" '[[ "$r" == *"err=program"* ]]' "$r"
r=$(send $PAL stranger $W/note.txt "note.txt")
check "ordinary attachments still work" '[[ "$r" != *"err="* ]]' "$r"

check "the admin revokes pal (no kinds ticked)" '[[ $(grant pal 1) == 30* ]]' "post failed"
r=$(send $PAL stranger $W/small.zip "Pal.zip")
check "after revoke pal can't send a zip" '[[ "$r" == *"err=program"* ]]' "$r"
code=$(curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$PAL" -d "username=stranger&k_zip=on&max_mb=5" $URL/admin/accounts/programs)
check "a non-admin can't grant" '[[ "$code" == 404 ]]' "$code"
check "the grant and log survived (store reload)" 'grep -q "programs_revoke" $W/store.lgp' "no log line"
stop

# The store written above loads again (no duplicate or unknown tags).
start
check "the server restarts on the written store" 'curl -s -o /dev/null $URL/' "down"
stop

# With LIQUIDGOV_BLOB_ACCEL the app hands program downloads to nginx.
start LIQUIDGOV_BLOB_ACCEL=/_lgblob/
curl -s -D $W/accel.txt -o $W/accel.bin -b "lg_session=$PAL" "$URL$blob"
id=${blob##*/}
check "a program download becomes X-Accel-Redirect" 'grep -qi "X-Accel-Redirect: /_lgblob/${id:0:2}/$id" $W/accel.txt' "$(grep -i accel $W/accel.txt)"
check "with the name and type still set" 'grep -qi "filename=\"Jammo Testscript.zip\"" $W/accel.txt && grep -qi "application/zip" $W/accel.txt' "missing"
check "and no body from the app" '[[ ! -s $W/accel.bin ]]' "$(wc -c < $W/accel.bin) bytes"
stop

echo "---- $fails failed"
exit $fails
