#!/usr/bin/env bash
# www.liquidgov.us redirects to the apex (BUILD www apex): deploy/nginx-www-apex.py, the edit applied to the live nginx site config.
#   part 1  the transform, on a copy of the REAL live config (tests/fixtures/nginx-liquidgov.live.conf): exactly the two intended edits, everything
#           else untouched, idempotent, and it REFUSES a config of another shape instead of guessing. python3 only.
#   part 2  what a real nginx does with it (tests/nginx_www_behavior.sh): the control (the untransformed config serves www as a second site) and the
#           redirects. In CI nginx is installed (a missing nginx there is a FAILURE, not a skip).
source "$(dirname "$0")/lib.sh"

PYT="$(command -v python3 || true)"
if [ -z "$PYT" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "python3 is installed in CI" "no python3"; else echo "  skip: no python3 here"; fi
  exit 0
fi
XF="$REPO/deploy/nginx-www-apex.py"; FIX="$TESTS_DIR/fixtures/nginx-liquidgov.live.conf"; ZONE="$DAEMON_DIR/deploy/nginx-commons-http.conf"
OUT="$WORK/www.new.conf"

echo "-- the transform, on the live-shaped config"
"$PYT" "$XF" --check "$FIX"; assert_eq "the live shape is recognised as NOT applied (--check exits 1)" "1" "$?"
"$PYT" "$XF" "$FIX" > "$OUT"; assert_eq "the transform succeeds" "0" "$?"
"$PYT" "$XF" --check "$OUT"; assert_eq "and its result is recognised as applied (--check exits 0)" "0" "$?"
c() { grep -cF -- "$1" "$2"; }
assert_eq "exactly two redirects to https://liquidgov.us\$request_uri (443 block, port-80 block)" "2" "$(c 'return 301 https://liquidgov.us$request_uri;' "$OUT")"
assert_eq "exactly two 'if (\$host = www.liquidgov.us)' (one per block)" "2" "$(c 'if ($host = www.liquidgov.us)' "$OUT")"
assert_eq "no www redirect is left that keeps the www host (\$host)" "0" "$(awk '/if \(\$host = www.liquidgov.us\)/{f=1;next} f&&/return 301/{print; f=0}' "$OUT" | grep -cF '$host')"
assert_eq "certbot's apex redirect is untouched" "1" "$(c 'if ($host = liquidgov.us) {' "$OUT")"
assert_eq "server_name still names BOTH hosts in both blocks (the certificate match and certbot are unchanged)" "2" "$(c 'server_name liquidgov.us www.liquidgov.us;' "$OUT")"
for k in 'location ' 'listen ' 'ssl_certificate' 'proxy_pass' 'managed by Certbot' 'limit_req' 'alias '; do
  assert_eq "the number of '$k' lines is unchanged$([ "$k" = 'managed by Certbot' ] && echo ' (the ONE certbot marker on the edited www line is gone)')" \
    "$([ "$k" = 'managed by Certbot' ] && echo $(( $(c "$k" "$FIX") - 1 )) || c "$k" "$FIX")" "$(c "$k" "$OUT")"
done
added="$(diff "$FIX" "$OUT" | grep -c '^>')"; removed="$(diff "$FIX" "$OUT" | grep -c '^<')"
assert_eq "the diff is 8 added lines and 2 removed (nothing else moved)" "8 2" "$added $removed"
first_if="$(grep -nF 'if ($host = www.liquidgov.us)' "$OUT" | head -1 | cut -d: -f1)"; first_loc="$(grep -nE '^\s*location ' "$OUT" | head -1 | cut -d: -f1)"
[ "${first_if:-999}" -lt "${first_loc:-0}" ] && _ok "the 443 redirect sits above the first location" || _bad "the 443 redirect sits above the first location" "if at $first_if, first location at $first_loc"
"$PYT" "$XF" "$OUT" > "$WORK/www.twice.conf"; cmp -s "$OUT" "$WORK/www.twice.conf" && _ok "idempotent: transforming the result changes nothing" || _bad "idempotent" "the second pass differs"
cp "$FIX" "$WORK/www.write.conf"; "$PYT" "$XF" --write "$WORK/www.write.conf" >/dev/null; cmp -s "$OUT" "$WORK/www.write.conf" && _ok "--write edits in place to exactly the same result" || _bad "--write" "differs"
"$PYT" "$XF" --write "$WORK/www.write.conf" | grep -q 'already applied' && _ok "--write on an applied file says so and changes nothing" || _bad "--write idempotent" ""

echo "-- it refuses a config it does not know (exit 2, no output config)"
refuse() { local name="$1"; "$PYT" "$XF" "$2" > "$WORK/refuse.out" 2> "$WORK/refuse.err"; local rc=$?
  assert_eq "$name: exits 2" "2" "$rc"; assert_contains "$name: says REFUSED" "$(cat "$WORK/refuse.err")" "REFUSED"; assert_eq "$name: prints no config" "0" "$(wc -c < "$WORK/refuse.out" | tr -d ' ')"; }
printf 'server {\n    listen 443 ssl;\n    server_name liquidgov.us www.liquidgov.us;\n}\n' > "$WORK/r1.conf"; refuse "no port-80 block" "$WORK/r1.conf"
{ cat "$FIX"; echo; awk '/^server \{/{n++} n==1' "$FIX"; } > "$WORK/r2.conf"       # (the live file has no trailing newline)
assert_eq "precondition: that config really has two 443 blocks naming both hosts" "2" "$(awk '/^server \{/{b=""} {b=b $0 "\n"} /^\}/{ if (b ~ /listen 443/ && b ~ /server_name liquidgov.us www.liquidgov.us;/) c++ } END{print c+0}' "$WORK/r2.conf")"
refuse "two 443 blocks naming both hosts" "$WORK/r2.conf"
sed 's/server_name liquidgov.us www.liquidgov.us;/server_name liquidgov.us;/' "$FIX" > "$WORK/r3.conf"; refuse "no block names both hosts" "$WORK/r3.conf"
"$PYT" - "$OUT" "$WORK/r4.conf" <<'PY'
import sys                                    # half applied: keep the 443 redirect, restore certbot's original port-80 www redirect
s = open(sys.argv[1]).read()
s = s.replace("""    # BUILD www apex: straight to the canonical apex host (one hop, not https://www first).
    if ($host = www.liquidgov.us) {
        return 301 https://liquidgov.us$request_uri;
    }
""", """    if ($host = www.liquidgov.us) {
        return 301 https://$host$request_uri;
    } # managed by Certbot
""")
open(sys.argv[2], "w").write(s)
PY
refuse "half applied (443 done, port 80 not)" "$WORK/r4.conf"
"$PYT" "$XF" --check "$WORK/r1.conf"; assert_eq "--check on an unknown shape is 'not applied' (1), not a crash" "1" "$?"

echo "-- what a real nginx does (nginx_www_behavior.sh)"
NGINX="$(command -v nginx || true)"; [ -n "$NGINX" ] || { [ -x /usr/sbin/nginx ] && NGINX=/usr/sbin/nginx; }
if [ -z "$NGINX" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "nginx is installed in CI" "no nginx binary"; else echo "  skip: no nginx binary here (the behavioural half runs in CI)"; fi
  exit 0
fi
command -v openssl >/dev/null || { if [ "${CI:-}" = "true" ]; then _bad "openssl is installed in CI" ""; else echo "  skip: no openssl"; fi; exit 0; }
bash "$TESTS_DIR/nginx_www_behavior.sh" "$NGINX" "$FIX" "$OUT" "$ZONE" "$WORK/wwwng" > "$WORK/behavior.out" 2>&1; rc=$?
sed 's/^/     /' "$WORK/behavior.out"
assert_eq "no behaviour check failed (exit code)" "0" "$rc"
assert_contains "and its own summary says 0 failed" "$(cat "$WORK/behavior.out")" " checks, 0 failed"
b="$(sed -n 's/^behavior: \([0-9]*\) checks.*/\1/p' "$WORK/behavior.out")"
[ "${b:-0}" -ge 15 ] && _ok "at least 15 behaviour checks executed ($b)" || _bad "at least 15 behaviour checks executed" "$b"
