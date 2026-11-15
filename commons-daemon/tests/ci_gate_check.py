"""ci_gate_check.py: the CI deploy gate (deploy/ci-gate.sh) and its wiring into every make-go.py script
(Brandon 2026-11-08: "yes build the gate"). A deploy may go on only when the platform CI job passed for exactly the
liquidgov-platform/ tree being deployed.
Runs ci-gate.sh in a throwaway git repo with a fake gh (no network, no GitHub), in both directions:
  passes: a green run for HEAD's tree; a later commit that only adds deploy/ files (the generated go/remote pair, which
          CI never runs for) on top of a green tree; the override, which must say why in the log; dry mode never blocks.
  refuses: newest matching run red, still running, or missing; an older green hidden by a newer red; uncommitted
           liquidgov-platform changes; gh failing (fails closed); a green run for a DIFFERENT tree.
Then make-go.py (in a throwaway copy) must put exactly one gate call in a generated go script, before the backup.
Usage: python commons-daemon/tests/ci_gate_check.py <repo root>   -> last line passes=N fails=M; exit 1 on any FAIL."""
import os
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
GATE = os.path.join(ROOT, "deploy", "ci-gate.sh")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))
    passes += bool(ok)
    fails += not ok


def sh(cwd, *args):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    r = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise RuntimeError("%s failed: %s" % (" ".join(args), r.stderr.strip()))
    return r.stdout.strip()


def fwd(p):
    return p.replace("\\", "/")


tmp = tempfile.mkdtemp(prefix="ci-gate-check-")
try:
    repo = os.path.join(tmp, "repo")
    os.makedirs(os.path.join(repo, "liquidgov-platform"))
    os.makedirs(os.path.join(repo, "deploy"))
    sh(repo, "git", "init", "-q")
    sh(repo, "git", "config", "user.email", "t@t")
    sh(repo, "git", "config", "user.name", "t")
    sh(repo, "git", "config", "core.autocrlf", "false")

    def commit(rel, text, msg):
        with open(os.path.join(repo, rel), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        sh(repo, "git", "add", rel)
        sh(repo, "git", "commit", "-q", "-m", msg)
        return sh(repo, "git", "rev-parse", "HEAD")

    old = commit("liquidgov-platform/a.cpp", "int a = 1;\n", "old code")
    code = commit("liquidgov-platform/a.cpp", "int a = 2;\n", "new code")
    runs_file = os.path.join(tmp, "runs.txt")
    fake = os.path.join(tmp, "fake-gh.sh")
    with open(fake, "w", encoding="utf-8", newline="\n") as f:
        # prints the run lines the real gh would print with -q; exits 1 when the file says FAIL
        f.write('#!/usr/bin/env bash\nif grep -qx FAIL "%s"; then echo "HTTP 502" >&2; exit 1; fi\ncat "%s"\n'
                % (fwd(runs_file), fwd(runs_file)))

    def gate(mode, runs, extra_env=None):
        with open(runs_file, "w", encoding="utf-8", newline="\n") as f:
            f.write("".join(l + "\n" for l in runs))
        env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and k != "LIQUIDGOV_CI_OVERRIDE"}
        env.update({"LIQUIDGOV_GH": "bash " + fwd(fake), "LIQUIDGOV_GATE_REPO": fwd(repo)})
        env.update(extra_env or {})
        # stdin closed and a timeout: a gate that reads stdin must fail fast, never hang the commit hook
        try:
            r = subprocess.run([BASH, fwd(GATE), mode], capture_output=True, text=True, env=env,
                               stdin=subprocess.DEVNULL, timeout=60)
        except subprocess.TimeoutExpired:
            return 124, "TIMEOUT after 60 s"
        return r.returncode, (r.stdout + r.stderr).strip()

    # precondition: the two commits really have different liquidgov-platform trees (else every "other tree" case is vacuous)
    t_old = sh(repo, "git", "rev-parse", old + ":liquidgov-platform")
    t_code = sh(repo, "git", "rev-parse", code + ":liquidgov-platform")
    check("precondition: old and new commits have different liquidgov-platform trees", t_old != t_code, (t_old, t_code))

    rc, out = gate("go", ["11 %s completed success" % code])
    check("green run for HEAD's tree -> CI_GATE_OK, exit 0", rc == 0 and out.startswith("CI_GATE_OK"), (rc, out))

    rc, out = gate("go", ["12 %s completed failure" % code])
    check("red run for HEAD's tree -> refused, exit 1", rc == 1 and "CI_GATE_REFUSED" in out, (rc, out))

    rc, out = gate("go", ["13 %s in_progress " % code])
    check("run still in progress -> refused", rc == 1 and "CI_GATE_REFUSED" in out and "in_progress" in out, (rc, out))

    rc, out = gate("go", [])
    check("no run at all -> refused", rc == 1 and "CI_GATE_REFUSED" in out, (rc, out))

    rc, out = gate("go", ["14 %s completed success" % old])
    check("green run only for a DIFFERENT tree -> refused", rc == 1 and "CI_GATE_REFUSED" in out, (rc, out))

    rc, out = gate("go", ["16 %s completed failure" % code, "15 %s completed success" % code])
    check("newest run red, older green for the same tree -> refused (newest decides)", rc == 1 and "CI_GATE_REFUSED" in out, (rc, out))

    rc, out = gate("go", ["FAIL"])
    check("gh fails -> refused (fails closed)", rc == 1 and "CI_GATE_REFUSED" in out and "gh run list failed" in out, (rc, out))

    rc, out = gate("go", ["17 %s completed failure" % code], {"LIQUIDGOV_CI_OVERRIDE": "rollback while GitHub is down"})
    check("override lets it through and logs the reason", rc == 0 and "CI_GATE_OVERRIDE: rollback while GitHub is down" in out, (rc, out))

    rc, out = gate("dry", ["18 %s completed failure" % code])
    check("dry mode never blocks, but says it would refuse", rc == 0 and "CI_GATE_WOULD_REFUSE" in out, (rc, out))

    # git that reads stdin, as git.exe does through WSL interop: inside the gate's `while read` loop it swallowed the
    # rest of the run list, so a red run past the first line came out as "no run" (live probe 2026-11-08)
    real_git = shutil.which("git")
    eat = os.path.join(tmp, "eatbin")
    os.makedirs(eat)
    with open(os.path.join(eat, "git"), "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\ncat >/dev/null 2>&1\nexec "%s" "$@"\n' % fwd(real_git))
    os.chmod(os.path.join(eat, "git"), 0o755)
    rc, out = gate("go", ["21 %s completed success" % old, "22 %s completed failure" % code],
                   {"PATH": eat + os.pathsep + os.environ.get("PATH", "")})
    check("stdin-reading git (WSL git.exe): a red run on line 2 is still found and named",
          rc == 1 and "conclusion=failure" in out, (rc, out))

    # the deploy-only commit (go/remote pair + naming.toml): CI never runs for it, the tree is unchanged
    pair = commit("deploy/x-go.sh", "echo hi\n", "deploy pair")
    rc, out = gate("go", ["19 %s completed success" % code])
    check("deploy-only commit on top of a green tree -> CI_GATE_OK (matched by tree, not commit)",
          rc == 0 and out.startswith("CI_GATE_OK") and pair != code, (rc, out))

    # the local CI record (scripts/local_ci.py, 2026-11-15: GitHub Actions costs money): deploy/ci-pass/<tree>.txt
    os.makedirs(os.path.join(repo, "deploy", "ci-pass"), exist_ok=True)
    commit("deploy/ci-pass/%s.txt" % t_old, "LIQUIDGOV_LOCAL_CI PASS\n", "record for the OLD tree")
    rc, out = gate("go", ["FAIL"])
    check("a local pass record for a DIFFERENT tree -> refused (GitHub asked, and it fails)", rc == 1 and "CI_GATE_REFUSED" in out, (rc, out))
    commit("deploy/ci-pass/%s.txt" % t_code, "LIQUIDGOV_LOCAL_CI FAIL\n", "a failed record")
    rc, out = gate("go", [])
    check("a record that does not say PASS -> refused", rc == 1 and "CI_GATE_REFUSED" in out, (rc, out))
    commit("deploy/ci-pass/%s.txt" % t_code, "LIQUIDGOV_LOCAL_CI PASS\ntree %s\n" % t_code, "the pass record")
    rc, out = gate("go", ["FAIL"])
    check("a committed local pass record for HEAD's tree -> CI_GATE_OK without GitHub",
          rc == 0 and out.startswith("CI_GATE_OK") and "local CI record" in out, (rc, out))
    with open(os.path.join(repo, "deploy", "ci-pass", t_code + ".txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("LIQUIDGOV_LOCAL_CI PASS\n")
    sh(repo, "git", "rm", "-q", "--cached", "deploy/ci-pass/%s.txt" % t_code)
    sh(repo, "git", "commit", "-q", "-m", "record only on disk")
    rc, out = gate("go", [])
    check("a pass record on disk but not committed -> refused", rc == 1 and "CI_GATE_REFUSED" in out, (rc, out))
    sh(repo, "git", "rm", "-q", "-r", "--cached", "--ignore-unmatch", "deploy/ci-pass")
    shutil.rmtree(os.path.join(repo, "deploy", "ci-pass"))
    sh(repo, "git", "commit", "-q", "--allow-empty", "-m", "no records")

    with open(os.path.join(repo, "liquidgov-platform", "a.cpp"), "a", encoding="utf-8", newline="\n") as f:
        f.write("// not committed\n")
    rc, out = gate("go", ["20 %s completed success" % code])
    check("uncommitted liquidgov-platform change -> refused", rc == 1 and "uncommitted" in out, (rc, out))
    sh(repo, "git", "checkout", "-q", "--", "liquidgov-platform/a.cpp")

    # make-go.py wiring: every generated go script calls the gate once, before the backup, with the run's mode
    mg = os.path.join(tmp, "mg")
    os.makedirs(os.path.join(mg, "deploy"))
    for rel in ["deploy/make-go.py", "naming.toml"] + ["deploy/%s-%s.sh" % (n, p) for n in ("jbshell", "care") for p in ("go", "remote")]:
        shutil.copy2(os.path.join(ROOT, rel), os.path.join(mg, rel))
    for name, f in (("gateone", "assets/x.js"), ("gatetwo", "src/x.cpp")):
        env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
        r = subprocess.run([sys.executable, os.path.join(mg, "deploy", "make-go.py"), "auto", name, "gate test", f],
                           capture_output=True, text=True, cwd=mg, env=env)
        go = os.path.join(mg, "deploy", "%s-go.sh" % name)
        txt = open(go, encoding="utf-8").read() if os.path.exists(go) else ""
        call = 'bash "$ROOT/deploy/ci-gate.sh" "${1:-}" || exit 1'
        ok = r.returncode == 0 and txt.count(call) == 1 and 0 <= txt.find(call) < txt.find("\nremote backup")
        check("make-go %s (%s): one gate call, before 'remote backup'" % (name, f), ok,
              (r.returncode, r.stderr.strip()[-200:], txt.count(call)))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
