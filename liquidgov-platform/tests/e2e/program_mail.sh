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
open(f"{w}/note.txt", "w").write("plain text still works\n")
EOF

send() { # session to file name -> "code location"
    curl -s -o $W/resp.html -w '%{http_code} %{redirect_url}' -b "lg_session=$1" \
        -F "to=$2" -F "text=see attached" -F "attachment=@$3;filename=$4" $URL/mail
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

r=$(send $ADMIN pal $W/big.zip "Big.zip")
check "the admin can send 30 MB (over the old 25 MB wall)" '[[ "$r" != *"err="* && "$r" == 30* ]]' "$r"

code=$(send $PAL Bbottin $W/big.zip "Big.zip" | cut -d' ' -f1)
check "a member's 30 MB upload is refused before it is read (413)" '[[ "$code" == 413 ]]' "$code"
check "the server is still up after a 413" 'curl -s -o /dev/null $URL/' "down"

check "the admin grants pal zips up to 1 MB" '[[ $(grant pal 1 zip) == 30* ]]' "post failed"
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
