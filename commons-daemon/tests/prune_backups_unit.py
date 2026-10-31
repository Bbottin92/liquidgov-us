#!/usr/bin/env python3
"""Unit cases for deploy/prune-backups.py (the pure retention decision). Run by test_retention.sh.
Prints 'prune_backups_unit: N checks, F failed'; exit 1 if any failed."""
import calendar
import importlib.util
import os
import random
import subprocess
import sys
import time

sys.dont_write_bytecode = True      # loading deploy/prune-backups.py must not leave deploy/__pycache__ in the synced tree
HERE = os.path.dirname(os.path.abspath(__file__))
PRUNE = os.path.join(HERE, "..", "deploy", "prune-backups.py")
spec = importlib.util.spec_from_file_location("prune", PRUNE)
prune = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prune)

NOW = calendar.timegm((2026, 9, 20, 0, 0, 0))
DAY = 86400
checks = failed = 0


def check(ok, what):
    global checks, failed
    checks += 1
    if not ok:
        failed += 1
        print("  FAIL", what)


def stamp(epoch):
    return time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(epoch))


def entry(age_s, label="a1b2c3d", mtime_age_s=None):
    """A backup that is age_s old by its name; its mtime defaults to the same instant (a file nobody touched)."""
    t = NOW - age_s
    m = t if mtime_age_s is None else NOW - mtime_age_s
    return (m, "commons.sqlite.pre-%s-%s" % (label, stamp(t)))


def verdicts(entries, **kw):
    dec, anomaly = prune.decide(entries, NOW, **kw)
    return {name: verb for verb, _, name in dec}, anomaly


# ---- what the droplet holds today: ten backups, all within a day or two -> nothing to delete
today = [entry(h * 3600, lab) for h, lab in [(1, "a56eba3"), (4, "hardening"), (5, "reap"), (9, "orphans"), (14, "reap-manual"),
                                             (20, "open"), (26, "rebind"), (30, "janitor"), (38, "snapshot"), (44, "inventory")]]
v, a = verdicts(today)
check(a is None and "DELETE" not in v.values(), "today's droplet: nothing is deleted")
check(list(v.values()).count("KEEP") == 10, "today's droplet: all ten kept")

# ---- the two policies together: 34 backups, one per 3 days, ages 1..100 days
ents = [entry(d * DAY + 1, "reap" if d % 2 else "b%06x" % d) for d in range(1, 101, 3)]
v, a = verdicts(ents)
by_age = sorted(ents, key=lambda e: e[0], reverse=True)                       # newest (largest mtime) first
newest14 = {n for _, n in by_age[:14]}
for m, n in ents:
    age = NOW - m
    want = "KEEP" if (n in newest14 or age <= 30 * DAY) else "DELETE"
    check(v[n] == want, "34-file spread: %s (%.0f days) -> %s" % (n[-20:], age / DAY, want))
check(sum(1 for x in v.values() if x == "DELETE") == 20, "34-file spread: exactly 20 deleted (34 - the newest 14)")
check(all(v[n] == "KEEP" for n in newest14), "the newest 14 stay however old (some are 40 days old)")

# ---- newest 14 stay however old; 6 of 20 ancient ones go
old = [entry((100 + i) * DAY, "x%d" % i) for i in range(20)]
v, _ = verdicts(old)
check(sum(1 for x in v.values() if x == "KEEP") == 14 and sum(1 for x in v.values() if x == "DELETE") == 6,
      "20 backups all 100+ days old: newest 14 kept, 6 deleted")

# ---- within 30 days stays however many
many = [entry(i * 3 * 3600 + 3600, "m%d" % i) for i in range(160)]          # 160 backups spread over 20 days
v, _ = verdicts(many)
check("DELETE" not in v.values(), "160 backups within 20 days: none deleted (30-day rule beats the count)")

# ---- the 30-day boundary
b = [entry(i * 3600 + 1, "n%d" % i) for i in range(14)]                       # 14 fresh ones fill the newest 14
edge_keep = entry(30 * DAY, "edge")
edge_del = entry(30 * DAY + 1, "edgeplus")
v, _ = verdicts(b + [edge_keep, edge_del])
check(v[edge_keep[1]] == "KEEP", "exactly 30 days old is kept (older than, not at least)")
check(v[edge_del[1]] == "DELETE", "30 days + 1s is deleted when outside the newest 14")

# ---- never the newest, even if ancient, even with keep=0
lone = [entry(400 * DAY, "only")]
v, _ = verdicts(lone)
check(v[lone[0][1]] == "KEEP", "a single ancient backup is kept (it is the newest)")
v, _ = verdicts(lone, keep=0, days=0)
check(v[lone[0][1]] == "KEEP", "the newest is kept even with --keep 0 --days 0")

# ---- the 24-hour hard floor holds even if days is overridden to 0
pair = [entry(2 * 3600, "fresh1"), entry(5 * 3600, "fresh2")]
v, _ = verdicts(pair, keep=1, days=0)
check(v[pair[1][1]] == "KEEP", "days=0: a 5h-old backup outside the newest is STILL kept (24h floor)")
pair = [entry(2 * 3600, "fresh1"), entry(25 * 3600, "day2")]
v, _ = verdicts(pair, keep=1, days=0)
check(v[pair[1][1]] == "DELETE", "days=0: a 25h-old one outside the newest goes (the floor is 24h, not more)")

# ---- age is the YOUNGER of the name's timestamp and the mtime
# Both are placed OUTSIDE the newest 14 (14+ fresher backups), so only the age rule can save them.
touched = entry(200 * DAY, "restored", mtime_age_s=10 * DAY)                   # named 200 days ago, copied 10 days ago
v, _ = verdicts(b + [touched])
check(v[touched[1]] == "KEEP", "an old-named backup with a 10-day-old mtime is kept (name-only age would delete it)")
stale_mtime = entry(20 * DAY, "newname", mtime_age_s=200 * DAY)                # named 20 days ago, mtime ancient
v, _ = verdicts([stale_mtime] + [entry((i + 1) * 3600, "z%d" % i) for i in range(15)])
check(v[stale_mtime[1]] == "KEEP", "a 20-day-named backup with an ancient mtime is kept (mtime-only age would delete it)")
control = entry(200 * DAY, "really-old", mtime_age_s=200 * DAY)                # old by BOTH
v, _ = verdicts(b + [control])
check(v[control[1]] == "DELETE", "control: old by both name and mtime is deleted")

# ---- one naming scheme, two producers: deploy names and the daemon's pre-reap names rank together
mixed = [entry((40 + i) * DAY, "reap" if i % 2 else "c%06x" % i) for i in range(20)]
v, _ = verdicts(mixed)
check(sum(1 for x in v.values() if x == "KEEP") == 14, "deploy + pre-reap names are one ranking: 14 kept in total")
check(sum(1 for x in v.values() if x == "DELETE") == 6, "and 6 deleted, whichever producer made them")

# ---- files that are not backups are never counted and never deleted
junk = [(NOW - 500 * DAY, n) for n in [
    "notes.txt", "commons.sqlite", "commons.sqlite-wal", "commons.sqlite-shm",
    "commons.sqlite.pre-x-20200101T000000Z.tmp", "commons.sqlite.pre--20200101T000000Z",
    "commons.sqlite.pre-x-20201340T250000Z", "Commons.sqlite.pre-x-20200101T000000Z", "backup.sqlite"]]
v, _ = verdicts(junk + b)
check(all(v[n] == "IGNORE" for _, n in junk), "non-backup names (incl. an unparseable stamp) are IGNOREd, never deleted")
v, _ = verdicts(junk + [entry(400 * DAY, "real")] + b)
check(v[entry(400 * DAY, "real")[1]] == "DELETE", "...and they do not count toward the newest 14 (a real old backup still goes)")

# ---- a clock/file in the future: nothing at all is deleted
fut = [entry(-2 * 3600, "future")] + [entry((100 + i) * DAY, "o%d" % i) for i in range(30)]
dec, a = prune.decide(fut, NOW)
check(a is not None and "FUTURE" in a, "a backup dated 2h ahead is an ANOMALY")
check(all(verb != "DELETE" for verb, _, _ in dec), "...and then NOTHING is marked DELETE")
dec, a = prune.decide([entry(-1800, "slightly")] + [entry((100 + i) * DAY, "o%d" % i) for i in range(30)], NOW)
check(a is None and any(verb == "DELETE" for verb, _, _ in dec), "30 minutes ahead is tolerated (clock jitter): pruning proceeds")

# ---- empty input
dec, a = prune.decide([], NOW)
check(dec == [] and a is None, "no files: nothing, no anomaly")

# ---- sidecars: <backup>-wal and <backup>-shm (exact names) follow their backup and never take a rank
def side(e, suffix, mtime_age_s=None):
    return (e[0] if mtime_age_s is None else NOW - mtime_age_s, e[1] + suffix)

fresh10 = [entry((i + 1) * 3600, "f%d" % i) for i in range(10)]
olds = [entry((100 + i) * DAY, "o%d" % i) for i in range(5)]
sides = [side(e, s) for e in fresh10 + olds for s in ("-wal", "-shm")]
v, a = verdicts(fresh10 + olds + sides)
check(sum(1 for x in v.values() if x == "DELETE") == 1, "sidecars do NOT count in the newest 14: exactly one of the five old backups is outside it")
check(v[olds[4][1]] == "DELETE", "...the oldest one")
check(v[olds[4][1] + "-wal"] == "SIDE-DELETE" and v[olds[4][1] + "-shm"] == "SIDE-DELETE", "its -wal and -shm are deleted with it")
check(sum(1 for x in v.values() if x == "SIDE-DELETE") == 2, "and ONLY those two sidecars are deleted")
check(all(v[e[1] + s] == "SIDE-KEEP" for e in fresh10 + olds[:4] for s in ("-wal", "-shm")), "every kept backup keeps both sidecars")

# a sidecar is never judged by its own age: an ancient sidecar of a fresh backup stays; a fresh sidecar of a doomed backup goes
v, _ = verdicts(fresh10 + [side(fresh10[0], "-wal", 500 * DAY)])
check(v[fresh10[0][1] + "-wal"] == "SIDE-KEEP", "an ancient sidecar of a KEPT backup is kept (not judged by its own age)")
doomed = entry(200 * DAY, "doomed")
v, _ = verdicts(b + [doomed, side(doomed, "-wal", 1), side(doomed, "-shm", 1)])
check(v[doomed[1] + "-wal"] == "SIDE-DELETE" and v[doomed[1] + "-shm"] == "SIDE-DELETE", "a brand-new sidecar of a DOOMED backup is deleted with it")

# only one of the two present is fine
v, _ = verdicts(b + [doomed, side(doomed, "-shm")])
check(v[doomed[1]] == "DELETE" and v[doomed[1] + "-shm"] == "SIDE-DELETE", "a backup with only a -shm: both go")

# orphans: reported, NEVER deleted, whatever the policy
gone = "commons.sqlite.pre-gone-20200101T000000Z"
orph = [(NOW - 500 * DAY, gone + "-wal"), (NOW - 500 * DAY, gone + "-shm")]
v, _ = verdicts(b + orph)
check(v[gone + "-wal"] == "ORPHAN" and v[gone + "-shm"] == "ORPHAN", "a sidecar whose backup file is gone is an ORPHAN")
v, _ = verdicts(b + orph, keep=0, days=0)
check(v[gone + "-wal"] == "ORPHAN" and v[gone + "-shm"] == "ORPHAN", "an orphan is never deleted, even with --keep 0 --days 0: it stays ORPHAN")
near = doomed[1].replace("T000000Z", "T000001Z")                                # a main name one second different
assert near != doomed[1], "test setup: the near-miss name must differ"
v, _ = verdicts(b + [doomed, side(doomed, "-wal"), (NOW - 500 * DAY, near + "-wal")])
check(v[near + "-wal"] == "ORPHAN", "a sidecar of a near-miss name is an orphan, not paired with a similar backup")
check(v[doomed[1] + "-wal"] == "SIDE-DELETE", "...and the real pair is unaffected")

# exact names only: near-miss suffixes are just other files
x = doomed[1]
lookalikes = [(NOW - 500 * DAY, x + s) for s in ("-wal2", "-wal.bak", "-journal", ".wal", "-WAL", "-wal-wal", "-shm~", "_wal", "-wal ")]
v, _ = verdicts(b + [doomed, side(doomed, "-wal")] + lookalikes)
check(all(v[n] == "IGNORE" for _, n in lookalikes), "-wal2, -wal.bak, -journal, .wal, -WAL, -wal-wal, -shm~ ... are IGNOREd (never deleted)")
check(v[x] == "DELETE" and v[x + "-wal"] == "SIDE-DELETE", "while the exact pair still goes with its backup")

# a sidecar of a name that is not a valid backup (unparseable stamp) is ignored too
bad = "commons.sqlite.pre-x-20201340T250000Z"
v, _ = verdicts(b + [(NOW, bad), (NOW, bad + "-wal")])
check(v[bad] == "IGNORE" and v[bad + "-wal"] == "IGNORE", "an invalid backup name and its sidecar are both IGNOREd")

# the clock anomaly holds sidecars back too
fut = [entry(-2 * 3600, "future")] + [entry((100 + i) * DAY, "o%d" % i) for i in range(20)]
fut += [side(e, s) for e in fut for s in ("-wal", "-shm")]
dec, a = prune.decide(fut, NOW)
check(a is not None and all(verb not in ("DELETE", "SIDE-DELETE") for verb, _, _ in dec), "an anomaly deletes no backup and no sidecar")

# order: newest backup first, each backup followed by its own sidecars, then orphans, then ignored files
o1, o2, o3 = entry(3 * 3600, "c"), entry(1 * 3600, "a"), entry(2 * 3600, "b")
mix = [o1, side(o1, "-wal"), o2, side(o2, "-shm"), o3, side(o3, "-shm"), side(o3, "-wal"),
       (NOW, gone + "-shm"), (NOW, "notes.txt"), (NOW, "commons.sqlite")]
dec, _ = prune.decide(mix, NOW)
got = [n for _, _, n in dec]
want = [o2[1], o2[1] + "-shm", o3[1], o3[1] + "-shm", o3[1] + "-wal", o1[1], o1[1] + "-wal", gone + "-shm", "commons.sqlite", "notes.txt"]
check(got == want, "newest first, sidecars right after their backup, then orphans, then ignored: %s" % got)

# ---- properties over random directories
rng = random.Random(20260920)
labels = ["reap", "a56eba3", "hardening", "house-policy", "reap-manual"]
for trial in range(300):
    n = rng.randint(0, 60)
    es = [entry(rng.randint(60, 200 * DAY), rng.choice(labels) + str(i)) for i in range(n)]   # unique names, like a real directory
    extras = [(NOW - rng.randint(0, 900 * DAY), rng.choice(["x.txt", "commons.sqlite-wal", "commons.sqlite.pre-x-20200101T000000Z.tmp"]))
              for _ in range(rng.randint(0, 4))]
    sc = [side(e, suf) for e in es if rng.random() < 0.5 for suf in ("-wal", "-shm")]
    oc = [(NOW - rng.randint(0, 900 * DAY), "commons.sqlite.pre-gone%d-20200101T000000Z-%s" % (k, rng.choice(["wal", "shm"])))
          for k in range(rng.randint(0, 3))]
    dec, a = prune.decide(es + extras + sc + oc, NOW)
    verb = {name: v for v, _, name in dec}
    rank = sorted(es, key=lambda e: (max(e[0], prune.name_epoch(prune.NAME.match(e[1]).group(2))), e[1]), reverse=True)
    effective = lambda e: max(e[0], prune.name_epoch(prune.NAME.match(e[1]).group(2)))
    ok = True
    if es:
        ok &= verb[rank[0][1]] == "KEEP"                                       # the newest is never deleted
    ok &= sum(1 for e in es if verb[e[1]] == "KEEP") >= min(len(es), 14)       # at least the newest 14 survive
    for e in es:
        if verb[e[1]] == "DELETE":
            age = NOW - effective(e)
            ok &= age > 30 * DAY and age >= 24 * 3600 and e not in rank[:14]
    ok &= all(verb[n] == "IGNORE" for _, n in extras)
    ok &= all(verb[n] == ("SIDE-DELETE" if verb[n[:-4]] == "DELETE" else "SIDE-KEEP") for _, n in sc)     # a sidecar follows its backup
    ok &= all(verb[n] == "ORPHAN" for _, n in oc)                                                          # orphans are only reported
    ok &= sum(1 for v_ in verb.values() if v_ == "DELETE") == sum(1 for e in es if verb[e[1]] == "DELETE")
    survivors = [e for e in es if verb[e[1]] != "DELETE"] + extras + [s for s in sc if verb[s[1]] == "SIDE-KEEP"] + oc
    dec2, _ = prune.decide(survivors, NOW)                                     # pruning twice deletes nothing more
    ok &= all(v != "DELETE" for v, _, _ in dec2)
    check(ok, "random directory #%d (%d backups) satisfies every invariant" % (trial, n))

# ---- the CLI
lines = "".join("%s %s\n" % (m, name) for m, name in old)
p = subprocess.run([sys.executable, PRUNE, "--now", str(NOW)], input=lines, capture_output=True, text=True)
check(p.returncode == 0 and sum(1 for l in p.stdout.splitlines() if l.startswith("DELETE")) == 6, "CLI: 6 DELETE lines for the 20 ancient backups")
check(all(len(l.split("\t")) == 3 for l in p.stdout.strip().splitlines()), "CLI: every line is VERB<TAB>reason<TAB>name")
lines = "".join("%s %s\n" % (m, name) for m, name in fut)
p = subprocess.run([sys.executable, PRUNE, "--now", str(NOW)], input=lines, capture_output=True, text=True)
check(p.returncode == 3 and "ANOMALY" in p.stdout and not any(l.startswith("DELETE") for l in p.stdout.splitlines()),
      "CLI: an anomaly exits 3 and prints no DELETE")
p = subprocess.run([sys.executable, PRUNE, "--nonsense"], input="", capture_output=True, text=True)
check(p.returncode != 0, "CLI: an unknown flag is an error")

print("prune_backups_unit: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
