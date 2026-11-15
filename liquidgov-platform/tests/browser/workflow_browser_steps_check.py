"""Every browser check runs in CI (Grok Bot's audit AUDIT-CLAUDE.md, ranked fix 1: map_live_check.py and
juketube_overlap_check.py "run nowhere automatically"; run 4 added five more the same way). Brief 5 item 1.
Static: reads .github/workflows/platform.yml, no browser.

  W0  platform.yml is there and has steps
  W1  each tests/browser/*_check.py is run by a step of its own, directly or through its ci_<name> wrapper
      (ci_phone_check.py runs phone_check.py, and so on). Exempt, with the reason printed: me_autosave_check.py, whose
      A3 samples a 3 ms answer and failed 3 of 6 runs (run 4); me_autosave_slow_check.py runs the same 8 cases.
      run_phone_check.py: a Windows runner, not a check.
  W2  every such step is report-only and bounded: if: always(), continue-on-error: true, timeout-minutes <= 10
      (a flaky or hung browser must never turn the run red or hold it: deploy/ci-gate.sh needs a green run)
  W3  every such step writes its pictures where the screenshot upload looks ($RUNNER_TEMP/phone), when it takes an
      out dir

Usage: python3 workflow_browser_steps_check.py [<repo root>]
Prints PASS/FAIL lines and "VERDICT workflow-browser-steps: ALL-PASS" or the failures; exit 1 on a FAIL."""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXEMPT = {"me_autosave_check.py": "A3 is timing-flaky on a fast mock (3 of 6 runs failed, run 4); "
                                  "me_autosave_slow_check.py runs the same 8 cases with a 400 ms answer",
          "run_phone_check.py": "a Windows runner for phone_check.py (WSL server + Edge), not a check; "
                                "phone_check.py runs in CI through ci_phone_check.py",
          # retired 2026-11-15 (the map is one world; map_world_check.py checks it); kept as code other checks import
          "map_live_check.py": "retired: the old ring model; kept for its page markup and helpers (walker checks import it)",
          "map_live_check2.py": "retired: the old ring model; kept for its fake graph (map_world_check imports it)",
          "map_big_level_check.py": "retired: the old ring model; kept for its big graph (map_world_check imports it)"}
OUT_DIR = '"$RUNNER_TEMP/phone"'


def steps(text):
    """[(name, body)] for each '- name:' step under jobs.*.steps (indentation-based; no YAML library needed)."""
    out = []
    parts = re.split(r"\n(?=      - (?:name|uses): )", text)
    for p in parts[1:]:
        m = re.match(r"      - (?:name|uses): *(.*)", p)
        out.append((m.group(1).strip() if m else "?", p))
    return out


def takes_out_dir(path):
    """The check's usage line names an <out dir> argument."""
    try:
        with open(path, encoding="utf-8") as f:
            return "<out dir>" in f.read(3000)
    except OSError:
        return False


def main(argv):
    repo = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
    wf_path = os.path.join(repo, ".github", "workflows", "platform.yml")
    browser = os.path.join(repo, "liquidgov-platform", "tests", "browser")
    fails, n = [], [0]

    def check(name, ok, detail=""):
        n[0] += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:700]), flush=True)
        if not ok:
            fails.append(name)

    wf = open(wf_path, encoding="utf-8").read() if os.path.exists(wf_path) else ""
    st = steps(wf)
    check("W0 platform.yml is there and has steps", bool(st), wf_path)
    names = sorted(os.path.basename(p) for p in glob.glob(os.path.join(browser, "*_check.py")))
    for name in names:
        if name.startswith("ci_"):
            continue  # a CI wrapper: judged as the check it runs
        if name in EXEMPT:
            print("SKIP W1 %s: %s" % (name, EXEMPT[name]), flush=True)
            continue
        runners = [name] + (["ci_" + name] if os.path.exists(os.path.join(browser, "ci_" + name)) else [])
        call = re.compile(r"tests/browser/(%s)\b" % "|".join(re.escape(r) for r in runners))
        mine = [(sn, body) for sn, body in st if call.search(body)]
        check("W1 %s runs in a CI step" % name, bool(mine), {"looked for": runners})
        for sn, body in mine:
            tm = re.search(r"\n +timeout-minutes: *(\d+)", body)
            ok = (re.search(r"\n +if: *always\(\)", body) is not None
                  and re.search(r"\n +continue-on-error: *true\b", body) is not None
                  and tm is not None and int(tm.group(1)) <= 10)
            check("W2 %s's step is report-only and bounded (%s)" % (name, sn), ok,
                  "needs if: always(), continue-on-error: true, timeout-minutes <= 10")
            script = call.search(body).group(1)
            if takes_out_dir(os.path.join(browser, script)):
                check("W3 %s's step writes its pictures to %s" % (name, OUT_DIR), OUT_DIR in body, body[-300:])
    print("VERDICT workflow-browser-steps: %s  (%d checks)" % ("ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), n[0]),
          flush=True)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
