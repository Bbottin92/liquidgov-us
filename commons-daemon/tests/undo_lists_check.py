#!/usr/bin/env python3
"""Every feature undo script names the same record types everywhere (Brandon 2026-11-12: "make the undo checks
generic"; the same drift lgp1_consistency.py check 4 guards for city-undo.py).
    python3 undo_lists_check.py [repo root]
For each deploy/<feature>-undo.py other than city-undo.py (which lgp1_consistency.py covers):
  1. it has a <NAME>_TAGS tuple and its docstring's "record types:" list names the same types;
  2. liquidgov-platform/tests/rollback/<feature>_undo.sh exists, strips exactly those types (its grep -vE list) and
     checks each one is in the store first (its precondition loop);
  3. every type is one the store's loader reads and file_store.hpp documents (so the list is not stale).
Prints 'undo_lists_check: N checks, F failed'; exit 1 if any failed."""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
checks = failed = 0


def check(ok, what):
    global checks, failed
    checks += 1
    print(("PASS " if ok else "FAIL ") + what)
    if not ok:
        failed += 1


def rd(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


cpp = rd("liquidgov-platform/libs/liquidgov_file_store/src/file_store.cpp")
hpp = rd("liquidgov-platform/libs/liquidgov_file_store/include/liquidgov/platform/file_store.hpp")
read = set(re.findall(r'tag == "([a-z]+)"', cpp))
documented = set(re.findall(r"^//\s{2,3}([a-z]+)\s{1,}<", hpp, re.M))
scripts = sorted(f for f in os.listdir(os.path.join(ROOT, "deploy")) if f.endswith("-undo.py") and f != "city-undo.py")
for fn in scripts:
    feature = fn[:-len("-undo.py")]
    src = rd("deploy/" + fn)
    m = re.search(r"^[A-Z]+_TAGS\s*=\s*\(([^)]*)\)", src, re.M)
    check(m is not None, "%s: has a <NAME>_TAGS tuple" % fn)
    tags = set(re.findall(r'"([a-z]+)"', m.group(1))) if m else set()
    check(len(tags) >= 1, "%s: names at least one type (%s)" % (fn, sorted(tags)))
    doc = re.search(r"record types:\s*\n\s*\n\s*([a-z ]+)\n", src)
    check(doc is not None and set(doc.group(1).split()) == tags,
          "%s: the docstring list matches the tuple (%s vs %s)" % (fn, doc and sorted(doc.group(1).split()), sorted(tags)))
    suite = "liquidgov-platform/tests/rollback/%s_undo.sh" % feature.replace("-", "_")
    have = os.path.exists(os.path.join(ROOT, suite))
    check(have, "%s: its rollback suite %s exists" % (fn, suite))
    if have:
        sh = rd(suite)
        g = re.search(r"grep -vE '\^\(([a-z|]+)\)", sh)
        check(g is not None and set(g.group(1).split("|")) == tags, "%s: strips exactly the tuple (%s)" % (suite, g and g.group(1)))
        pre = re.search(r"for t in ([a-z ]+); do", sh)
        check(pre is not None and set(pre.group(1).split()) == tags, "%s: its precondition loop covers the tuple (%s)" % (suite, pre and pre.group(1)))
    for t in sorted(tags):
        check(t in read, "%s: '%s' is a type the loader reads" % (fn, t))
        check(t in documented, "%s: '%s' is documented in file_store.hpp" % (fn, t))
check(len(scripts) >= 1, "found the feature undo scripts (%s)" % " ".join(scripts))
print("undo_lists_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
