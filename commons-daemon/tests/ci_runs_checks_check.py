"""ci_runs_checks_check.py: GitHub CI runs every check the git hooks run (waste-audit PLAN 0.3, Grok Bot 2026-11-11;
Brandon 2026-11-12 "you take 0.3"). Before this, 0 of the 27 checks in this folder ran in any workflow: a clone
without core.hooksPath, or a push that skipped the hooks, got past all of them.

  C0  preconditions: 20+ checks on disk, workflows present (so nothing below passes on an empty list)
  C1  one workflow has a step that runs scripts/run_hook_checks.py
  C2  that workflow has no paths: filter and runs on a push to master and wip/**, or (manual-only since 2026-11-15,
      Actions costs money) scripts/local_ci.py runs it on every run (it can't be skipped)
  C3  run_hook_checks.py --list prints exactly affected_checks.py's run order of the checks on disk (one list,
      shared with the hooks; a new check file is picked up without editing the runner or the workflow)
  C4  run on a fake repo: one passing + one failing check -> exit 1, the failing one named, SUMMARY fails=1
  C5  run on a fake repo with no checks -> exit 1 (an empty run is not a pass)

Usage: python commons-daemon/tests/ci_runs_checks_check.py [repo root] -> last line passes=N fails=M; exit 1 on FAIL."""
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
RUNNER = os.path.join(ROOT, "scripts", "run_hook_checks.py")
SELECTOR = os.path.join(ROOT, "scripts", "affected_checks.py")
TESTS = os.path.join(ROOT, "commons-daemon", "tests")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-500:]))
    passes += bool(ok)
    fails += not ok


def run(*args):
    return subprocess.run([sys.executable] + list(args), capture_output=True, text=True, env=ENV, timeout=300)


def on_block(text):
    """The text of the workflow's top-level `on:` block (up to the next top-level key)."""
    m = re.search(r"^on:\s*\n(.*?)(?=^\S)", text, re.M | re.S)
    return m.group(1) if m else ""


sys.path.insert(0, os.path.dirname(SELECTOR))
import affected_checks as ac  # noqa: E402

on_disk = {n for n in os.listdir(TESTS) if ac.is_check(n)}
workflows = sorted(glob.glob(os.path.join(ROOT, ".github", "workflows", "*.yml")))
check("C0 precondition: 20+ checks on disk", len(on_disk) >= 20, sorted(on_disk))
check("C0 precondition: workflows found", len(workflows) >= 2, workflows)

steps = [w for w in workflows if re.search(r"^\s*(?:run:|python3?)\s.*scripts/run_hook_checks\.py",
                                           open(w, encoding="utf-8").read(), re.M)]
check("C1 a workflow step runs scripts/run_hook_checks.py", len(steps) == 1, [os.path.basename(w) for w in workflows])
for w in steps:
    block = on_block(open(w, encoding="utf-8").read())
    check("C2 %s has no paths: filter" % os.path.basename(w), not re.search(r"^\s+paths(-ignore)?:", block, re.M), block)
    if re.search(r"^\s+push:", block, re.M):
        check("C2 %s push covers master and wip/**" % os.path.basename(w),
              "master" in block and '"wip/**"' in block, block)
    else:
        # Manual-only on GitHub (Brandon 2026-11-15: Actions costs money): scripts/local_ci.py runs this workflow
        # instead, on every run, with no paths filter of its own, and the deploy gate needs its record.
        lc = os.path.join(ROOT, "scripts", "local_ci.py")
        txt = open(lc, encoding="utf-8").read() if os.path.exists(lc) else ""
        name = os.path.splitext(os.path.basename(w))[0]
        check("C2 %s is manual-only (workflow_dispatch)" % os.path.basename(w), "workflow_dispatch" in block, block)
        check("C2 scripts/local_ci.py runs %s on every run" % name,
              re.search(r'^WORKFLOWS = \[[^\]]*"%s"' % re.escape(name), txt, re.M) is not None
              and not re.search(r'wf == "%s" and' % re.escape(name), txt), lc)

if os.path.exists(RUNNER):
    r = run(RUNNER, ROOT, "--list")
    listed = [l for l in r.stdout.splitlines() if l.strip()]
    check("C3 --list = affected_checks run order", r.returncode == 0 and listed == ac.run_order(on_disk),
          (r.returncode, listed, r.stderr[-300:]))
else:
    check("C3 scripts/run_hook_checks.py exists", False, RUNNER)

tmp = tempfile.mkdtemp(prefix="ci-runs-checks-")
try:
    for name, bodies in (("mixed", {"a_ok_check.py": "print('PASS a'); print('passes=1 fails=0')",
                                    "b_bad_check.py": "print('FAIL b broke'); print('passes=0 fails=1'); raise SystemExit(1)"}),
                         ("empty", {})):
        fake = os.path.join(tmp, name)
        os.makedirs(os.path.join(fake, "scripts"))
        os.makedirs(os.path.join(fake, "commons-daemon", "tests"))
        shutil.copy(SELECTOR, os.path.join(fake, "scripts"))
        if os.path.exists(RUNNER):
            shutil.copy(RUNNER, os.path.join(fake, "scripts"))
        for f, body in bodies.items():
            open(os.path.join(fake, "commons-daemon", "tests", f), "w", encoding="utf-8").write(body + "\n")
        runner = os.path.join(fake, "scripts", "run_hook_checks.py")
        if not os.path.exists(runner):
            check("C4/C5 %s: runner present" % name, False, "no scripts/run_hook_checks.py")
            continue
        r = run(runner, fake)
        out = r.stdout + r.stderr
        if name == "mixed":
            check("C4 a failing check -> exit 1", r.returncode == 1, (r.returncode, out[-400:]))
            check("C4 the failing check is named", "b_bad_check.py" in out and "FAIL b broke" in out, out[-400:])
            check("C4 the passing check ran too", "a_ok_check.py" in out, out[-400:])
            check("C4 SUMMARY line says passes=1 fails=1",
                  re.search(r"^SUMMARY hook-checks passes=1 fails=1$", r.stdout, re.M), r.stdout[-300:])
        else:
            check("C5 no checks found -> exit 1", r.returncode == 1, (r.returncode, out[-300:]))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
