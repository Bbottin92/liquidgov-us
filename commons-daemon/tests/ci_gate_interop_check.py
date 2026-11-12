"""ci_gate_interop_check.py: deploy/ci-gate.sh when Windows git through WSL interop (git.exe) is flaky.
On 2026-11-12 two liquidgov.us deploys (hideint1, hideint2) were refused with a green CI run present: WSL git cannot
read a worktree whose .git points at a C:/ path, so every read goes through git.exe, and a git.exe call that hit WSL
"accept4 failed 110" either refused the deploy ("cannot read the liquidgov-platform tree of HEAD") or, inside the run
loop, was skipped as "a commit this clone lacks", so the green run read as "no platform CI run" (ci-gate.sh:70).
Runs ci-gate.sh in a throwaway repo where plain `git -C <repo>` fails (its .git points at a missing gitdir, like the
Windows worktree seen from WSL) and a fake git.exe + wslpath on PATH stand in for interop, failing on purpose:
  must pass:   git.exe drops the first call, then answers -> CI_GATE_OK (a retry, not a refusal)
               git.exe drops the first read of the green run's tree -> CI_GATE_OK (not "no platform CI run")
               a run for a commit this clone does not have, then the green run -> CI_GATE_OK (missing commit skipped)
  must refuse: git.exe never answers -> refused, naming the tree read
               git.exe answers HEAD then stops answering for good -> refused saying git stopped answering, never
               "no platform CI run" (a dead git must not look like a missing run)
               a red run for this tree with git.exe dropping calls -> still refused as conclusion=failure
Usage: python commons-daemon/tests/ci_gate_interop_check.py <repo root>  -> last line passes=N fails=M; exit 1 on FAIL."""
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
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:500]))
    passes += bool(ok)
    fails += not ok


def fwd(p):
    return p.replace("\\", "/")


def sh(cwd, *args):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    r = subprocess.run(list(args), cwd=cwd, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise RuntimeError("%s failed: %s" % (" ".join(args), r.stderr.strip()))
    return r.stdout.strip()


tmp = tempfile.mkdtemp(prefix="ci-gate-interop-")
try:
    # the real repo, which only the fake git.exe may read
    real = os.path.join(tmp, "real")
    os.makedirs(os.path.join(real, "liquidgov-platform"))
    sh(real, "git", "init", "-q")
    sh(real, "git", "config", "user.email", "t@t")
    sh(real, "git", "config", "user.name", "t")
    sh(real, "git", "config", "core.autocrlf", "false")
    with open(os.path.join(real, "liquidgov-platform", "a.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write("int a = 1;\n")
    sh(real, "git", "add", "-A")
    sh(real, "git", "commit", "-q", "-m", "code")
    head = sh(real, "git", "rev-parse", "HEAD")
    tree = sh(real, "git", "rev-parse", "HEAD:liquidgov-platform")

    # the worktree as WSL sees it: its .git points at a gitdir that does not exist, so plain git fails there
    wt = os.path.join(tmp, "wt")
    os.makedirs(wt)
    with open(os.path.join(wt, ".git"), "w", encoding="utf-8", newline="\n") as f:
        f.write("gitdir: C:/no/such/repo/.git/worktrees/wt\n")

    real_git = fwd(shutil.which("git"))
    fakebin = os.path.join(tmp, "bin")
    os.makedirs(fakebin)
    state = os.path.join(tmp, "state")
    os.makedirs(state)
    # wslpath -w <dir>: interop path conversion; here it just names the real repo
    with open(os.path.join(fakebin, "wslpath"), "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\necho "%s"\n' % fwd(real))
    # git.exe -C <path> <args>: behaviour from files in $state, so each case sets it up:
    #   drop_first   -> the first call of the whole run fails (accept4 110), later calls answer
    #   drop_match   -> the first call whose args contain this text fails, later ones answer
    #   dead_after   -> after this many answered calls, every call fails (git stopped answering)
    #   never        -> every call fails
    with open(os.path.join(fakebin, "git.exe"), "w", encoding="utf-8", newline="\n") as f:
        f.write(r'''#!/usr/bin/env bash
S="%s"
# Git Bash may resolve a plain `git` to this file: anything not aimed at the interop path is real git, uncounted
if [ "${1:-}" != "-C" ] || [ "${2:-}" != "%s" ]; then exec "%s" "$@"; fi
n=$(( $(cat "$S/calls" 2>/dev/null || echo 0) + 1 )); echo "$n" > "$S/calls"
fail() { echo "<3>WSL (1 - ) ERROR: UtilAcceptVsock:281: accept4 failed 110" >&2; exit 1; }
[ -f "$S/never" ] && fail
if [ -f "$S/drop_first" ] && [ "$n" = 1 ]; then fail; fi
if [ -f "$S/drop_match" ] && [ ! -f "$S/dropped" ] && [[ "$*" == *"$(cat "$S/drop_match")"* ]]; then touch "$S/dropped"; fail; fi
if [ -f "$S/dead_after" ]; then
  ok=$(( $(cat "$S/answered" 2>/dev/null || echo 0) ))
  [ "$ok" -ge "$(cat "$S/dead_after")" ] && fail
fi
shift 2  # -C <path>
out="$("%s" -C "%s" "$@")" || exit $?
echo $(( $(cat "$S/answered" 2>/dev/null || echo 0) + 1 )) > "$S/answered"
printf '%%s\n' "$out"
''' % (fwd(state), fwd(real), real_git, real_git, fwd(real)))
    for n in ("wslpath", "git.exe"):
        os.chmod(os.path.join(fakebin, n), 0o755)

    runs_file = os.path.join(tmp, "runs.txt")
    fake_gh = os.path.join(tmp, "fake-gh.sh")
    with open(fake_gh, "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\ncat "%s"\n' % fwd(runs_file))

    def gate(runs, setup):
        for name in os.listdir(state):
            os.remove(os.path.join(state, name))
        for name, text in setup.items():
            with open(os.path.join(state, name), "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
        with open(runs_file, "w", encoding="utf-8", newline="\n") as f:
            f.write("".join(l + "\n" for l in runs))
        env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and k != "LIQUIDGOV_CI_OVERRIDE"}
        env.update({"LIQUIDGOV_GH": "bash " + fwd(fake_gh), "LIQUIDGOV_GATE_REPO": fwd(wt),
                    "PATH": fakebin + os.pathsep + env.get("PATH", "")})
        try:
            r = subprocess.run([BASH, fwd(GATE), "go"], capture_output=True, text=True, env=env,
                               stdin=subprocess.DEVNULL, timeout=120)
        except subprocess.TimeoutExpired:
            return 124, "TIMEOUT after 120 s"
        return r.returncode, (r.stdout + r.stderr).strip()

    green = ["31 %s completed success" % head]
    missing = "0123456789abcdef0123456789abcdef01234567"

    # preconditions: plain git really cannot read the worktree, and the fake git.exe really reads the real repo
    # GIT_* stripped as for the gate itself: a git hook sets GIT_DIR, which would let plain git read the real repo
    r = subprocess.run(["git", "-C", wt, "rev-parse", "HEAD"], capture_output=True, text=True,
                       env={k: v for k, v in os.environ.items() if not k.startswith("GIT_")})
    check("precondition: plain git cannot read the worktree (its .git points at a missing gitdir)", r.returncode != 0,
          r.stdout)
    rc, out = gate(green, {})
    check("precondition: with git.exe answering every call, a green run -> CI_GATE_OK",
          rc == 0 and out.startswith("CI_GATE_OK") and tree[:12] in out, (rc, out))

    rc, out = gate(green, {"drop_first": ""})
    check("git.exe drops the first call (reading HEAD's tree), then answers -> CI_GATE_OK",
          rc == 0 and out.startswith("CI_GATE_OK"), (rc, out))

    rc, out = gate(green, {"drop_match": head + ":liquidgov-platform"})
    check("git.exe drops the first read of the green run's tree -> CI_GATE_OK, not 'no platform CI run'",
          rc == 0 and out.startswith("CI_GATE_OK"), (rc, out))

    rc, out = gate(["32 %s completed failure" % missing] + green, {})
    check("a run for a commit this clone lacks, then the green run -> CI_GATE_OK (the missing one is skipped)",
          rc == 0 and out.startswith("CI_GATE_OK"), (rc, out))

    rc, out = gate(green, {"never": ""})
    check("git.exe never answers -> refused, naming the tree read",
          rc == 1 and "CI_GATE_REFUSED" in out and "cannot read the liquidgov-platform tree of HEAD" in out, (rc, out))

    # HEAD's tree and git status answer (2 calls), then git stops answering for good
    rc, out = gate(green, {"dead_after": "2"})
    check("git.exe stops answering after HEAD -> refused saying git stopped answering, never 'no platform CI run'",
          rc == 1 and "CI_GATE_REFUSED" in out and "stopped answering" in out and "no platform CI run" not in out,
          (rc, out))

    rc, out = gate(["33 %s completed failure" % head], {"drop_first": "", "drop_match": head + ":liquidgov-platform"})
    check("red run for this tree with git.exe dropping calls -> still refused as conclusion=failure",
          rc == 1 and "conclusion=failure" in out, (rc, out))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
