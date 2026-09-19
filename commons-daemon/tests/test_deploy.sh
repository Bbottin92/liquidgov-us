#!/usr/bin/env bash
# The deploy scripts (commons-daemon/deploy/), driven against a FAKE droplet: a temp directory behind a shim
# "ssh" (it runs the remote command locally) with stub systemctl/journalctl/cmake/chown on PATH. The scripts run
# from a throwaway git repo (a copy of deploy/ plus a tiny fake tree), so the real repo is never touched and
# nothing here can reach the production droplet. Verify talks to the real test daemon over loopback.
#
# Pinned: drift is caught and blocks; a dirty or unpushed tree is refused; nothing sensitive is synced; the
# marker records what is live; backups are online, integrity-checked and never overwrite; the build is -j1 with a
# reconfigure only when CMake changed; restart comes after build; a failed backup / drift / civic change stops
# BEFORE anything changes; verify catches errors, inactive services and lost rows.
source "$(dirname "$0")/lib.sh"
start_civic; start_daemon

R="$WORK/repo"; FAKE="$WORK/droplet"; BIN_STUBS="$WORK/bin"; CALLS="$WORK/calls.log"; JOURNAL="$WORK/journal.txt"
mkdir -p "$R" "$FAKE/opt/commons-daemon/build" "$FAKE/opt/commons" "$FAKE/var/lib/commons" "$BIN_STUBS"
: > "$CALLS"
echo "commons-daemon: listening on 127.0.0.1:8765" > "$JOURNAL"
touch "$FAKE/opt/commons-daemon/build/CMakeCache.txt"

# ---- the shim ssh, and stubs for what only exists on the droplet
cat > "$WORK/fakessh" <<'EOF'
#!/usr/bin/env bash
shift            # the host
exec bash -c "$*"
EOF
cat > "$BIN_STUBS/systemctl" <<EOF
#!/usr/bin/env bash
echo "systemctl \$*" >> "$CALLS"
case "\$1" in
  is-active) [ -e "$WORK/inactive.\$2" ] && echo inactive || echo active;;
  show) case "\$3\$2" in
          *NRestarts*) echo 0;;
          *ActiveEnterTimestamp*) echo "Fri 2026-09-19 12:55:33 UTC";;
        esac;;
esac
exit 0
EOF
cat > "$BIN_STUBS/journalctl" <<EOF
#!/usr/bin/env bash
cat "$JOURNAL"
EOF
cat > "$BIN_STUBS/cmake" <<EOF
#!/usr/bin/env bash
echo "cmake \$*" >> "$CALLS"
exit 0
EOF
cat > "$BIN_STUBS/chown" <<EOF
#!/usr/bin/env bash
echo "chown \$*" >> "$CALLS"
exit 0
EOF
chmod +x "$WORK/fakessh" "$BIN_STUBS"/*
export PATH="$BIN_STUBS:$PATH"
export DEPLOY_HOST=fake DEPLOY_SSH="$WORK/fakessh"
export DEPLOY_DAEMON_DIR="$FAKE/opt/commons-daemon" DEPLOY_CLIENT_DIR="$FAKE/opt/commons" DEPLOY_DATA_DIR="$FAKE/var/lib/commons"
export DEPLOY_SERVICE_USER="$(id -un)" DEPLOY_PUBLIC_BASE="http://127.0.0.1:8765" DEPLOY_PREFIX="" DEPLOY_SKIP_SITE=1
DD="$FAKE/opt/commons-daemon"; CD="$FAKE/opt/commons"

# ---- a throwaway repo shaped like the real one
g() { git -C "$R" "$@"; }
mkdir -p "$R/commons-daemon/src" "$R/commons-daemon/tests" "$R/commons-daemon/data" "$R/commons/x" "$R/Persistence/TheCommons" "$R/liquidgov-platform/src"
cp -r "$DAEMON_DIR/deploy" "$R/commons-daemon/deploy"
echo "cmake_minimum_required(VERSION 3.10)" > "$R/commons-daemon/CMakeLists.txt"
echo "v1" > "$R/commons-daemon/src/main.cpp"
echo "a test" > "$R/commons-daemon/tests/t.sh"
echo "create table t(x);" > "$R/Persistence/TheCommons/schema.sql"
ln -s ../Persistence/TheCommons/schema.sql "$R/commons-daemon/schema.sql"
echo "client v1" > "$R/commons/index.html"
echo "civic v1" > "$R/liquidgov-platform/src/http_server.cpp"
printf 'build/\ndata/*.sqlite\n' > "$R/.gitignore"
echo "live data that must never leave" > "$R/commons-daemon/data/junk.sqlite"       # ignored, must not be synced
git init -q -b master "$R"; g config user.email t@t; g config user.name t; g config commit.gpgsign false
git init -q --bare "$WORK/origin.git"; g remote add origin "$WORK/origin.git"
g add -A; g commit -q -m v1; g push -q -u origin master
BASE1="$(g rev-parse HEAD)"
S="$R/commons-daemon/deploy"
commit_push() { g add -A; g commit -q -m "$1"; g push -q origin master; }
runs() { OUT="$("$@" 2>&1)"; RC=$?; }
sum() { sha256sum "$1" | cut -d' ' -f1; }
mkdb() { python3 - "$1" <<'PY'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1])
c.execute("create table players(n text)"); c.execute("create table placed(n text)")
c.executemany("insert into players values(?)", [("a",), ("b",), ("c",)]); c.execute("insert into placed values('w')")
c.commit()
PY
}
mkdb "$FAKE/var/lib/commons/commons.sqlite"

echo "-- first use: no marker on the droplet means we cannot tell what is live"
runs "$S/drift-check.sh"
assert_eq "drift-check with no marker and no --baseline exits 2" "2" "$RC"
assert_contains "and says what to do" "$OUT" "--baseline"

echo "-- sync: what ships and what never does"
runs "$S/sync-to-droplet.sh"
assert_eq "sync ok" "0" "$RC"
assert_eq "source landed" "v1" "$(cat "$DD/src/main.cpp" 2>/dev/null)"
[ -L "$DD/schema.sql" ] && r=symlink || r=file
assert_eq "schema.sql is the real file on the droplet, not a dangling symlink" "file" "$r"
assert_eq "with the spec's content" "create table t(x);" "$(cat "$DD/schema.sql")"
[ -e "$DD/tests" ] && r=shipped || r=absent;             assert_eq "tests/ is not shipped" "absent" "$r"
[ -e "$DD/data/junk.sqlite" ] && r=shipped || r=absent;  assert_eq "no sqlite file is ever shipped" "absent" "$r"
[ -e "$DD/.git" ] && r=shipped || r=absent;              assert_eq ".git is not shipped" "absent" "$r"
assert_eq "the marker holds the deployed revision" "$BASE1" "$(awk 'NR==1{print $1}' "$DD/.deployed-rev")"
assert_contains "and the deploy log has a line" "$(cat "$DD/.deploy-log")" "$BASE1"
runs "$S/sync-client-to-droplet.sh"
assert_eq "client sync ok" "0" "$RC"
assert_eq "client landed" "client v1" "$(cat "$CD/index.html" 2>/dev/null)"
[ -e "$CD/.deployed-client-rev" ] && r=in-served-dir || r=not-served
assert_eq "the client marker is NOT in the nginx-served directory" "not-served" "$r"
assert_eq "it is beside the daemon's marker" "$BASE1" "$(awk 'NR==1{print $1}' "$DD/.deployed-client-rev")"
assert_contains "the client dir was chowned to the service user" "$(cat "$CALLS")" "chown -R $(id -un):$(id -un) $CD"

echo "-- drift check"
runs "$S/drift-check.sh"
assert_eq "clean: exits 0 using the droplet's own marker" "0" "$RC"
assert_contains "and says so" "$OUT" "no drift"
runs "$S/drift-check.sh" --baseline "$BASE1"
assert_eq "an explicit --baseline works too" "0" "$RC"
echo "v2" > "$R/commons-daemon/src/main.cpp"; echo "changed test" > "$R/commons-daemon/tests/t.sh"; echo "client v2" > "$R/commons/index.html"
commit_push v2
runs "$S/drift-check.sh"
assert_eq "a committed change is a plan, not drift: exit 0" "0" "$RC"
assert_contains "the plan lists the daemon file that will change" "$OUT" "src/main.cpp"
assert_contains "and the client file" "$OUT" "index.html"
assert_not_contains "but never tests/ (not shipped)" "$OUT" "t.sh"
echo "// hand edit on the box" >> "$DD/src/main.cpp"
runs "$S/drift-check.sh"
assert_eq "an edit made on the droplet is DRIFT: exit 1" "1" "$RC"
assert_contains "named" "$OUT" "DRIFT"
assert_contains "with the file" "$OUT" "src/main.cpp"
assert_contains "and the verdict" "$OUT" "do not sync"
printf 'v1\n' > "$DD/src/main.cpp"
echo "stray" > "$DD/src/extra.rej"
runs "$S/drift-check.sh"
assert_eq "a droplet-only file is reported but is not drift" "0" "$RC"
assert_contains "listed under droplet-only" "$OUT" "src/extra.rej"
rm -f "$DD/src/extra.rej"
rm -f "$DD/src/main.cpp"
runs "$S/drift-check.sh"
assert_eq "a file that was DELETED on the droplet is drift too" "1" "$RC"
printf 'v1\n' > "$DD/src/main.cpp"

echo "-- a sync ships the working tree, so the tree must be HEAD"
echo "uncommitted" >> "$R/commons-daemon/src/main.cpp"
runs "$S/drift-check.sh"
assert_eq "a modified tracked file refuses: exit 1" "1" "$RC"
assert_contains "and says why" "$OUT" "differ from HEAD"
runs "$S/sync-to-droplet.sh"
assert_eq "sync refuses a dirty tree" "1" "$RC"
assert_eq "and changed nothing on the droplet" "v1" "$(cat "$DD/src/main.cpp")"
runs "$S/sync-to-droplet.sh" -n
assert_eq "a dry run is allowed on a dirty tree" "0" "$RC"
assert_eq "and still changes nothing" "v1" "$(cat "$DD/src/main.cpp")"
runs "$S/drift-check.sh" --allow-dirty
assert_eq "--allow-dirty is an explicit override" "0" "$RC"
g checkout -q -- commons-daemon/src/main.cpp
echo "new" > "$R/commons-daemon/src/new_untracked.cpp"
runs "$S/drift-check.sh"
assert_eq "an UNTRACKED file in the synced tree refuses too (it would be shipped)" "1" "$RC"
rm -f "$R/commons-daemon/src/new_untracked.cpp"
echo "scratch" > "$R/commons-daemon/tests/scratch.tmp"
runs "$S/drift-check.sh"
assert_eq "an untracked file in tests/ does not (tests/ is not shipped)" "0" "$RC"
rm -f "$R/commons-daemon/tests/scratch.tmp"
echo "unpushed" > "$R/commons-daemon/src/unpushed.cpp"; g add -A; g commit -q -m unpushed
runs "$S/drift-check.sh"
assert_eq "a commit that is not pushed refuses: exit 1" "1" "$RC"
assert_contains "and says so" "$OUT" "ahead"
runs "$S/drift-check.sh" --allow-unpushed
assert_eq "--allow-unpushed overrides" "0" "$RC"
g push -q origin master
rm -f "$DD/src/unpushed.cpp"

echo "-- backup: online, checked, never overwriting"
runs "$S/backup-db.sh" "pre test"
assert_eq "a bad label is refused" "1" "$RC"
runs "$S/backup-db.sh" t1
assert_eq "backup ok" "0" "$RC"
assert_contains "integrity_check ok is reported" "$OUT" "integrity_check: ok"
assert_contains "row counts are printed (players)" "$OUT" "count players 3"
assert_contains "(placed)" "$OUT" "count placed 1"
B1="$(sed -n 's/^BACKUP //p' <<<"$OUT" | tail -1)"
assert_contains "it lands in the backups directory, named for the label" "$B1" "/backups/commons.sqlite.pre-t1-"
[ -f "$B1" ] && r=exists || r=missing; assert_eq "the file exists" "exists" "$r"
assert_eq "it is a valid database with the same rows" "3/1" "$(python3 -c "import sqlite3,sys;c=sqlite3.connect(sys.argv[1]);print('%d/%d'%(c.execute('select count(*) from players').fetchone()[0],c.execute('select count(*) from placed').fetchone()[0]))" "$B1")"
assert_eq "mode 600" "600" "$(stat -c %a "$B1")"
assert_eq "the live database was not touched" "3" "$(python3 -c "import sqlite3,sys;print(sqlite3.connect(sys.argv[1]).execute('select count(*) from players').fetchone()[0])" "$FAKE/var/lib/commons/commons.sqlite")"
[ -e "$FAKE/var/lib/commons/commons.sqlite-wal" ] && r=created || r=none
assert_eq "no -wal/-shm files were created beside the live DB by the backup" "none" "$r"
DEPLOY_BACKUP_STAMP=20260101T000000Z runs "$S/backup-db.sh" same
assert_eq "a first backup under a fixed name works" "0" "$RC"
first="$(sum "$FAKE/var/lib/commons/backups/commons.sqlite.pre-same-20260101T000000Z")"
python3 -c "import sqlite3,sys;c=sqlite3.connect(sys.argv[1]);c.execute(\"insert into placed values('later')\");c.commit()" "$FAKE/var/lib/commons/commons.sqlite"
DEPLOY_BACKUP_STAMP=20260101T000000Z runs "$S/backup-db.sh" same
assert_eq "the same name again REFUSES (a backup is never overwritten)" "1" "$RC"
assert_contains "and says why" "$OUT" "refusing to overwrite"
assert_eq "the earlier backup is byte-identical" "$first" "$(sum "$FAKE/var/lib/commons/backups/commons.sqlite.pre-same-20260101T000000Z")"
python3 -c "import sqlite3,sys;c=sqlite3.connect(sys.argv[1]);c.execute(\"delete from placed where n='later'\");c.commit()" "$FAKE/var/lib/commons/commons.sqlite"
mv "$FAKE/var/lib/commons/commons.sqlite" "$WORK/held.sqlite"
runs "$S/backup-db.sh" t2
assert_eq "no database: the backup fails (it does not invent an empty one)" "1" "$RC"
mv "$WORK/held.sqlite" "$FAKE/var/lib/commons/commons.sqlite"

echo "-- build: -j1, and a reconfigure only when asked"
: > "$CALLS"
runs "$S/build-on-droplet.sh"
assert_eq "build ok" "0" "$RC"
assert_contains "cmake --build with -j1" "$(cat "$CALLS")" "--build $DD/build -j1"
assert_not_contains "and NO configure step" "$(cat "$CALLS")" "-S "
: > "$CALLS"
runs "$S/build-on-droplet.sh" --reconfigure
assert_contains "--reconfigure configures first" "$(cat "$CALLS")" "-S $DD -B $DD/build"
mv "$DD/build/CMakeCache.txt" "$WORK/cc.txt"
runs "$S/build-on-droplet.sh"
assert_eq "an unconfigured build dir refuses unless --reconfigure" "1" "$RC"
mv "$WORK/cc.txt" "$DD/build/CMakeCache.txt"

echo "-- verify"
runs "$S/verify.sh"
assert_eq "verify passes against the running daemon" "0" "$RC"
assert_contains "the guest lifecycle ran" "$OUT" "the guest is gone after leave"
assert_contains "a bad secret was checked" "$OUT" "a bad secret is 403"
runs "$S/verify.sh" --guest-name x
assert_eq "a guest named x joins and moves (the reqjson fix, through the real daemon)" "0" "$RC"
assert_contains "under its own name" "$OUT" "at the coordinates sent, under its name"
runs "$S/verify.sh" --guest-name "bad name!"
assert_eq "a bad guest name is refused" "1" "$RC"
runs "$S/verify.sh" --compare-backup "$B1"
assert_eq "counts equal to the backup pass" "0" "$RC"
assert_contains "and are reported" "$OUT" "table players: 3 rows, unchanged"
python3 -c "import sqlite3,sys;c=sqlite3.connect(sys.argv[1]);c.execute(\"insert into players values('d')\");c.commit()" "$FAKE/var/lib/commons/commons.sqlite"
runs "$S/verify.sh" --compare-backup "$B1"
assert_eq "MORE rows than the backup is normal" "0" "$RC"
assert_contains "reported as grew" "$OUT" "3 -> 4 (grew)"
python3 -c "import sqlite3,sys;c=sqlite3.connect(sys.argv[1]);c.execute(\"delete from players\");c.commit()" "$FAKE/var/lib/commons/commons.sqlite"
runs "$S/verify.sh" --compare-backup "$B1"
assert_eq "FEWER rows than the backup fails the verify" "1" "$RC"
assert_contains "naming the table" "$OUT" "table players lost rows"
python3 -c "import sqlite3,sys;c=sqlite3.connect(sys.argv[1]);c.executemany(\"insert into players values(?)\",[('a',),('b',),('c',)]);c.commit()" "$FAKE/var/lib/commons/commons.sqlite"
echo "commons-daemon: terminate called after throwing an instance of 'std::runtime_error'" >> "$JOURNAL"
runs "$S/verify.sh"
assert_eq "an error line in the journal fails the verify" "1" "$RC"
assert_contains "and shows it" "$OUT" "terminate called"
echo "commons-daemon: listening on 127.0.0.1:8765" > "$JOURNAL"
touch "$WORK/inactive.commons-daemon"
runs "$S/verify.sh"
assert_eq "an inactive service fails the verify" "1" "$RC"
rm -f "$WORK/inactive.commons-daemon"
touch "$WORK/inactive.liquidgov"
runs "$S/verify.sh"
assert_eq "civic being down fails the verify too" "1" "$RC"
rm -f "$WORK/inactive.liquidgov"
: > "$JOURNAL"
runs "$S/verify.sh"
assert_eq "no 'listening' line in the journal fails the verify" "1" "$RC"
echo "commons-daemon: listening on 127.0.0.1:8765" > "$JOURNAL"

echo "-- deploy-daemon.sh: plan only without --go"
: > "$CALLS"; before="$(sum "$DD/src/main.cpp")"; nb="$(ls "$FAKE/var/lib/commons/backups" | wc -l)"
runs "$S/deploy-daemon.sh"
assert_eq "the plan runs and exits 0" "0" "$RC"
assert_contains "it says it is a plan" "$OUT" "Plan only"
assert_contains "it shows the steps" "$OUT" "5 restart"
assert_eq "the droplet source is untouched" "$before" "$(sum "$DD/src/main.cpp")"
assert_eq "no new backup" "$nb" "$(ls "$FAKE/var/lib/commons/backups" | wc -l)"
assert_not_contains "nothing was restarted" "$(cat "$CALLS")" "restart"
assert_not_contains "nothing was built" "$(cat "$CALLS")" "cmake"

echo "-- a CMake change is planned with a reconfigure; a civic change blocks --go"
echo "# changed" >> "$R/commons-daemon/CMakeLists.txt"; commit_push cmake
runs "$S/deploy-daemon.sh"
assert_contains "the plan says WITH a reconfigure" "$OUT" "WITH a reconfigure"
echo "civic v2" > "$R/liquidgov-platform/src/http_server.cpp"; commit_push civic
runs "$S/deploy-daemon.sh"
assert_contains "the plan warns civic changed" "$OUT" "CIVIC CHANGED"
: > "$CALLS"; before="$(sum "$DD/src/main.cpp")"
runs "$S/deploy-daemon.sh" --go
assert_eq "--go refuses while civic is undeployed" "1" "$RC"
assert_contains "naming --civic-done" "$OUT" "--civic-done"
assert_eq "and nothing changed" "$before" "$(sum "$DD/src/main.cpp")"
assert_not_contains "no restart" "$(cat "$CALLS")" "restart"

echo "-- drift stops --go BEFORE anything changes"
echo "// hand edit" >> "$DD/src/main.cpp"; before="$(sum "$DD/src/main.cpp")"; nb="$(ls "$FAKE/var/lib/commons/backups" | wc -l)"; : > "$CALLS"
runs "$S/deploy-daemon.sh" --go --civic-done
assert_eq "drift: exit 1" "1" "$RC"
assert_contains "it stopped at the drift check" "$OUT" "did not pass"
assert_eq "the hand edit was NOT overwritten" "$before" "$(sum "$DD/src/main.cpp")"
assert_eq "no backup was taken" "$nb" "$(ls "$FAKE/var/lib/commons/backups" | wc -l)"
assert_not_contains "no restart" "$(cat "$CALLS")" "restart"
printf 'v1\n' > "$DD/src/main.cpp"

echo "-- a failed backup stops --go before the sync"
mv "$FAKE/var/lib/commons/commons.sqlite" "$WORK/held.sqlite"; before="$(sum "$DD/src/main.cpp")"; : > "$CALLS"
runs "$S/deploy-daemon.sh" --go --civic-done
assert_eq "no database to back up: exit 1" "1" "$RC"
assert_contains "it says nothing was synced" "$OUT" "nothing was synced"
assert_eq "the droplet source is unchanged" "$before" "$(sum "$DD/src/main.cpp")"
assert_not_contains "no restart" "$(cat "$CALLS")" "restart"
mv "$WORK/held.sqlite" "$FAKE/var/lib/commons/commons.sqlite"

echo "-- the full run, in order"
: > "$CALLS"; nb="$(ls "$FAKE/var/lib/commons/backups" | wc -l)"
runs "$S/deploy-daemon.sh" --go --civic-done --with-client --guest-name x --label full
assert_eq "--go completes: exit 0" "0" "$RC"
assert_eq "the daemon source on the droplet is the new revision" "v2" "$(cat "$DD/src/main.cpp")"
assert_eq "the marker is HEAD" "$(g rev-parse HEAD)" "$(awk 'NR==1{print $1}' "$DD/.deployed-rev")"
assert_eq "the client is deployed (last)" "client v2" "$(cat "$CD/index.html")"
assert_eq "one new backup, named for the label" "1" "$(( $(ls "$FAKE/var/lib/commons/backups" | wc -l) - nb ))"
assert_contains "label in the name" "$(ls "$FAKE/var/lib/commons/backups")" "pre-full-"
assert_contains "the CMake change triggered a reconfigure" "$(cat "$CALLS")" "-S $DD -B $DD/build"
assert_contains "the build was -j1" "$(cat "$CALLS")" "--build $DD/build -j1"
assert_contains "the service was restarted" "$(cat "$CALLS")" "systemctl restart commons-daemon"
bl="$(grep -n 'cmake --build' "$CALLS" | head -1 | cut -d: -f1)"; rl="$(grep -n 'systemctl restart' "$CALLS" | head -1 | cut -d: -f1)"
[ -n "$bl" ] && [ -n "$rl" ] && [ "$bl" -lt "$rl" ] && r=build-then-restart || r="build=$bl restart=$rl"
assert_eq "restart comes AFTER the build" "build-then-restart" "$r"
assert_contains "verify ran with the backup comparison" "$OUT" "the database against the pre-deploy backup"
assert_contains "and passed" "$OUT" "== deployed"
assert_contains "the guest named x was used" "$OUT" "a real guest, named 'x'"

echo "-- a failing verify fails the deploy and says how to roll back"
echo "v3" > "$R/commons-daemon/src/main.cpp"; commit_push v3
echo "commons-daemon: Error: something broke" >> "$JOURNAL"
runs "$S/deploy-daemon.sh" --go --civic-done
assert_eq "verify failed: the deploy exits 1" "1" "$RC"
assert_contains "it says so" "$OUT" "VERIFY FAILED"
assert_contains "and how to roll back" "$OUT" "Roll back"

get "$D/health"; assert_code "the test daemon was never disturbed" 200
