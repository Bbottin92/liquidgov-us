"""scripts/ci_scope.py and its wiring in .github/workflows/platform.yml (Brandon 2026-11-19: "efficiency first, always"; a canvas JS/CSS change
should not wait for the whole server build and every suite). The classifier says canvas_only=true only when every changed file is canvas.js,
canvas_layout_check.py, naming.toml, Persistence/, deploy/, or site.css with ONLY added canvas-scoped rules; the workflow's changes job exposes it and
the heavy jobs skip when it is true, while a light browser-canvas job still runs canvas_layout_check.
Usage: python3 tests/repo/ci_scope_check.py   (exit 0 = pass; fails before scripts/ci_scope.py exists)"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # tests/repo/x.py -> tests/repo -> tests -> the repo root
SCRIPT = os.path.join(ROOT, "scripts", "ci_scope.py")
WORKFLOW = os.path.join(ROOT, ".github", "workflows", "platform.yml")
fails = 0


def check(name, ok, detail=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    fails += not ok


def scope(files, css_diff="", full=False):
    """(canvas_only line, stderr, map line): the classifier prints one line for each."""
    tmp = tempfile.mkdtemp(prefix="ciscope-")
    f, d = os.path.join(tmp, "files.txt"), os.path.join(tmp, "css.diff")
    open(f, "w", encoding="utf-8").write("\n".join(files) + "\n")
    open(d, "w", encoding="utf-8").write(css_diff)
    p = subprocess.run([sys.executable, SCRIPT, "--files", f, "--css-diff", d] + (["--full"] if full else []),
                       capture_output=True, text=True, timeout=30)
    lines = p.stdout.split()
    pick = lambda key: next((l for l in lines if l.startswith(key + "=")), "")
    return pick("canvas_only"), p.stderr, pick("map")


A = "liquidgov-platform/assets/"
T = "liquidgov-platform/tests/browser/"
SCOPED_DIFF = "@@ -40,0 +41,4 @@\n+/* a comment\n+   continued */\n+html.cfs .x{a:b}\n+.cfs-dock{c:d}\n+.cdive-btn{e:f}\n"
check("precondition: scripts/ci_scope.py exists", os.path.isfile(SCRIPT), SCRIPT)
check("canvas.js + its check + notes + deploy pair -> true",
      scope([A + "canvas.js", T + "canvas_layout_check.py", "Persistence/notes.md", "deploy/canvas3-go.sh", "naming.toml"])[0] == "canvas_only=true")
check("site.css with only added canvas-scoped rules -> true", scope([A + "canvas.js", A + "site.css"], SCOPED_DIFF)[0] == "canvas_only=true")
check("site.css with an unscoped added rule -> false", scope([A + "site.css"], "@@ -1,0 +2 @@\n+.card{x:y}\n")[0] == "canvas_only=false")
check("site.css with a removed line -> false", scope([A + "site.css"], "@@ -3 +3 @@\n-.cfs-a{b:c}\n+.cfs-a{b:d}\n")[0] == "canvas_only=false")
check("site.css with an empty diff -> false (nothing proves it is scoped)", scope([A + "site.css"], "")[0] == "canvas_only=false")
for other in ("liquidgov-platform/src/site/features/canvas/canvas.cpp", A + "jukebox.js", T + "map_world_check.py",
              "liquidgov-platform/tests/e2e/asset_routes.sh", "scripts/lgcheck.sh", ".github/workflows/platform.yml"):
    check("a change that also touches %s -> false" % other, scope([A + "canvas.js", other])[0] == "canvas_only=false")
check("a new ref (no base) -> false", scope(["new ref"])[0] == "canvas_only=false")
check("an empty file list -> false", scope([])[0] == "canvas_only=false")
p = subprocess.run([sys.executable, SCRIPT, "--files", os.path.join(tempfile.gettempdir(), "no-such-ciscope.txt"), "--css-diff", os.path.join(tempfile.gettempdir(), "no-such2.txt")],
                   capture_output=True, text=True, timeout=30)
check("unreadable input is false and says so", "canvas_only=false" in p.stdout.split() and "cannot read" in p.stderr, p.stdout + p.stderr)
check("unreadable input runs the map checks (map=true)", "map=true" in p.stdout.split(), p.stdout)

# map=false only when nothing the map checks see changed (2026-11-21: a Juketube-only PR waited ~13 min on them)
J = [A + "juketube.js", A + "juketube.css", "liquidgov-platform/src/site/features/jukebox/jukebox_routes.cpp",
     "liquidgov-platform/src/site/features/live/live_sessions.cpp", T + "juketube_full_check.py",
     "liquidgov-platform/tests/e2e/juketube_history.sh", "liquidgov-platform/tests/e2e/e2e_ws.py", "Persistence/notes.md"]
check("M1 a Juketube-only change -> map=false", scope(J)[2] == "map=false", scope(J))
check("M2 a canvas-only change -> map=false", scope([A + "canvas.js", "Persistence/notes.md"])[2] == "map=false")
for other in (A + "lgmap_walk.js", A + "site.css", "liquidgov-platform/src/site/router.cpp", T + "map_bar_check.py",
              "scripts/lgcheck.sh", ".github/workflows/platform.yml", "liquidgov-platform/libs/liquidgov_file_store/src/file_store.cpp"):
    check("M3 Juketube plus %s -> map=true" % other, scope(J + [other])[2] == "map=true", scope(J + [other]))
check("M4 --full (a push to master) -> map=true even for Juketube only", scope(J, full=True)[2] == "map=true")
check("M5 a new ref -> map=true", scope(["new ref"])[2] == "map=true")
check("M6 an empty file list -> map=true", scope([])[2] == "map=true")

wf = open(WORKFLOW, encoding="utf-8").read() if os.path.isfile(WORKFLOW) else ""
check("workflow: the changes job exposes canvas_only", "canvas_only: ${{ steps." in wf, "")
check("workflow: the changes job exposes map", "map: ${{ steps.diff.outputs.map }}" in wf, "")
check("workflow: a push to master classifies with --full", re.search(r"ci_scope\.py[^\n]*\$FULL", wf) is not None
      and 'FULL="--full"' in wf, "")
# every map/walker browser step is skipped when map=false; every other browser step is not
steps = re.findall(r"\n      - name: ([^\n]*)\n        if: ([^\n]*)\n(?:        [^\n]*\n)*?          python3 liquidgov-platform/tests/browser/(\w+)\.py", wf)
mapish = re.compile(r"^(map_|walker_|ci_map|here_media|dots_pad|profile_dots)")
check("precondition: the workflow's browser steps were found", len(steps) >= 30, len(steps))
for name, cond, script in steps:
    gated = "needs.changes.outputs.map != 'false'" in cond
    if mapish.match(script):
        check("workflow: map step %s skips when map=false" % script, gated, cond)
    elif gated:
        check("workflow: non-map step %s is not skipped by map=false" % script, False, cond)
for job in ("check", "browser-site-a", "browser-site-b", "browser-pages", "screenshots"):
    m = re.search(r"\n  %s:\n(?:    .*\n)*?    if: ([^\n]*)\n" % re.escape(job), wf)
    check("workflow: %s skips when the change is canvas-only" % job, bool(m) and "canvas_only != 'true'" in m.group(1), m.group(1) if m else "no if line")
m = re.search(r"\n  browser-canvas:\n((?:    .*\n|\n)+)", wf)
body = m.group(1) if m else ""
check("workflow: a browser-canvas job runs for canvas-only changes and runs canvas_layout_check",
      "canvas_only == 'true'" in body and "canvas_layout_check.py" in body, body[:200])

try:  # a workflow GitHub cannot parse never starts (2026-11-19: a plain scalar with a colon in the new job did exactly that)
    import yaml
    for name in sorted(os.listdir(os.path.dirname(WORKFLOW))):
        if name.endswith((".yml", ".yaml")):
            try:
                yaml.safe_load(open(os.path.join(os.path.dirname(WORKFLOW), name), encoding="utf-8"))
                check("workflow YAML parses: %s" % name, True)
            except Exception as exc:  # reported with the parser's own words
                check("workflow YAML parses: %s" % name, False, exc)
except ImportError:
    print("SKIP workflow YAML parse: PyYAML is not installed here (the CI runner has it, so the changes job runs this)")

print("ci_scope_check: %d failure(s)" % fails)
sys.exit(1 if fails else 0)
