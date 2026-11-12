#!/usr/bin/env bash
# Profile CSS styles the whole profile page below the site header, not a small sandboxed box (Brandon 2026-11-12:
# "can we make the css area cover the whole profile page (except the title banner thing)"; decisions.md, all
# recommended options). The CSS is nested inside .lg-profile-scope; the outer div clips it; a "plain view" link outside
# the styled area drops it. Leaving the sandbox means the filter must hold on its own: escapes (\), the nesting
# selector (&), unbalanced braces, image-set() and other @-rules are refused, and stored CSS that fails today's rules
# is not rendered (the owner is told on /me).
# Usage: bash profile_css_page.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18121. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-profile-css-page; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18121; URL=http://127.0.0.1:$PORT
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
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$BIN") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
# save CSS as a user: prints the redirect target (Location header)
savecss() { curl -s -o /dev/null -w '%{redirect_url}' -b "lg_session=$1" --data-urlencode "css=$2" "$URL/me/css"; }
page() { curl -s -o "$W/$3" -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
# python helpers read the saved page
before() { python3 -c 'import sys; t=open(sys.argv[1],encoding="utf-8").read(); a=t.find(sys.argv[2]); b=t.find(sys.argv[3]); sys.exit(0 if 0<=a<b else 1)' "$W/$1" "$2" "$3"; }
inside() { python3 - "$W/$1" "$2" <<'PY'
import sys, re
t = open(sys.argv[1], encoding="utf-8").read()
start = t.find('class="lg-profile-scope"')
if start < 0: sys.exit(1)
# the scope div's content: count <div ...> / </div> from its opening tag
open_at = t.rfind("<div", 0, start); depth = 0; i = open_at
for m in re.finditer(r"<div\b|</div>", t[open_at:]):
    depth += 1 if m.group(0) == "<div" else -1
    if depth == 0:
        end = open_at + m.end(); break
else:
    sys.exit(1)
sys.exit(0 if sys.argv[2] in t[open_at:end] else 1)
PY
}

start
GOOD='.lg-theme h1{color:#a23b3b} .lg-theme{background:#fdf6e3} @media (max-width:600px){h1{font-size:2rem}}'
r=$(savecss $PAL "$GOOD")
check "good CSS saves (ok=css)" '[[ "$r" == *"ok=css"* ]]' "$r"
check "precondition: the store holds pal's CSS" 'grep -q "^ac" $W/store.lgp' "$(grep -c '^ac' $W/store.lgp)"

for who in STRANGER ANON; do
    s=""; [ $who = STRANGER ] && s=$STRANGER
    r=$(page "$s" /u/pal pal-$who.html)
    check "$who: /u/pal answers 200" '[ "$r" = 200 ]' "$r"
    check "$who: the CSS is nested in .lg-profile-scope" 'grep -qF ".lg-profile-scope{$GOOD}" $W/pal-$who.html' "$(grep -o 'lg-profile-scope{[^<]\{0,80\}' $W/pal-$who.html | head -1)"
    check "$who: no sandboxed theme iframe any more" '! grep -q "/u/pal/theme" $W/pal-$who.html' "iframe still there"
    check "$who: the site header comes before the styled area" 'before pal-$who.html "</header>" "class=\"lg-profile-outer\""' "order wrong or missing"
    check "$who: the outer div clips (contain:paint)" 'grep -qE "class=\"lg-profile-outer\"[^>]*contain:paint" $W/pal-$who.html' "no contain:paint"
    check "$who: the profile name is inside the styled area" 'inside pal-$who.html "<h1>pal</h1>"' "h1 not inside scope"
    check "$who: old .lg-theme CSS still matches (lg-theme inside the scope)" 'inside pal-$who.html "class=\"lg-theme\""' "no .lg-theme in scope"
    check "$who: a plain view link exists" 'grep -q "href=\"/u/pal?plain=1\"" $W/pal-$who.html' "no plain link"
    check "$who: the plain view link is outside the styled area" '! inside pal-$who.html "?plain=1"' "plain link inside scope"
done

r=$(page $STRANGER "/u/pal?plain=1" pal-plain.html)
check "plain view answers 200" '[ "$r" = 200 ]' "$r"
check "plain view has no author CSS" '! grep -qF "a23b3b" $W/pal-plain.html' "CSS still there"
check "plain view still shows the profile" 'grep -q "<h1>pal</h1>" $W/pal-plain.html' "no h1"
check "plain view links back to the styled view" 'grep -q "href=\"/u/pal\"" $W/pal-plain.html' "no way back"

r=$(page $STRANGER /u/stranger nocss.html)
check "control: a profile with no CSS has no plain view link" '! grep -q "?plain=1" $W/nocss.html' "plain link on a CSS-less profile"
check "control: ...and no scope style" '! grep -q "lg-profile-scope{" $W/nocss.html' "scope style without CSS"

# refused at save time (each leaves pal's good CSS in place)
refuse() {
    local name=$1 css=$2 reason=$3
    local loc; loc=$(savecss $PAL "$css")
    check "refused: $name (reason=$reason)" '[[ "$loc" == *"err=css"* && "$loc" == *"reason=$reason"* ]]' "$loc"
}
refuse 'escape \72' 'h1{color:\72 ed}' escape
refuse 'escaped url' 'h1{background:u\72l(https://x.example/a.png)}' escape
refuse 'nesting selector &' 'html:has(&) header{display:none}' nesting
refuse 'extra closing brace' 'h1{color:red}} header{display:none}{' braces
refuse 'unclosed brace' 'h1{color:red' braces
refuse 'unclosed comment' 'h1{color:red} /* never ends' braces
refuse 'image-set()' 'h1{background-image:image-set("https://x.example/a.png" 1x)}' image
refuse '-webkit-image-set()' 'h1{background-image:-webkit-image-set("https://x.example/a.png" 1x)}' image
refuse '@keyframes (other @-rules)' '@keyframes spin{to{transform:rotate(1turn)}}' at-rule
refuse 'url() still' 'h1{background:url(https://x.example/a.png)}' url
r=$(page $STRANGER /u/pal pal-after.html)
check "refusals kept the good CSS" 'grep -qF "a23b3b" $W/pal-after.html' "good CSS gone"
check "a brace inside a string is fine" '[[ "$(savecss $PAL "h1::after{content:\"}\"}")" == *"ok=css"* ]]' "string brace refused"
check "a brace inside a comment is fine" '[[ "$(savecss $PAL "/* } */ h1{color:red}")" == *"ok=css"* ]]' "comment brace refused"
savecss $PAL "$GOOD" >/dev/null

# stored CSS that passed the OLD rules but fails today's is not rendered; the owner is told on /me
stop
python3 - "$W/store.lgp" <<'PY'
import sys
p = sys.argv[1]; lines = open(p, encoding="utf-8").read().split("\n")
out = []
for l in lines:
    if l.startswith("ac\t"):
        f = l.split("\t"); f[2] = "h1%7Bcolor%3A%5C72%20ed%7D"  # h1{color:\72 ed}: old filter allowed it
        l = "\t".join(f)
    out.append(l)
open(p, "w", encoding="utf-8").write("\n".join(out))
PY
check "precondition: the stored CSS now holds an escape" 'grep -q "^ac.*%5C72" $W/store.lgp' "$(grep '^ac' $W/store.lgp)"
start
r=$(page $STRANGER /u/pal pal-old.html)
check "old-rule CSS: profile still answers 200" '[ "$r" = 200 ]' "$r"
check "old-rule CSS is NOT rendered" '! grep -qF "\\72" $W/pal-old.html' "escaped CSS rendered"
check "old-rule CSS: no scope style emitted" '! grep -q "lg-profile-scope{" $W/pal-old.html' "scope style emitted"
r=$(page $PAL /me me-old.html)
check "owner sees why on /me" 'grep -q "isn.t shown on your profile" $W/me-old.html' "no notice on /me"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
