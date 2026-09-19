#!/usr/bin/env bash
# An ONLINE copy of the live commons.sqlite, made with SQLite's backup API (never `cp` of a live database:
# a copy taken mid-write can be torn). Checks the copy with integrity_check and prints the row counts of every
# table, the baseline that verify.sh --compare-backup compares the live DB against after a restart.
#
# Written to <data dir>/backups/commons.sqlite.pre-<label>-<UTC timestamp> (the same directory the daemon's own
# pre-reap backups use), owned by the service user, mode 600. Retention is deliberately NOT here (its own name).
# The source is opened read-only, so this can never create -wal/-shm files owned by root next to the live DB.
#
# Usage: backup-db.sh [label]      label: letters, digits, . _ - (default "manual")
# Last line on success:  BACKUP <path>
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

label="${1:-manual}"
[[ "$label" =~ ^[A-Za-z0-9._-]{1,40}$ ]] || die "label must be 1-40 of letters, digits, . _ -"
stamp="${DEPLOY_BACKUP_STAMP:-$(date -u +%Y%m%dT%H%M%SZ)}"       # the env var is a test seam (two backups in one second)
dest="$BACKUP_DIR_REMOTE/commons.sqlite.pre-$label-$stamp"

remote "test -f $(q "$DB_REMOTE")" || die "no database at $DB_REMOTE on $HOST (a first install has nothing to back up)"
remote "mkdir -p $(q "$BACKUP_DIR_REMOTE")"
remote "python3 - $(q "$DB_REMOTE") $(q "$dest")" <<'PY'
import os, sqlite3, sys
src, dst = sys.argv[1], sys.argv[2]
if os.path.exists(dst):
    sys.exit("refusing to overwrite an existing backup: " + dst)
s = sqlite3.connect("file:%s?mode=ro" % src, uri=True)
d = sqlite3.connect(dst)
s.backup(d)                       # the online backup API: a consistent snapshot while the daemon runs
d.close(); s.close()
c = sqlite3.connect("file:%s?mode=ro" % dst, uri=True)
ic = c.execute("pragma integrity_check").fetchone()[0]
print("integrity_check:", ic)
for (t,) in c.execute("select name from sqlite_master where type='table' and name not like 'sqlite_%' order by name").fetchall():
    print("count %s %d" % (t, c.execute('select count(*) from "%s"' % t).fetchone()[0]))
if ic != "ok":
    sys.exit("the backup FAILED integrity_check")
PY
remote "chown $(q "$SERVICE_USER:$SERVICE_USER") $(q "$dest") && chmod 600 $(q "$dest")"
say "BACKUP $dest"
