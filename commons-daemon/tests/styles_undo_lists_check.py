#!/usr/bin/env python3
"""The style library's undo lists name the same record types everywhere (Brandon 2026-11-12, profile CSS slice 2;
the same drift lgp1_consistency.py check 4 guards for city-undo.py).
    python3 styles_undo_lists_check.py [repo root]
  1. deploy/styles-undo.py's docstring list == its STYLE_TAGS;
  2. liquidgov-platform/tests/rollback/styles_undo.sh strips exactly STYLE_TAGS (its grep -vE list) and checks each one
     is in the store first (its precondition loop);
  3. every STYLE_TAGS type is one the store's loader reads and documents in file_store.hpp (so the list is not stale).
Prints 'styles_undo_lists_check: N checks, F failed'; exit 1 if any failed."""
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


undo = rd("deploy/styles-undo.py")
m = re.search(r"^STYLE_TAGS\s*=\s*\(([^)]*)\)", undo, re.M)
check(m is not None, "styles-undo.py's STYLE_TAGS was found")
tags = set(re.findall(r'"([a-z]+)"', m.group(1))) if m else set()
check(len(tags) >= 4, "STYLE_TAGS names the four style types (%s)" % sorted(tags))
doc = re.search(r"record types:\s*\n\s*\n\s*([a-z ]+)\n", undo)
check(doc is not None and set(doc.group(1).split()) == tags,
      "the docstring list matches STYLE_TAGS (%s vs %s)" % (doc and sorted(doc.group(1).split()), sorted(tags)))

sh = rd("liquidgov-platform/tests/rollback/styles_undo.sh")
g = re.search(r"grep -vE '\^\(([a-z|]+)\)", sh)
check(g is not None and set(g.group(1).split("|")) == tags, "styles_undo.sh strips exactly STYLE_TAGS (%s)" % (g and g.group(1)))
pre = re.search(r"for t in ([a-z ]+); do", sh)
check(pre is not None and set(pre.group(1).split()) == tags, "styles_undo.sh's precondition loop covers STYLE_TAGS (%s)" % (pre and pre.group(1)))

cpp = rd("liquidgov-platform/libs/liquidgov_file_store/src/file_store.cpp")
hpp = rd("liquidgov-platform/libs/liquidgov_file_store/include/liquidgov/platform/file_store.hpp")
read = set(re.findall(r'tag == "([a-z]+)"', cpp))
documented = set(re.findall(r"^//\s{2,3}([a-z]+)\s{1,}<", hpp, re.M))
for t in sorted(tags):
    check(t in read, "'%s' is a type the loader reads" % t)
    check(t in documented, "'%s' is documented in file_store.hpp" % t)

print("styles_undo_lists_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
