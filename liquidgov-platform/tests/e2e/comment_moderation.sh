#!/usr/bin/env bash
# Operator moderation of single comments and older posts (Brandon 2026-11-08: "and what about posts and groups and
# anything else that might need deleting?"): /admin/comments lists comments on posts and on proposals, searchable;
# delete removes only that one comment after archiving it to <store>.moderation.jsonl (refused if the archive can't
# be written). /admin/posts?q= finds posts beyond the newest 80. Only operators (LIQUIDGOV_OPERATORS) get any of it.
# Usage: bash comment_moderation.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18099. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-comment-moderation; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18099; URL=http://127.0.0.1:$PORT
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
row() { awk -F'\t' -v t="$1" -v pat="$2" '$1==t && $0 ~ pat' "$W/store.lgp"; }

start
# one old post (pushed past the newest 80 by 85 newer ones), a post with two comments, a proposal with two comments
post $PAL /feed -F "text=Oldneedle post from long ago" -F "anybody=1" > /dev/null
for i in $(seq 85); do post $PAL /feed -F "text=filler post $i" -F "anybody=1" > /dev/null; done
post $PAL /feed -F "text=Busroute post" -F "anybody=1" > /dev/null
pid=$(row post Busroute | cut -f2 | head -1)
post $STRANGER "/post/$pid/reply" --data-urlencode "text=Spamcomment buy cheap pills" > /dev/null
post $STRANGER "/post/$pid/reply" --data-urlencode "text=Goodcomment the 12 is always late" > /dev/null
post $PAL /proposals -d "title=Plant shade trees" -d "body=A real proposal" -d "kind=0" > /dev/null
prop=$(row prop shade | cut -f2 | head -1)
post $STRANGER /proposals/reply -d "id=$prop" --data-urlencode "text=Propspam abusive words" > /dev/null
post $STRANGER /proposals/reply -d "id=$prop" --data-urlencode "text=Propgood I support this" > /dev/null
spam_c=$(row reply Spamcomment | cut -f2); good_c=$(row reply Goodcomment | cut -f2)
pspam_c=$(row prply Propspam | cut -f2); pgood_c=$(row prply Propgood | cut -f2)
check "precondition: 87 posts, two post comments, two proposal comments" \
    '[ "$(awk -F"\t" "\$1==\"post\"" "$W/store.lgp" | wc -l)" -ge 87 ] && [ -n "$spam_c" ] && [ -n "$good_c" ] && [ -n "$pspam_c" ] && [ -n "$pgood_c" ]' \
    "posts=$(awk -F'\t' '$1=="post"' "$W/store.lgp" | wc -l) c=$spam_c,$good_c p=$pspam_c,$pgood_c"

# only operators
r=$(get $PAL /admin/comments nc.html); check "a non-operator gets no comments page" '[[ "$r" == 404* ]]' "$r"
r=$(post $PAL /admin/comments/delete -d "on=post" -d "id=$spam_c"); check "a non-operator's comment delete is refused" '[[ "$r" == 404* ]]' "$r"
check "after the refused delete the comment is still there" '[ -n "$(row reply Spamcomment)" ]' "gone"
r=$(get $ADMIN /admin/comments ac.html)
check "the operator sees both kinds of comment" '[[ "$r" == 200* ]] && grep -q "Spamcomment" $W/ac.html && grep -q "Propspam" $W/ac.html && grep -q "/admin/comments/delete" $W/ac.html' "$r"
check "the operator tab bar links Comments" 'grep -q "href=\"/admin/comments\">Comments" $W/ac.html' "no tab"
r=$(get $ADMIN "/admin/comments?q=propspam" aq.html)
check "comment search is case-insensitive and filters" 'grep -q "Propspam" $W/aq.html && ! grep -q "Goodcomment" $W/aq.html' "$r"

# older posts are reachable by search
r=$(get $ADMIN /admin/posts ap.html); check "precondition: the old post is past the newest-80 list" '[[ "$r" == 200* ]] && ! grep -q "Oldneedle" $W/ap.html' "$r"
r=$(get $ADMIN "/admin/posts?q=oldneedle" aps.html)
check "post search finds the old post and only it" 'grep -q "Oldneedle" $W/aps.html && ! grep -q "filler post" $W/aps.html' "$r"

# delete refused when the archive can't be written
mkdir -p "$ARCH"
r=$(post $ADMIN /admin/comments/delete -d "on=post" -d "id=$spam_c"); check "an unwritable archive refuses the delete" '[[ "$r" == *"archive-failed"* ]]' "$r"
check "after the refused delete the comment is still there" '[ -n "$(row reply Spamcomment)" ]' "gone without evidence"
rmdir "$ARCH"

# bad input changes nothing
r=$(post $ADMIN /admin/comments/delete -d "on=group" -d "id=$spam_c"); check "an unknown kind changes nothing" '[[ "$r" == *"done=none"* ]] && [ -n "$(row reply Spamcomment)" ]' "$r"
r=$(post $ADMIN /admin/comments/delete -d "on=post" -d "id=999999"); check "an unknown id changes nothing" '[[ "$r" == *"done=none"* ]]' "$r"
r=$(post $ADMIN /admin/comments/delete -d "on=post" -d "id=abc"); check "a non-numeric id changes nothing" '[[ "$r" == *"done=none"* ]]' "$r"
check "after the bad inputs every comment remains" '[ -n "$(row reply Spamcomment)" ] && [ -n "$(row reply Goodcomment)" ] && [ -n "$(row prply Propspam)" ] && [ -n "$(row prply Propgood)" ]' "lost"

# delete one post comment and one proposal comment
r=$(post $ADMIN /admin/comments/delete -d "on=post" -d "id=$spam_c"); check "post comment delete redirects done=deleted" '[[ "$r" == *"done=deleted"* ]]' "$r"
r=$(post $ADMIN /admin/comments/delete -d "on=proposal" -d "id=$pspam_c"); check "proposal comment delete redirects done=deleted" '[[ "$r" == *"done=deleted"* ]]' "$r"
check "only the spam comments are gone" '[ -z "$(row reply Spamcomment)" ] && [ -z "$(row prply Propspam)" ] && [ -n "$(row reply Goodcomment)" ] && [ -n "$(row prply Propgood)" ]' "rows wrong"
check "the post and the proposal themselves remain" '[ -n "$(row post Busroute)" ] && [ -n "$(row prop shade)" ]' "parent lost"
check "the archive has both deleted comments with who deleted them" 'grep -q "Spamcomment" "$ARCH" && grep -q "Propspam" "$ARCH" && grep -q "\"deleted_by\":\"Bbottin\"" "$ARCH" && ! grep -q "Goodcomment" "$ARCH"' "$(head -c 400 "$ARCH" 2>/dev/null)"
check "the audit log names the deletes" '[ "$(grep -c "delete_comment" $W/store.lgp)" -ge 2 ]' "no audit rows"
r=$(get $ADMIN "/post/$pid" pp.html); check "the post page no longer shows the deleted comment" '! grep -q "Spamcomment" $W/pp.html && grep -q "Goodcomment" $W/pp.html' "$r"

# survives a restart
stop; start
r=$(get $ADMIN "/post/$pid" pr.html); check "after a restart the deletes hold" '! grep -q "Spamcomment" $W/pr.html && grep -q "Goodcomment" $W/pr.html' "$r"
r=$(get $ADMIN "/proposals?id=$prop" ppr.html); check "after a restart the proposal comment stays deleted" '! grep -q "Propspam" $W/ppr.html && grep -q "Propgood" $W/ppr.html' "$r"

echo "passes=$passes fails=$fails"
[ "$fails" = 0 ]
