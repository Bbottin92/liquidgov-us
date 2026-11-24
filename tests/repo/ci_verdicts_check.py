"""scripts/ci_verdicts.py, the one parser of the browser checks' VERDICT lines, and its wiring (q291; 2026-11-21: five
verdicts failed on master for a day unseen because the steps are report-only; lessons/merging-or-pushing.md:80).

  V0  precondition: scripts/ci_verdicts.py exists
  V1  a gh log: ALL-PASS verdicts pass with no cases; a FAILS verdict carries the FAIL lines its job printed since that
      job's previous verdict, whole (a case name with commas stays one case)
  V2  FAIL lines do not leak: not from another job, not from before the same job's previous verdict
  V3  plain lines (a local CI record, no job column) parse too
  V4  parse --github: one ::warning:: per failing verdict, and $GITHUB_STEP_SUMMARY gets a table naming each one
  V5  latest (fake gh): skips a run that built nothing, reports the newest built run's FAILS verdicts and exits 1;
      a clean newest run exits 0 and says clean
  V6  history (fake gh): per verdict, the pass and fail counts over the runs that built
  V7  ship.py reads verdicts through ci_verdicts.verdicts (no second parser)
  V8  .github/workflows/ci-verdicts.yml runs on platform's completed workflow_run and runs ci_verdicts.py parse --github
Usage: python3 tests/repo/ci_verdicts_check.py   (exit 0 = pass; fails before scripts/ci_verdicts.py exists)"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, "scripts", "ci_verdicts.py")
fails = 0


def check(name, ok, detail=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))
    fails += not ok


def gh_line(job, text):
    return "%s\tUNKNOWN STEP\t2026-11-21T13:31:10.2330715Z %s" % (job, text)


Y2 = "Y2 Private: a box private to someone else is not entered (you stop at its edge, in no box); private to you, it is entered"
LOG = "\n".join([
    gh_line("browser-map", "FAIL Z9 an earlier check's failure  :: {}"),
    gh_line("browser-map", "VERDICT earlier: FAILS: Z9 an earlier check's failure  (1 checks)"),
    gh_line("browser-map", "PASS Y1 Open: the walker enters an Open box"),
    gh_line("browser-site", "FAIL S4 a finger on the trail's first button hits it  :: {\"hit\": \"lgm-keybtn\"}"),
    gh_line("browser-map", "FAIL " + Y2 + "  :: {\"at\": \"\"}"),
    gh_line("browser-map", "VERDICT walker-valve-types: FAILS: " + Y2 + "  (5 checks)"),
    gh_line("browser-site", "VERDICT sticky-ci: FAILS: S4 a finger on the trail's first button hits it  (5 checks)"),
    gh_line("browser-site", "VERDICT bar-ci: ALL-PASS  (7 checks; report /home/runner/work/_temp/phone/phone-bar-ci.json)"),
])
CLEAN = "\n".join([gh_line("browser-site", "VERDICT bar-ci: ALL-PASS  (7 checks)"),
                   gh_line("browser-map", "VERDICT walker-valve-types: ALL-PASS  (5 checks)")])
SKIPPED = gh_line("changes", "no platform change against the base: build and tests skipped, run reports success")

check("V0 precondition: scripts/ci_verdicts.py exists", os.path.isfile(SCRIPT), SCRIPT)
if not os.path.isfile(SCRIPT):
    print("ci_verdicts_check: %d failure(s)" % fails)
    sys.exit(1)
sys.path.insert(0, os.path.dirname(SCRIPT))
import ci_verdicts as cv  # noqa: E402

vs = {v["name"]: v for v in cv.verdicts(LOG)}
check("V1 an ALL-PASS verdict passes with no cases", vs.get("bar-ci", {}).get("ok") is True and vs["bar-ci"]["cases"] == [], vs.get("bar-ci"))
check("V1 a FAILS verdict carries its job's FAIL line whole (commas inside one case)",
      vs.get("walker-valve-types", {}).get("ok") is False and vs["walker-valve-types"]["cases"] == [Y2], vs.get("walker-valve-types"))
check("V2 no leak from another job: sticky-ci has only its own job's S4",
      vs.get("sticky-ci", {}).get("cases") == ["S4 a finger on the trail's first button hits it"], vs.get("sticky-ci"))
check("V2 no leak from before the job's previous verdict: Z9 belongs to 'earlier' only",
      vs.get("earlier", {}).get("cases") == ["Z9 an earlier check's failure"] and "Z9" not in json.dumps(vs.get("walker-valve-types")), vs)
plain = cv.verdicts("LIQUIDGOV_LOCAL_CI PASS\nFAIL B6 Back from Groups goes up to You  :: x\nVERDICT bar-ci: FAILS: B6 Back  (6 checks)\nVERDICT intro: ALL-PASS  (12 checks)\n")
check("V3 plain lines (a local record) parse", [(v["name"], v["ok"], v["cases"]) for v in plain]
      == [("bar-ci", False, ["B6 Back from Groups goes up to You"]), ("intro", True, [])], plain)

tmp = tempfile.mkdtemp(prefix="ci-verdicts-check-")
try:
    logf, summary = os.path.join(tmp, "run.log"), os.path.join(tmp, "summary.md")
    open(logf, "w", encoding="utf-8").write(LOG)
    p = subprocess.run([sys.executable, SCRIPT, "parse", logf, "--github"], capture_output=True, text=True, timeout=60,
                       env=dict(os.environ, GITHUB_STEP_SUMMARY=summary))
    ann = [l for l in p.stdout.splitlines() if l.startswith("::warning ")]
    table = open(summary, encoding="utf-8").read() if os.path.exists(summary) else ""
    check("V4 parse --github: exit 0, one ::warning:: per failing verdict (3)", p.returncode == 0 and len(ann) == 3
          and any("title=VERDICT walker-valve-types FAILS" in a for a in ann), (p.returncode, p.stdout[-400:], p.stderr[-300:]))
    check("V4 the job summary names every failing verdict", all("| %s |" % n in table for n in ("earlier", "walker-valve-types", "sticky-ci"))
          and "bar-ci" not in table, table[:400])

    # a fake gh: `run list` prints runs.json, `run view <id> --log` prints <id>.log
    fake = os.path.join(tmp, "fake_gh.py")
    open(fake, "w", encoding="utf-8").write(
        "import os, sys\nd = os.path.dirname(os.path.abspath(__file__))\na = sys.argv[1:]\n"
        "if a[:2] == ['run', 'list']:\n    sys.stdout.write(open(os.path.join(d, 'runs.json')).read())\n"
        "elif a[:2] == ['run', 'view']:\n    sys.stdout.write(open(os.path.join(d, a[2] + '.log'), encoding='utf-8').read())\n"
        "else:\n    sys.exit(3)\n")

    def runs(ids):
        open(os.path.join(tmp, "runs.json"), "w").write(json.dumps(
            [{"databaseId": i, "headSha": "%07d" % i + "abc", "headBranch": "master", "status": "completed", "createdAt": "t%d" % i} for i in ids]))

    for i, text in ((3, SKIPPED), (2, LOG), (1, CLEAN)):
        open(os.path.join(tmp, "%d.log" % i), "w", encoding="utf-8").write(text)
    env = dict(os.environ, CI_VERDICTS_GH=json.dumps([sys.executable, fake]))

    def tool(*args):
        cache = tempfile.mkdtemp(dir=tmp)   # a fresh cache per call: each call reads the fake
        return subprocess.run([sys.executable, SCRIPT] + list(args) + ["--cache", cache], capture_output=True, text=True, timeout=60, env=env)

    runs([3, 2, 1])
    p = tool("latest")
    check("V5 latest: skips the run that built nothing, names the newest built run's FAILS verdicts, exit 1",
          p.returncode == 1 and "run 2 " in p.stdout and "3 of 4 verdicts FAIL" in p.stdout and "walker-valve-types" in p.stdout,
          (p.returncode, p.stdout[-400:], p.stderr[-300:]))
    runs([3, 1])
    p = tool("latest")
    check("V5 latest: a clean newest built run exits 0 and says clean", p.returncode == 0 and "clean" in p.stdout,
          (p.returncode, p.stdout[-300:], p.stderr[-300:]))
    runs([3, 2, 1])
    p = tool("history")
    rows = {l.split()[0]: l for l in p.stdout.splitlines()[1:] if l.strip()}
    check("V6 history: 2 runs that built; walker-valve-types 1 pass 1 fail, bar-ci 2 pass 0 fail",
          p.returncode == 0 and p.stdout.startswith("2 run(s) that built")
          and re.search(r"walker-valve-types\s+1 pass\s+1 fail", rows.get("walker-valve-types", "")) is not None
          and re.search(r"bar-ci\s+2 pass\s+0 fail", rows.get("bar-ci", "")) is not None, (p.stdout[-500:], p.stderr[-300:]))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

ship = open(os.path.join(ROOT, "scripts", "ship.py"), encoding="utf-8").read()
check("V7 ship.py reads verdicts through ci_verdicts.verdicts, no parser of its own",
      "ci_verdicts.verdicts(log)" in ship and 'l.index("VERDICT ")' not in ship)
wf_path = os.path.join(ROOT, ".github", "workflows", "ci-verdicts.yml")
wf = open(wf_path, encoding="utf-8").read() if os.path.exists(wf_path) else ""
check("V8 ci-verdicts.yml runs on platform's completed workflow_run and runs ci_verdicts.py parse --github",
      re.search(r"^on:\s*\n\s+workflow_run:\s*\n\s+workflows: \[platform\]\s*\n\s+types: \[completed\]", wf, re.M) is not None
      and re.search(r"python3 scripts/ci_verdicts\.py parse \S+ --github", wf) is not None, wf_path)

print("ci_verdicts_check: %d failure(s)" % fails)
sys.exit(1 if fails else 0)
