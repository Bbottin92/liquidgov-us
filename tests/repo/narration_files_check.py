"""narration_files_check.py: every Laws chapter has its narration file (Brandon 2026-11-09: "i want the audio").
http_server.cpp serves /assets/chapter<N>.mp3 for N = 1..law_chapters().size(), with a built file name that
lgp1_consistency.py (literal asset names) cannot see. So this counts the chapters in law_chapters() and checks that
liquidgov-platform/assets/chapter1..N.mp3 all exist and are not empty, and that the route's bound is still the chapter
count. Then it proves it can fail: a throwaway copy with one file removed must be refused.
Usage: python tests/repo/narration_files_check.py <repo root> [--self-test-off]
Last line: narration_files_check: N checks, F failed. Exit 1 on any failure."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SELF_TEST = "--self-test-off" not in sys.argv
checks = failed = 0


def check(ok, what):
    global checks, failed
    checks += 1
    if not ok:
        failed += 1
        print("  FAIL " + what)


srv = __import__("runpy").run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "liquidgov-platform", "tests", "browser", "server_source.py"))["read"](os.path.join(ROOT, "liquidgov-platform"))  # http_server.cpp + its src/site pieces
body = srv[srv.find("const std::vector<LawChapter>& law_chapters()"):]
body = body[:body.find("\n}\n")] if "\n}\n" in body else body
n = len(re.findall(r'^\s*\{"[^"]+",\s*\d+,', body, re.M))
check(n >= 10, "found the Laws chapters in law_chapters() (%d)" % n)
check("n > law_chapters().size()" in srv, "the narration route's upper bound is the chapter count")
assets = os.path.join(ROOT, "liquidgov-platform", "assets")
for i in range(1, n + 1):
    p = os.path.join(assets, "chapter%d.mp3" % i)
    check(os.path.exists(p) and os.path.getsize(p) > 0, "assets/chapter%d.mp3 exists and is not empty" % i)

if SELF_TEST and not failed:
    # the check must be able to fail: the same check on a copy with chapter 7's file removed
    tmp = tempfile.mkdtemp(prefix="narration-check-")
    try:
        os.makedirs(os.path.join(tmp, "liquidgov-platform", "src"))
        shutil.copytree(os.path.join(ROOT, "liquidgov-platform", "src"), os.path.join(tmp, "liquidgov-platform", "src"), dirs_exist_ok=True)
        shutil.copytree(assets, os.path.join(tmp, "liquidgov-platform", "assets"),
                        ignore=lambda d, names: [x for x in names if x == "chapter7.mp3"])
        r = subprocess.run([sys.executable, os.path.abspath(__file__), tmp, "--self-test-off"], capture_output=True, text=True)
        check(r.returncode == 1 and "chapter7.mp3" in r.stdout, "self-test: a copy missing chapter7.mp3 is refused")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

print("covered: %d chapters, assets/chapter1..%d.mp3%s" % (n, n, ", self-test" if SELF_TEST else ""))
print("narration_files_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
