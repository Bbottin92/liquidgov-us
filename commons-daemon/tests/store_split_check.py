"""store_split_check.py: the LGP1 store's split into modules stays readable by everything that reads its text
(Brandon 2026-11-12: split file_store.cpp "into per-record-type modules with the same behavior and the same bytes").
deploy/rollback_guard.py, lgp1_consistency.py and program_messaging_test.cpp read the store code as the files
libs/liquidgov_file_store/sources.cmake lists, joined in order. This keeps that list and the code honest:
  1. sources.cmake exists and lists only files that exist, each once; every .cpp under src/ is listed (an unlisted
     store file would be built by nobody and read by nobody);
  2. the joined text reads (rollback_guard.store_text, the one reader) and has load() and flush();
  3. every record type the loader reads (`tag == "x"`) appears once, and its own field-count check (`f.size()`) comes
     before the next type's `tag == ` (the guard reads the FIRST check after each type: a type whose check moved
     elsewhere would be judged by its neighbour's);
  4. record literals live only in listed .cpp files: no `tag == "` or `<< "x\t` in the store's headers;
  5. the round-trip fixtures (tests/fixtures/lgp1_roundtrip*/store_all_tags.lgp1) have a row of every type the loader
     reads, so the byte-for-byte round trip (store_roundtrip_test.cpp) covers every type, including new ones.
Usage: python commons-daemon/tests/store_split_check.py <repo root>  -> last line passes=N fails=M; exit 1 on FAIL."""
import os
import re
import runpy
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
LIB = os.path.join(ROOT, "liquidgov-platform", "libs", "liquidgov_file_store")
FIXTURE = os.path.join(ROOT, "liquidgov-platform", "tests", "fixtures", "lgp1_roundtrip", "store_all_tags.lgp1")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:500]))
    passes += bool(ok)
    fails += not ok


def read(rel):
    path = os.path.join(LIB, rel.replace("/", os.sep))
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8", errors="replace", newline="") as f:
        return f.read()


guard = runpy.run_path(os.path.join(ROOT, "deploy", "rollback_guard.py"))
listing = read("sources.cmake")
check("libs/liquidgov_file_store/sources.cmake exists", listing is not None)
listed = guard["LISTED"].findall(listing or "")
check("sources.cmake lists the store's .cpp files (%d)" % len(listed), len(listed) >= 1)
check("every listed file exists", all(read(p) is not None for p in listed), [p for p in listed if read(p) is None])
check("no file is listed twice", len(listed) == len(set(listed)), listed)
on_disk = sorted(os.path.relpath(os.path.join(d, n), LIB).replace(os.sep, "/")
                 for d, _, names in os.walk(os.path.join(LIB, "src")) for n in names if n.endswith(".cpp"))
check("every .cpp under src/ is listed", sorted(set(listed)) == on_disk, sorted(set(on_disk) ^ set(listed)))

text = ""
try:
    text = guard["store_text"](read)
    check("rollback_guard.store_text reads the split store (%d bytes)" % len(text), True)
except RuntimeError as e:
    check("rollback_guard.store_text reads the split store", False, e)

marks = [(m.start(), m.group(1)) for m in re.finditer(r'tag == "([a-z]+)"', text)]
tags = [t for _, t in marks]
check("the loader reads record types (%d)" % len(tags), len(tags) >= 60)
dup = sorted(t for t in set(tags) if tags.count(t) > 1)
check("each record type is read in one place", not dup, dup)
loose = []
for i, (pos, tag) in enumerate(marks):
    end = marks[i + 1][0] if i + 1 < len(marks) else len(text)
    if not re.search(r"f\.size\(\)\s*[!<>=]=?\s*\d+", text[pos:end]):
        loose.append(tag)
check("every record type's own field-count check comes before the next type", not loose, loose)

headers = sorted(os.path.relpath(os.path.join(d, n), LIB) for d, _, names in os.walk(LIB) for n in names
                 if n.endswith(".hpp"))
in_headers = [h for h in headers if re.search(r'tag == "|<< "[a-z]+\\t', read(h.replace(os.sep, "/")) or "")]
check("no record literal in the store's headers (%d headers)" % len(headers), not in_headers, in_headers)

have = set()
# the first fixture, plus the one for the types added after it (styles, here: tests/store_roundtrip_styles_here_test.cpp)
for fx in (FIXTURE, FIXTURE.replace("lgp1_roundtrip" + os.sep, "lgp1_roundtrip_styles_here" + os.sep)):
    if os.path.isfile(fx):
        with open(fx, encoding="utf-8", newline="") as f:
            have |= {line.split("\t", 1)[0] for line in f.read().split("\n")[1:] if line}
missing = sorted(set(tags) - have)
check("the round-trip fixtures have a row of every record type (%d)" % len(have), not missing, missing)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
