#!/usr/bin/env python3
"""When one part of the LGP1 store changes, everything that depends on it must change with it.
    python3 lgp1_consistency.py <repo root>
Brandon (2026-11-07): "when one thing changes, everything thats effected by that change is also changed accordingly."
Each check below is one dependency that drifted, or nearly did, on 2026-11-06/07:
  1. every record type the loader reads is documented in file_store.hpp's header comment;
  2. every record type the writer writes is read back by the loader (no write-only records);
  3. every record type added since the version before the city features (6ab8234) is stripped by
     deploy/city-undo.py, otherwise a rollback leaves a store the old binary refuses;
  4. city-undo.py's docstring, its CITY_TAGS and tests/e2e/city_undo.sh's list name the same types;
  5. every asset file http_server.cpp opens from assets/ exists in liquidgov-platform/assets (the deploy ships that folder).
Prints what it covered and 'lgp1_consistency: N checks, F failed'; exit 1 if any failed.
"""
import os, re, sys

ROOT = sys.argv[1]
checks = failed = 0
def check(ok, what):
    global checks, failed
    checks += 1
    if not ok:
        failed += 1
        print("  FAIL " + what)
def rd(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8", errors="replace") as f:
        return f.read()

# The record types of the store the previous production binary (6ab8234, before the city features) could read.
BASELINE = set("a ac afn alog av blk cm comm cvf cvm em f gc gcm gcn gid gmh gmsg gmsn inner jauto jh jpos jrelay jsub jt lk "
               "m ma ml mlr n nn note pa pau pba pbk pbo pbv pf pg ph pn poff popen post pq pr pref prgr prof prop propn pv "
               "pvote px r repln reply rp s sec".split())

cpp = __import__("runpy").run_path(os.path.join(ROOT, "deploy", "rollback_guard.py"))["store_text"](lambda rel: rd("liquidgov-platform/libs/liquidgov_file_store/" + rel) if os.path.exists(os.path.join(ROOT, "liquidgov-platform/libs/liquidgov_file_store", rel)) else None)  # the store files sources.cmake lists, joined
hpp = rd("liquidgov-platform/libs/liquidgov_file_store/include/liquidgov/platform/file_store.hpp")
read = set(re.findall(r'tag == "([a-z]+)"', cpp))
written = set(re.findall(r'<< "([a-z]+)\\t', cpp))
check(len(read) >= 60, "found the loader's record types (%d)" % len(read))
check(len(written) >= 40, "found the writer's record types (%d)" % len(written))
documented = set(re.findall(r"^//\s{2,3}([a-z]+)\s{1,}<", hpp, re.M))
for t in sorted(read):
    check(t in documented, "record type '%s' is documented in file_store.hpp's header comment" % t)
for t in sorted(written):
    check(t in read, "record type '%s' is written AND read back by the loader" % t)

undo = rd("deploy/city-undo.py")
m = re.search(r"^CITY_TAGS\s*=\s*\(([^)]*)\)", undo, re.M)
check(m is not None, "city-undo.py's CITY_TAGS was found")
city_tags = set(re.findall(r'"([a-z]+)"', m.group(1))) if m else set()
new_since = read - BASELINE
# 2026-11-12 (edited with Brandon's OK: "make the undo checks generic"): a new type may be stripped by ANY
# deploy/<feature>-undo.py with a <NAME>_TAGS tuple, so rolling back one feature never wipes another's records and a
# new feature needs no edit here. tests/repo/undo_lists_check.py keeps each such script and its suite in step.
undo_tags = set(city_tags)
for fn in sorted(os.listdir(os.path.join(ROOT, "deploy"))):
    if fn.endswith("-undo.py") and fn != "city-undo.py":
        um = re.search(r"^[A-Z]+_TAGS\s*=\s*\(([^)]*)\)", rd("deploy/" + fn), re.M)
        check(um is not None, "deploy/%s has a <NAME>_TAGS tuple" % fn)
        undo_tags |= set(re.findall(r'"([a-z]+)"', um.group(1))) if um else set()
for t in sorted(new_since):
    check(t in undo_tags, "new record type '%s' (not in the pre-city binary) is stripped by a deploy/*-undo.py" % t)
for t in sorted(city_tags):
    check(t not in BASELINE, "city-undo.py does not strip '%s', which the pre-city binary owns" % t)
doc = re.search(r"record types:\s*\n\s*\n\s*([a-z ]+)\n", undo)
check(doc is not None and set(doc.group(1).split()) == city_tags,
      "city-undo.py's docstring list matches CITY_TAGS (%s vs %s)" % (doc and sorted(doc.group(1).split()), sorted(city_tags)))
sh = rd("liquidgov-platform/tests/e2e/city_undo.sh")
g = re.search(r"grep -vE '\^\(([a-z|]+)\)", sh)
check(g is not None and set(g.group(1).split("|")) == city_tags,
      "city_undo.sh strips the same list as CITY_TAGS (%s)" % (g and sorted(set(g.group(1).split("|")) ^ city_tags)))
pre = re.search(r"for t in ([a-z ]+); do", sh)
check(pre is not None and set(pre.group(1).split()) <= city_tags and len(set(pre.group(1).split())) >= len(city_tags) - 2,
      "city_undo.sh's precondition loop covers the stripped types (%s)" % (pre and sorted(city_tags - set(pre.group(1).split()))))

srv = __import__("runpy").run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "liquidgov-platform", "tests", "browser", "server_source.py"))["read"](os.path.join(ROOT, "liquidgov-platform"))  # http_server.cpp + its src/site pieces
assets = sorted(set(re.findall(r'ifstream in\("assets/([\w.-]+)"', srv)) | set(re.findall(r'js \? "assets/([\w.-]+)" : "assets/([\w.-]+)"', srv) and
                [x for pair in re.findall(r'js \? "assets/([\w.-]+)" : "assets/([\w.-]+)"', srv) for x in pair] or []))
assets = sorted(set(assets) | set(re.findall(r'^\s*\{"/[^"]+",\s*"assets/([\w.-]+)"', srv, re.M)))   # + the static_files.cpp table rows (2026-11-17)
check(len(assets) >= 5, "found the asset files http_server.cpp opens (%d)" % len(assets))
for a in assets:
    check(os.path.exists(os.path.join(ROOT, "liquidgov-platform", "assets", a)), "asset '%s' that the server opens exists" % a)

print("covered: %d loader types, %d writer types, %d new since 6ab8234 (%s), %d undo types, %d assets" %
      (len(read), len(written), len(new_since), " ".join(sorted(new_since)), len(city_tags), len(assets)))
print("lgp1_consistency: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
