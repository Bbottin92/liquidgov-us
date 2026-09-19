#!/usr/bin/env bash
# Backup retention (deploy/prune-backups.sh + prune-backups.py), against a FAKE droplet: a temp backups directory
# with real files and controlled mtimes behind a shim ssh. Nothing here can reach the real droplet.
#
# Policy under test: delete a backup ONLY if it is BOTH outside the newest 14 AND older than 30 days; the newest 14
# stay however old, anything within 30 days stays however many; never the newest, never younger than 24h; dry run
# by default, only --delete deletes; files that are not backups are never touched; one naming scheme for the
# deploy (`pre-<label>-<stamp>`) and daemon (`pre-reap-<stamp>`) backups.
source "$(dirname "$0")/lib.sh"

FAKE="$WORK/droplet"; DATA="$FAKE/var/lib/commons"; BK="$DATA/backups"; mkdir -p "$BK"
cat > "$WORK/fakessh" <<'EOF'
#!/usr/bin/env bash
shift            # the host
exec bash -c "$*"
EOF
chmod +x "$WORK/fakessh"
export DEPLOY_HOST=fake DEPLOY_SSH="$WORK/fakessh" DEPLOY_DATA_DIR="$DATA"
DAY=86400; NOW="$(date -u -d 2026-09-20T00:00:00Z +%s)"; export DEPLOY_NOW="$NOW"
P="$DAEMON_DIR/deploy/prune-backups.sh"
runs() { OUT="$("$@" 2>&1)"; RC=$?; }
stamp() { date -u -d "@$1" +%Y%m%dT%H%M%SZ; }
# mk AGE_SECONDS LABEL [MTIME_AGE_SECONDS]  -> a backup named for NOW-AGE whose mtime is NOW-MTIME_AGE
mk() { local age=$1 label=$2 mage=${3:-$1} name; name="commons.sqlite.pre-$label-$(stamp $((NOW - age)))"
       printf 'db %s\n' "$name" > "$BK/$name"; touch -d "@$((NOW - mage))" "$BK/$name"; LAST="$name"; }
# side  -> the -wal (empty) and -shm (32K, like the real ones) sidecars of the backup mk just made, same mtime as it
side() { : > "$BK/$LAST-wal"; head -c 32768 /dev/zero > "$BK/$LAST-shm"; touch -r "$BK/$LAST" "$BK/$LAST-wal" "$BK/$LAST-shm"; }
nb() { find "$BK" -maxdepth 1 -type f -name 'commons.sqlite.pre-*-[0-9]*Z' | wc -l; }
ns() { find "$BK" -maxdepth 1 -type f \( -name 'commons.sqlite.pre-*-[0-9]*Z-wal' -o -name 'commons.sqlite.pre-*-[0-9]*Z-shm' \) | wc -l; }
clear_bk() { find "$BK" -mindepth 1 -delete; }
names() { find "$BK" -maxdepth 1 -type f -name 'commons.sqlite.pre-*-[0-9]*Z' -printf '%f\n' | sort; }

echo "-- the decision function (prune_backups_unit.py)"
if ! command -v python3 >/dev/null; then echo "FATAL: python3 missing"; exit 1; fi
unit="$(python3 "$TESTS_DIR/prune_backups_unit.py" 2>&1)"; urc=$?
echo "$unit" | sed 's/^/     /'
assert_eq "no unit case failed" "0" "$urc"
n="$(sed -n 's/^prune_backups_unit: \([0-9]*\) checks.*/\1/p' <<<"$unit")"
[ "${n:-0}" -ge 350 ] && _ok "at least 350 checks executed ($n)" || _bad "at least 350 checks executed" "$n"

echo "-- today's droplet: ten backups, each with its -wal/-shm sidecars, all within two days"
for spec in "1 a56eba3" "4 hardening" "5 reap" "9 orphans" "14 reap-manual" "20 open" "26 rebind" "30 janitor" "38 snapshot" "44 inventory"; do
  set -- $spec; mk $(( $1 * 3600 )) "$2"; side
done
assert_eq "setup: ten backups" "10" "$(nb)"
assert_eq "setup: twenty sidecars" "20" "$(ns)"
runs "$P"
assert_eq "dry run exits 0" "0" "$RC"
assert_contains "nothing to delete" "$OUT" "nothing to delete"
assert_contains "the sidecars are counted apart from the backups (they never take a rank)" "$OUT" "10 backups: keep 10, delete 0; sidecars: keep 20, delete 0"
assert_contains "the policy is printed" "$OUT" "outside the newest 14 AND older than 30 days"
assert_not_contains "no warning at the locked policy" "$OUT" "WARNING"
runs "$P" --delete
assert_eq "--delete exits 0" "0" "$RC"
assert_eq "and deletes nothing" "10" "$(nb)"
assert_eq "not a single sidecar either" "20" "$(ns)"

echo "-- a spread: 34 backups over 100 days, each with -wal/-shm sidecars (deploy and pre-reap names mixed), plus orphans and other files"
clear_bk
for d in $(seq 1 3 100); do if [ $((d % 2)) = 1 ]; then mk $((d * DAY + 1)) reap; else mk $((d * DAY + 1)) "b$(printf %06x $d)"; fi; side; done
OLDEST="$LAST"                                                       # d=100, the oldest: it will be deleted, with its sidecars
old500() { for f in "$@"; do touch -d "@$((NOW - 500 * DAY))" "$BK/$f"; done; }
# orphans: sidecars whose backup file is gone (an ancient pair, and a near-miss of a real backup's name)
GONE=commons.sqlite.pre-gone-20200101T000000Z; NEAR="${OLDEST/T235959Z/T235958Z}"
: > "$BK/$GONE-wal"; : > "$BK/$GONE-shm"; : > "$BK/$NEAR-wal"; old500 "$GONE-wal" "$GONE-shm" "$NEAR-wal"
[ "$NEAR" != "$OLDEST" ] || _bad "setup: the near-miss name differs" "$NEAR"
# not backups, not sidecars: never touched
echo "notes" > "$BK/notes.txt"; echo "tmp" > "$BK/commons.sqlite.pre-x-20200101T000000Z.tmp"; echo "db" > "$BK/commons.sqlite"
: > "$BK/commons.sqlite-wal"; : > "$BK/$OLDEST-wal.bak"
old500 notes.txt commons.sqlite.pre-x-20200101T000000Z.tmp commons.sqlite commons.sqlite-wal "$OLDEST-wal.bak"
mkdir "$BK/commons.sqlite.pre-dir-20200101T000000Z"; touch -d "@$((NOW - 500 * DAY))" "$BK/commons.sqlite.pre-dir-20200101T000000Z"
assert_eq "setup: 34 backups" "34" "$(nb)"
assert_eq "setup: 68 paired sidecars + 3 orphans" "71" "$(ns)"
before="$(find "$BK" -mindepth 1 -maxdepth 1 | wc -l)"
runs "$P"
assert_eq "no flags = DRY RUN, exit 0" "0" "$RC"
assert_contains "it says so" "$OUT" "DRY RUN: nothing was deleted"
assert_contains "it counts backups, sidecars, orphans and ignored files apart" "$OUT" "keep 14, delete 20; sidecars: keep 28, delete 40; orphan sidecars (reported, never deleted): 3; ignore 5 other file(s)"
assert_contains "and what it would free" "$OUT" "frees"
assert_contains "the dry run says how many files it would remove" "$OUT" "delete the 20 backup(s) and 40 sidecar file(s) marked DELETE"
assert_contains "a deletion is explained" "$OUT" "outside the newest 14 and older than 30 days"
assert_contains "orphans are listed as such" "$OUT" "ORPHAN"
assert_contains "and said to be report-only" "$OUT" "They are reported only"
assert_eq "the dry run removed NOTHING (not one file)" "$before" "$(find "$BK" -mindepth 1 -maxdepth 1 | wc -l)"
order="$(awk '$1=="KEEP"||$1=="DELETE"{print $2}' <<<"$OUT" | sed -E 's/.*-([0-9]{8}T[0-9]{6}Z)$/\1/')"
assert_eq "all 34 backups are listed" "34" "$(wc -l <<<"$order")"
assert_eq "NEWEST FIRST: the list is in descending time order" "$(sort -r <<<"$order")" "$order"
assert_eq "every sidecar is listed directly under its own backup" "" "$(awk '$1=="KEEP"||$1=="DELETE"{m=$2} $1 ~ /^SIDE-/{n=$2; sub(/-(wal|shm)$/,"",n); if (n!=m) print "misplaced: " $2}' <<<"$OUT")"
assert_eq "orphans and other files come after every backup" "0" "$(awk '$1=="ORPHAN"||$1=="IGNORE"{s=1} s && ($1=="KEEP"||$1=="DELETE"||$1 ~ /^SIDE-/){b=1} END{print b+0}' <<<"$OUT")"
runs "$P" --delete
assert_eq "--delete exits 0" "0" "$RC"
assert_contains "it reports what it did" "$OUT" "deleted 60 file(s) (20 backup(s) + 40 sidecar(s)); every backup and sidecar marked KEEP is still present, and the orphans were not touched"
assert_eq "exactly the newest 14 backups remain (ages 1..40 days; the 40-day-old ones stay because they are the newest 14)" "14" "$(nb)"
oldest_left="$(names | sed -n 's/.*-\([0-9]\{8\}T[0-9]\{6\}Z\)$/\1/p' | sort | head -1)"
assert_eq "the oldest survivor is the 40-day-old one" "$(stamp $((NOW - 40 * DAY - 1)))" "$oldest_left"
lonely=0; for n in $(names); do [ -e "$BK/$n-wal" ] && [ -e "$BK/$n-shm" ] || lonely=$((lonely + 1)); done
assert_eq "every surviving backup still has BOTH its sidecars" "0" "$lonely"
gone=0
for f in $(find "$BK" -maxdepth 1 -type f \( -name '*-wal' -o -name '*-shm' \) -printf '%f\n'); do
  case "$f" in "$GONE"-*|"$NEAR"-wal|commons.sqlite-wal) continue;; esac       # the known orphans and the non-backup wal
  m="${f%-wal}"; m="${m%-shm}"
  [ -e "$BK/$m" ] || gone=$((gone + 1))
done
assert_eq "no sidecar was left stranded by a deleted backup" "0" "$gone"
[ -e "$BK/$OLDEST" ] || [ -e "$BK/$OLDEST-wal" ] || [ -e "$BK/$OLDEST-shm" ] && r=LEFT || r=gone
assert_eq "the oldest backup and both its sidecars are gone" "gone" "$r"
for f in "$GONE-wal" "$GONE-shm" "$NEAR-wal"; do [ -e "$BK/$f" ] && r=kept || r=DELETED; assert_eq "orphan sidecar untouched: ${f: -30}" "kept" "$r"; done
for f in notes.txt commons.sqlite.pre-x-20200101T000000Z.tmp commons.sqlite commons.sqlite-wal "$OLDEST-wal.bak"; do [ -f "$BK/$f" ] && r=kept || r=DELETED; assert_eq "not a backup or sidecar, untouched: ${f: -32}" "kept" "$r"; done
[ -d "$BK/commons.sqlite.pre-dir-20200101T000000Z" ] && r=kept || r=DELETED
assert_eq "a directory that looks like a backup is untouched" "kept" "$r"
runs "$P" --delete
assert_contains "running it again finds nothing to do" "$OUT" "nothing to delete"
assert_contains "and still reports the orphans" "$OUT" "orphan sidecars (reported, never deleted): 3"
assert_eq "and changes nothing" "14" "$(nb)"
assert_eq "sidecars: 28 paired + 3 orphans" "31" "$(ns)"

echo "-- an old-named backup that was recently touched is kept (age = the younger of name and mtime)"
clear_bk
for i in $(seq 0 13); do mk $((i * 3600 + 3600)) "fresh$i"; side; done               # the newest 14
mk $((200 * DAY)) restored $((10 * DAY)); side                                        # named 200 days ago, copied 10 days ago
mk $((200 * DAY)) reallyold; side                                                      # old by name AND mtime
: > "$BK/$LAST-wal"                                                                    # (a fresh mtime on a doomed backup's sidecar)
runs "$P" --delete
assert_eq "exit 0" "0" "$RC"
[ -n "$(names | grep -- '-restored-')" ] && r=kept || r=DELETED
assert_eq "the touched one survives (outside the newest 14, but it looks 10 days old)" "kept" "$r"
assert_eq "and keeps both sidecars" "2" "$(find "$BK" -name 'commons.sqlite.pre-restored-*-[ws]*' | wc -l)"
[ -n "$(names | grep -- '-reallyold-')" ] && r=kept || r=DELETED
assert_eq "the one that is old by both is deleted" "DELETED" "$r"
assert_eq "with both its sidecars, even though one has a brand-new mtime (a sidecar is not judged by its own age)" "0" "$(find "$BK" -name 'commons.sqlite.pre-reallyold-*' | wc -l)"
assert_eq "the newest 14 (+ the touched one) are all still there" "15" "$(nb)"
assert_eq "with all their sidecars" "30" "$(ns)"

echo "-- 60 backups within 20 days: the 30-day rule beats the count"
clear_bk
for i in $(seq 1 60); do mk $((i * 8 * 3600)) "m$i"; done
runs "$P" --delete
assert_contains "nothing to delete" "$OUT" "nothing to delete"
assert_eq "all 60 remain" "60" "$(nb)"

echo "-- the newest is never deleted, even if it is the only one and ancient"
clear_bk; mk $((400 * DAY)) only
runs "$P" --delete
assert_eq "kept" "1" "$(nb)"

echo "-- a clock or file that looks wrong: nothing is deleted"
clear_bk
for i in $(seq 1 20); do mk $(( (100 + i) * DAY )) "old$i"; side; done
mk $(( -3 * 3600 )) future; side
runs "$P" --delete
assert_eq "an ANOMALY exits 3" "3" "$RC"
assert_contains "and says why" "$OUT" "ANOMALY"
assert_eq "nothing was deleted, even with --delete" "21" "$(nb)"
assert_eq "not a sidecar either" "42" "$(ns)"

echo "-- only orphans to report: nothing is deleted, and it says so"
clear_bk
mk $((2 * 3600)) recent; side
: > "$BK/commons.sqlite.pre-vanished-20190101T000000Z-wal"; : > "$BK/commons.sqlite.pre-vanished-20190101T000000Z-shm"
touch -d "@$((NOW - 900 * DAY))" "$BK/commons.sqlite.pre-vanished-20190101T000000Z-wal" "$BK/commons.sqlite.pre-vanished-20190101T000000Z-shm"
runs "$P" --delete
assert_eq "exit 0" "0" "$RC"
assert_contains "the orphans are reported" "$OUT" "orphan sidecars (reported, never deleted): 2"
assert_contains "nothing to delete" "$OUT" "nothing to delete"
assert_eq "both ancient orphans are still there" "2" "$(find "$BK" -name 'commons.sqlite.pre-vanished-*' | wc -l)"
assert_eq "as are the backup and its pair" "1" "$(nb)"

echo "-- a listing that fails is an error, never 'empty'"
runs env DEPLOY_DATA_DIR="$WORK/nonexistent" "$P" --delete
assert_eq "a missing backups directory exits 1" "1" "$RC"
assert_contains "and says nothing was deleted" "$OUT" "nothing was deleted"

echo "-- overrides and bad flags"
clear_bk
for d in $(seq 1 3 100); do mk $((d * DAY + 1)) "s$d"; done
runs "$P" --keep 3 --days 7
assert_contains "a more aggressive policy prints a WARNING" "$OUT" "more aggressive than the locked policy"
assert_contains "and applies it (keep 3 + those within 7 days)" "$OUT" "delete 31"
assert_eq "still a dry run" "34" "$(nb)"
runs "$P" --keep abc
assert_eq "a non-numeric --keep is refused" "1" "$RC"
runs "$P" --days -5
assert_eq "a negative --days is refused" "1" "$RC"
runs "$P" --frobnicate
assert_eq "an unknown flag is refused" "1" "$RC"
runs "$P" --keep 0 --days 0 --delete
assert_eq "--keep 0 --days 0 --delete still exits 0" "0" "$RC"
assert_eq "yet the newest backup survives, and so does every one younger than 24h" "1" "$(nb)"
