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

# ---- properties over random directories
rng = random.Random(20260920)
labels = ["reap", "a56eba3", "hardening", "house-policy", "reap-manual"]
for trial in range(300):
    n = rng.randint(0, 60)
    es = [entry(rng.randint(60, 200 * DAY), rng.choice(labels) + str(i)) for i in range(n)]   # unique names, like a real directory
    extras = [(NOW - rng.randint(0, 900 * DAY), rng.choice(["x.txt", "commons.sqlite-wal", "commons.sqlite.pre-x-20200101T000000Z.tmp"]))
              for _ in range(rng.randint(0, 4))]
    dec, a = prune.decide(es + extras, NOW)
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
    survivors = [e for e in es if verb[e[1]] != "DELETE"] + extras
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
