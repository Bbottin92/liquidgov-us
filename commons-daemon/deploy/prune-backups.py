#!/usr/bin/env python3
"""Which database backups may be deleted? A pure decision function plus a tiny CLI; it never touches a file.

Input (stdin): one line per file in the backups directory: "<mtime epoch seconds> <file name>".
Output (stdout), one line per file, tab-separated  VERB <TAB> reason <TAB> name, NEWEST BACKUP FIRST (each backup is
followed by its own sidecars; orphan sidecars and ignored files come last):
    KEEP / DELETE           a backup
    SIDE-KEEP / SIDE-DELETE the -wal / -shm sidecar of a backup, which follows its backup's verdict
    ORPHAN                  a sidecar whose backup file is gone: REPORTED, never deleted
    IGNORE                  anything else in the directory: never counted, never touched
Exit: 0 ok / 3 the clock or the listing looks wrong (nothing may be deleted; see the ANOMALY line).

The policy (locked by Brandon, 2026-09-19; see Persistence/TheCommons/backup-retention-built.md):
  A backup is deleted ONLY if it is BOTH outside the newest KEEP (14) AND older than DAYS (30).
  So the newest 14 stay however old they are, and anything within 30 days stays however many there are.
  Also, always: the single newest backup is never deleted, and no backup younger than FLOOR_HOURS (24h) is
  ever deleted (a hard floor that holds even if --days is overridden below a day).

What counts as a backup: a regular file named  commons.sqlite.pre-<label>-<YYYYmmddTHHMMSSZ>.  That is ONE
naming scheme with two producers: the deploy script (label = a short sha or a word, e.g. pre-a56eba3-...) and the
daemon's own automatic pre-reap backup (label `reap`), plus the hand-made ones (pre-hardening-..., pre-reap-manual-...).
They are ranked and treated identically.

Sidecars: a backup is a WAL-mode database, so opening it creates <backup>-wal and <backup>-shm beside it. Those two
EXACT names belong to that backup: they are deleted when, and only when, the backup itself is deleted, and kept
with it otherwise. They never count toward the newest 14 and are never judged by their own age. A sidecar whose
backup file no longer exists is an ORPHAN: reported, never deleted. (Anything that is not exactly
<backup>-wal or <backup>-shm, e.g. -journal, -wal2, .wal, uppercase, a near-miss name, is IGNOREd.)

Age and rank use the YOUNGER of the timestamp in the name and the file's mtime (max of the two), so a file
only looks old if BOTH say it is: a restored/copied/touched old backup counts as recent and is kept.
"""
import calendar
import re
import sys
import time

KEEP_DEFAULT = 14
DAYS_DEFAULT = 30
FLOOR_HOURS = 24

NAME = re.compile(r"^commons\.sqlite\.pre-([A-Za-z0-9._-]+)-(\d{8}T\d{6}Z)$")
SIDE = re.compile(r"^(commons\.sqlite\.pre-[A-Za-z0-9._-]+-\d{8}T\d{6}Z)-(wal|shm)$")


def name_epoch(stamp):
    return calendar.timegm(time.strptime(stamp, "%Y%m%dT%H%M%SZ"))


def decide(entries, now, keep=KEEP_DEFAULT, days=DAYS_DEFAULT, floor_hours=FLOOR_HOURS):
    """entries: [(mtime_epoch, name)]. Returns (decisions, anomaly) with decisions = [(verb, reason, name)] newest backup
    first, and anomaly = None or a string (then NOTHING is marked DELETE / SIDE-DELETE)."""
    parsed = []                 # (effective epoch, name) of every valid backup
    sidecars = []               # (main name, suffix, sidecar name)
    ignored = {}
    for mtime, name in entries:
        m = NAME.match(name)
        if m:
            try:
                parsed.append((max(name_epoch(m.group(2)), float(mtime)), name))
            except ValueError:
                ignored[name] = ("IGNORE", "unparseable timestamp", name)
            continue
        s = SIDE.match(name)
        if s:
            sidecars.append((s.group(1), s.group(2), name))
        else:
            ignored[name] = ("IGNORE", "not a backup name", name)

    anomaly = None
    if parsed and max(e for e, _ in parsed) > now + 3600:
        anomaly = "a backup is dated more than an hour in the FUTURE: the clock (or a file) is wrong, so nothing is deleted"

    parsed.sort(key=lambda p: (p[0], p[1]), reverse=True)     # newest first; ties broken by name
    main_verdict = {}
    for rank, (eff, name) in enumerate(parsed, 1):            # sidecars are NOT in `parsed`: they never take a rank
        age = now - eff
        if rank == 1:
            v = ("KEEP", "the newest backup")
        elif rank <= keep:
            v = ("KEEP", "newest %d (#%d)" % (keep, rank))
        elif age < floor_hours * 3600:
            v = ("KEEP", "younger than %dh (hard floor)" % floor_hours)
        elif age <= days * 86400:
            v = ("KEEP", "within %d days" % days)
        else:
            v = ("DELETE", "outside the newest %d and older than %d days (%.1f days old)" % (keep, days, age / 86400.0))
        if anomaly and v[0] == "DELETE":
            v = ("KEEP", "held back: " + anomaly.split(":")[0])
        main_verdict[name] = (v[0], v[1], name)

    # pair each sidecar with its backup by EXACT name; it follows that backup's verdict
    paired = {}                 # main name -> [(verb, reason, sidecar name)]
    orphans = []
    for main, suffix, name in sidecars:
        if main in main_verdict:
            verb = main_verdict[main][0]
            item = ("SIDE-DELETE", "%s of %s, which is being deleted" % (suffix, main), name) if verb == "DELETE" \
                else ("SIDE-KEEP", "%s of %s, which is kept" % (suffix, main), name)
            paired.setdefault(main, []).append(item)
        elif main in ignored:
            ignored[name] = ("IGNORE", "its backup name is not valid", name)
        else:
            orphans.append(("ORPHAN", "%s whose backup file is gone: reported, never deleted" % suffix, name))

    ordered = []
    for _, name in parsed:
        ordered.append(main_verdict[name])
        ordered.extend(sorted(paired.get(name, []), key=lambda t: t[2]))
    ordered.extend(sorted(orphans, key=lambda t: t[2]))
    ordered.extend(ignored[n] for n in sorted(ignored))
    return ordered, anomaly


def main(argv):
    keep, days, now = KEEP_DEFAULT, DAYS_DEFAULT, time.time()
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--keep": keep = int(argv[i + 1]); i += 2
        elif a == "--days": days = int(argv[i + 1]); i += 2
        elif a == "--now": now = float(argv[i + 1]); i += 2
        else: sys.exit("unknown argument: " + a)
    entries = []
    for line in sys.stdin:
        line = line.rstrip("\n")
        if not line.strip():
            continue
        mtime, _, name = line.partition(" ")
        entries.append((float(mtime), name))
    decisions, anomaly = decide(entries, now, keep, days)
    for verb, reason, name in decisions:
        print("%s\t%s\t%s" % (verb, reason, name))
    if anomaly:
        print("ANOMALY\t%s\t-" % anomaly)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
