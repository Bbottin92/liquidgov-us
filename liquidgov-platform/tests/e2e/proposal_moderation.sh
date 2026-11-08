#!/usr/bin/env bash
# Operator moderation of proposals (Brandon 2026-11-08: "make it so i can delete and edit proposals for now? people
# are abusing it"): /admin/proposals lists them; edit replaces title and text; delete archives the proposal with its
# comments to <store>.moderation.jsonl first, then removes it with its comments, reactions and votes; a delete is
# refused when the archive can't be written. Only operators (LIQUIDGOV_OPERATORS) get any of it.
# Usage: bash proposal_moderation.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18091. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-proposal-moderation; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18091; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

SERVER=""
start() {
    env LIQUIDGOV_OPERATORS=Bbottin LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$BIN" >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; cat "$W/server.log"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
ARCH="$W/store.lgp.moderation.jsonl"

start
post $PAL /proposals -d "title=SPAM buy cheap stuff" -d "body=abusive text here" -d "kind=0" > /dev/null
post $PAL /proposals -d "title=Plant shade trees" -d "body=A real proposal" -d "kind=0" > /dev/null
spam=$(awk -F'\t' '$1=="prop" && $0 ~ /SPAM/ {print $2}' "$W/store.lgp" | head -1)
keep=$(awk -F'\t' '$1=="prop" && $0 ~ /shade/ {print $2}' "$W/store.lgp" | head -1)
check "precondition: two proposals stored" '[ -n "$spam" ] && [ -n "$keep" ]' "spam=$spam keep=$keep"
post $STRANGER /proposals/reply -d "id=$spam" --data-urlencode "text=this is spam, report it" > /dev/null
post $STRANGER /proposals/react -d "id=$spam" -d "kind=support" > /dev/null
check "precondition: the spam proposal has a comment" 'awk -F"\t" -v id="$spam" "\$1==\"prply\" && \$3==id" "$W/store.lgp" | grep -q .' "no comment row"

# only operators
r=$(get $PAL /admin/proposals np.html); check "a non-operator gets no proposals page" '[[ "$r" == 404* ]]' "$r"
r=$(post $PAL /admin/proposals/delete -d "id=$spam"); check "a non-operator's delete is refused" '[[ "$r" == 404* ]]' "$r"
check "after the refused delete the proposal is still there" 'awk -F"\t" -v id="$spam" "\$1==\"prop\" && \$2==id" "$W/store.lgp" | grep -q .' "gone"
r=$(get $ADMIN /admin/proposals ap.html); check "the operator sees the proposals page" '[[ "$r" == 200* ]] && grep -q "SPAM buy cheap stuff" $W/ap.html && grep -q "/admin/proposals/delete" $W/ap.html' "$r"
check "the operator tab bar links Proposals" 'grep -q "href=\"/admin/proposals\">Proposals" $W/ap.html' "no tab"

# edit
r=$(post $ADMIN /admin/proposals/edit -d "id=$keep" --data-urlencode "title=Plant shade trees on Blackstone" --data-urlencode "body=Edited by the operator")
check "edit redirects with done=edited" '[[ "$r" == *"done=edited"* ]]' "$r"
r=$(get $ADMIN "/proposals?id=$keep" pk.html); check "the edited title shows on the proposal" 'grep -q "Plant shade trees on Blackstone" $W/pk.html && grep -q "Edited by the operator" $W/pk.html' "$r"
r=$(post $ADMIN /admin/proposals/edit -d "id=$keep" -d "title=" -d "body=x"); check "an empty title is refused" '[[ "$r" == *"done=none"* ]]' "$r"

# delete is refused when the archive can't be written (a directory sits where the file would go)
mkdir -p "$ARCH"
r=$(post $ADMIN /admin/proposals/delete -d "id=$spam"); check "an unwritable archive refuses the delete" '[[ "$r" == *"archive-failed"* ]]' "$r"
check "after the refused delete the spam is still there" 'awk -F"\t" -v id="$spam" "\$1==\"prop\" && \$2==id" "$W/store.lgp" | grep -q .' "gone without evidence"
rmdir "$ARCH"

# delete
r=$(post $ADMIN /admin/proposals/delete -d "id=$spam"); check "delete redirects with done=deleted" '[[ "$r" == *"done=deleted"* ]]' "$r"
check "the archive has the deleted proposal with its comment" 'grep -q "SPAM buy cheap stuff" "$ARCH" && grep -q "this is spam, report it" "$ARCH" && grep -q "\"deleted_by\":\"Bbottin\"" "$ARCH"' "$(head -c 300 "$ARCH" 2>/dev/null)"
check "the proposal row is gone from the store" '! awk -F"\t" -v id="$spam" "\$1==\"prop\" && \$2==id" "$W/store.lgp" | grep -q .' "still there"
check "its comments are gone from the store" '! awk -F"\t" -v id="$spam" "\$1==\"prply\" && \$3==id" "$W/store.lgp" | grep -q .' "comment rows left"
r=$(get $ADMIN /proposals pl.html); check "the proposals page no longer shows it" '! grep -q "SPAM buy cheap stuff" $W/pl.html && grep -q "Plant shade trees on Blackstone" $W/pl.html' "$r"
check "the audit log names the delete" 'grep -q "delete_proposal" $W/store.lgp' "no audit row"

# survives a restart
stop; start
r=$(get $ADMIN /proposals pr.html); check "after a restart the delete and the edit hold" '! grep -q "SPAM buy cheap stuff" $W/pr.html && grep -q "Plant shade trees on Blackstone" $W/pr.html' "$r"
r=$(get $ADMIN "/proposals?id=$spam" pg.html); check "the deleted id no longer opens" '! grep -q "SPAM buy cheap stuff" $W/pg.html' "$r"

echo "passes=$passes fails=$fails"
[ "$fails" = 0 ]
