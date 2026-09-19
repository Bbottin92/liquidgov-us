#!/usr/bin/env python3
"""Which database backups may be deleted? A pure decision function plus a tiny CLI; it never touches a file.

Input (stdin): one line per file in the backups directory: "<mtime epoch seconds> <file name>".
Output (stdout): one line per file:  KEEP <reason> <name>  |  DELETE <reason> <name>  |  IGNORE <reason> <name>
Exit: 0 ok / 3 the clock or the listing looks wrong (nothing may be deleted; see ANOMALY line).

The policy (locked by Brandon, 2026-09-19; see Persistence/TheCommons/backup-retention-built.md):
  A backup is deleted ONLY if it is BOTH outside the newest KEEP (14) AND older than DAYS (30).
  So the newest 14 stay however old they are, and anything within 30 days stays however many there are.
  Also, always: the single newest backup is never deleted, and no backup younger than FLOOR_HOURS (24h) is
  ever deleted (a hard floor that holds even if --days is overridden below a day).

What counts as a backup: a regular file named  commons.sqlite.pre-<label>-<YYYYmmddTHHMMSSZ>.  That is ONE
naming scheme with two producers: the deploy script (label = a short sha or a word, e.g. pre-a56eba3-...) and the
daemon's own automatic pre-reap backup (label `reap`), plus the hand-made ones (pre-hardening-..., pre-reap-manual-...).
They are ranked and treated identically. Anything else in the directory (other names, -wal/-shm, temp files) is
IGNOREd: never counted, never deleted.

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


def name_epoch(stamp):
    return calendar.timegm(time.strptime(stamp, "%Y%m%dT%H%M%SZ"))


def decide(entries, now, keep=KEEP_DEFAULT, days=DAYS_DEFAULT, floor_hours=FLOOR_HOURS):
    """entries: [(mtime_epoch, name)]. Returns (decisions, anomaly) with decisions = [(verb, reason, name)]
    in the input order, and anomaly = None or a string (then NOTHING is marked DELETE)."""
    parsed = []
    out = {}
    for mtime, name in entries:
        m = NAME.match(name)
        if not m:
            out[name] = ("IGNORE", "not a backup name", name)
            continue
        try:
            eff = max(name_epoch(m.group(2)), float(mtime))
        except ValueError:
            out[name] = ("IGNORE", "unparseable timestamp", name)
            continue
        parsed.append((eff, name))

    anomaly = None
    if parsed and max(e for e, _ in parsed) > now + 3600:
        anomaly = "a backup is dated more than an hour in the FUTURE: the clock (or a file) is wrong, so nothing is deleted"

    parsed.sort(key=lambda p: (p[0], p[1]), reverse=True)     # newest first; ties broken by name
    for rank, (eff, name) in enumerate(parsed, 1):
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
        out[name] = (v[0], v[1], name)
    return [out[n] for _, n in entries if n in out], anomaly


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
