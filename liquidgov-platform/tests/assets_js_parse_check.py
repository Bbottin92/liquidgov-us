"""assets_js_parse_check.py <liquidgov-platform dir>: every assets/*.js parses as JavaScript (node --check: parse only,
nothing runs). Brandon OK'd 2026-11-12 after jukebox.js shipped to a branch with a syntax error and every check green
(no check ran that script). Gating in CI (platform.yml), unlike the report-only browser checks.

Fails when: node is missing (a parse check that cannot run must not pass), fewer than MIN_FILES scripts are found
(wrong folder), or any file does not parse. Last line: VERDICT assets-js-parse: ALL-PASS (n checks) or the failures.
"""
import glob
import os
import shutil
import subprocess
import sys

MIN_FILES = 5   # assets/ held 9 .js files when this was written; fewer than 5 means the wrong folder


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    checks, fails = 0, []

    def check(ok, what, detail=""):
        nonlocal checks
        checks += 1
        print(("PASS " if ok else "FAIL ") + what + ("" if ok or not detail else "  (%s)" % detail))
        if not ok:
            fails.append(what)

    node = shutil.which("node")
    check(node is not None, "node is installed (needed to parse the scripts)")
    files = sorted(glob.glob(os.path.join(platform, "assets", "*.js")))
    check(len(files) >= MIN_FILES, "found the site's scripts in assets/ (%d .js files)" % len(files),
          "looked in %s" % os.path.join(platform, "assets"))
    if node:
        for f in files:
            r = subprocess.run([node, "--check", f], capture_output=True, text=True)
            err = [l for l in (r.stderr or r.stdout).strip().splitlines() if l.strip()]
            # node prints file:line, the source line, a caret, then "SyntaxError: ..." and a stack: show the useful part
            useful = [l for l in err if l.lstrip().startswith(("SyntaxError", "Error"))][:1] + err[:1]
            check(r.returncode == 0, "%s parses" % os.path.basename(f), " | ".join(useful)[:300])
    if fails:
        print("VERDICT assets-js-parse: %d FAIL of %d checks: %s" % (len(fails), checks, "; ".join(fails)))
        return 1
    print("VERDICT assets-js-parse: ALL-PASS  (%d checks)" % checks)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
