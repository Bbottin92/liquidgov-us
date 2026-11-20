#!/usr/bin/env python3
"""ci_scope.py: decide whether a change is canvas-only, so CI can run only what the change can reach (Brandon 2026-11-19, hard rule: "efficiency
first, always"; a JS/CSS-only canvas change waited ~15 minutes for the whole server build and every suite).

  python3 scripts/ci_scope.py --files <names.txt> --css-diff <site.css.diff>   ->  prints canvas_only=true or canvas_only=false

canvas_only=true only when EVERY changed file is one of: assets/canvas.js, tests/browser/canvas_layout_check.py, naming.toml, anything under
Persistence/ or deploy/, or assets/site.css when its diff ADDS only canvas-scoped rules (html.cfs, .cfs-*, .canvas-frame, #ccanvaswrap, .cdive*,
.cprops) and removes nothing. Anything else (C++, store, other assets, other tests, scripts, the workflows) is false: the full pipeline runs.
Unreadable input is false, never true. Test: tests/repo/ci_scope_check.py (run by the changes job before it classifies)."""
import argparse
import sys

EXACT = {"liquidgov-platform/assets/canvas.js", "liquidgov-platform/tests/browser/canvas_layout_check.py", "naming.toml"}
PREFIXES = ("Persistence/", "deploy/")
CSS = "liquidgov-platform/assets/site.css"
SCOPED = ("html.cfs", ".cfs-", ".canvas-frame", "#ccanvaswrap", ".cdive", ".cprops")


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


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--files", required=True)
    ap.add_argument("--css-diff", required=True)
    args = ap.parse_args(argv)
    try:
        files = open(args.files, encoding="utf-8").read().splitlines()
        diff = open(args.css_diff, encoding="utf-8").read()
    except OSError as exc:
        print("ci_scope: cannot read input (%s); treating the change as NOT canvas-only" % exc, file=sys.stderr)
        print("canvas_only=false")
        return 0
    print("canvas_only=%s" % ("true" if canvas_only(files, diff) else "false"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
