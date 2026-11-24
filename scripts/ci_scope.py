#!/usr/bin/env python3
"""ci_scope.py: decide whether a change is canvas-only, so CI can run only what the change can reach (Brandon 2026-11-19, hard rule: "efficiency
first, always"; a JS/CSS-only canvas change waited ~15 minutes for the whole server build and every suite).

  python3 scripts/ci_scope.py --files <names.txt> --css-diff <site.css.diff>   ->  prints canvas_only=true or canvas_only=false

canvas_only=true only when EVERY changed file is one of: assets/canvas.js, tests/browser/canvas_layout_check.py, naming.toml, anything under
Persistence/ or deploy/, or assets/site.css when its diff ADDS only canvas-scoped rules (html.cfs, .cfs-*, .canvas-frame, #ccanvaswrap, .cdive*,
.cprops) and removes nothing. Anything else (C++, store, other assets, other tests, scripts, the workflows) is false: the full pipeline runs.
Unreadable input is false, never true. Test: tests/repo/ci_scope_check.py (run by the changes job before it classifies).

map=false (Brandon 2026-11-21, "expedite that feature"): the ~22 map and walker browser steps took ~13 of a 17-minute run
on a Juketube-only PR. They are skipped only when EVERY changed file is in NOT_MAP (Juketube, its server and store
code and tests, notes, deploy pairs) or canvas-only. Anything else, unreadable input, a new ref, and --full (every push
to master) give map=true: the full set runs."""
import argparse
import sys

EXACT = {"liquidgov-platform/assets/canvas.js", "liquidgov-platform/tests/browser/canvas_layout_check.py", "naming.toml"}
PREFIXES = ("Persistence/", "deploy/")
CSS = "liquidgov-platform/assets/site.css"
SCOPED = ("html.cfs", ".cfs-", ".canvas-frame", "#ccanvaswrap", ".cdive", ".cprops")
P = "liquidgov-platform/"
# Files that cannot change what the map and walker checks see (one list; ci_scope_check.py tests it both ways).
NOT_MAP_EXACT = {P + "assets/juketube.js", P + "assets/juketube.css", P + "assets/jukebox.js",
                 P + "libs/liquidgov_file_store/src/records/jukebox.cpp",
                 P + "libs/liquidgov_file_store/include/liquidgov/platform/records/jukebox.hpp",
                 P + "tests/e2e/e2e_ws.py", "naming.toml"}
NOT_MAP_PREFIXES = (P + "src/site/features/jukebox/", P + "src/site/features/live/", P + "tests/browser/juketube_",
                    P + "tests/browser/jukebox_", P + "tests/e2e/juketube_", P + "tests/jukebox_", "Persistence/", "deploy/")


def css_scoped(diff_text):
    """True when the diff of site.css only adds canvas-scoped rules (and comments) and removes nothing."""
    in_comment = False
    added = 0
    for raw in diff_text.splitlines():
        if raw.startswith(("+++", "---", "@@", "diff ", "index ")) or not raw:
            continue
        if raw.startswith("-"):
            return False
        if not raw.startswith("+"):
            continue
        line = raw[1:].strip()
        added += 1
        if in_comment:
            in_comment = "*/" not in line
            continue
        if not line:
            continue
        if line.startswith("/*"):
            in_comment = "*/" not in line
            continue
        if not line.startswith(SCOPED):
            return False
    return added > 0


def canvas_only(files, css_diff):
    names = [f.strip() for f in files if f.strip()]
    if not names or names == ["new ref"]:
        return False
    for name in names:
        if name in EXACT or name.startswith(PREFIXES):
            continue
        if name == CSS and css_scoped(css_diff):
            continue
        return False
    return True


def map_untouched(files, css_diff):
    """True only when every changed file is in NOT_MAP or the change is canvas-only."""
    names = [f.strip() for f in files if f.strip()]
    if not names or names == ["new ref"]:
        return False
    if canvas_only(files, css_diff):
        return True
    return all(n in NOT_MAP_EXACT or n.startswith(NOT_MAP_PREFIXES) for n in names)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--files", required=True)
    ap.add_argument("--css-diff", required=True)
    ap.add_argument("--full", action="store_true", help="a push to master: the map checks always run")
    args = ap.parse_args(argv)
    try:
        files = open(args.files, encoding="utf-8").read().splitlines()
        diff = open(args.css_diff, encoding="utf-8").read()
    except OSError as exc:
        print("ci_scope: cannot read input (%s); treating the change as NOT canvas-only" % exc, file=sys.stderr)
        print("canvas_only=false")
        print("map=true")
        return 0
    print("canvas_only=%s" % ("true" if canvas_only(files, diff) else "false"))
    print("map=%s" % ("true" if args.full or not map_untouched(files, diff) else "false"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
