"""Every repo file a workflow names is on the branch (Grok Bot's audit AUDIT-CLAUDE.md, 50672d5 should-fix:
platform.yml pointed to commons-daemon/tests/ci_upload_bounds_check.py, which lives only in an uncommitted patch).
Brief 5 item 4. Static: reads .github/workflows/*.yml as text, no browser.

  P0  the workflows are there
  P1  each path in a workflow (run lines and comments) that starts at a top-level folder of the repo
      (liquidgov-platform/, commons-daemon/, scripts/, deploy/, ...) and ends in a file name with an extension exists,
      so a reader or a step never follows a pointer to nothing. Globs, $VARIABLES and ${{ }} paths are not repo paths.

Usage: python3 workflow_paths_check.py [<repo root>]
Prints PASS/FAIL lines and "VERDICT workflow-paths: ALL-PASS" or the failures; exit 1 on a FAIL."""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = re.compile(r"(?<![\w$}/.-])((?:[A-Za-z0-9_.-]+/)+[A-Za-z0-9_.-]+\.[A-Za-z0-9]+)\b")


def named_paths(text, tops):
    out = []
    for m in PATH.finditer(text):
        p = m.group(1)
        if p.split("/", 1)[0] in tops and "*" not in p:
            out.append(p)
    return sorted(set(out))


def main(argv):
    repo = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
    tops = {d for d in os.listdir(repo) if os.path.isdir(os.path.join(repo, d)) and d != ".git"}
    wfs = sorted(glob.glob(os.path.join(repo, ".github", "workflows", "*.yml")))
    fails, n = [], [0]

    def check(name, ok, detail=""):
        n[0] += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:700]), flush=True)
        if not ok:
            fails.append(name)

    check("P0 the workflows are there", bool(wfs), os.path.join(repo, ".github", "workflows"))
    for wf in wfs:
        paths = named_paths(open(wf, encoding="utf-8").read(), tops)
        missing = [p for p in paths if not os.path.exists(os.path.join(repo, p))]
        check("P1 every repo file %s names exists (%d named)" % (os.path.basename(wf), len(paths)), not missing,
              {"missing": missing})
    print("VERDICT workflow-paths: %s  (%d checks)" % ("ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), n[0]),
          flush=True)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
