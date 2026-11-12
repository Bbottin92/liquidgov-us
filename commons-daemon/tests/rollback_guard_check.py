"""rollback_guard_check.py: the no-rollback guard (deploy/rollback_guard.py) and its use in deploy/sync-to-droplet.sh
(Brandon 2026-11-09: static check; the live commit from /opt/liquidgov/DEPLOYED_COMMIT on the server; fails closed).
A rollback restores the program built from the LIVE commit; it must still open the store the new commit writes.
  1. rules, both directions, on made-up file_store.cpp versions in a throwaway repo: unchanged / a new record type /
     an exact field check changed / a lower bound that rose (fine) or fell / exact -> lower bound / override / unknown
     live commit / bad argument.
  2. real history, expected answers from OUTSIDE the guard: live 6ab8234 (before the city features) must refuse exactly
     deploy/city-undo.py's CITY_TAGS; live dacf8c3 (before Flow) must refuse exactly pain and mapp (the 2026-11-07
     rollback rule: the pre-flow program could not read Flowed pain rows); live a8080ee (Flow) must pass.
  3. sync-to-droplet.sh end to end with a fake server (fake ssh serving DEPLOYED_COMMIT, fake rsync recording calls,
     fake gh green): reads the live commit, refuses before rsync when incompatible or unknown, records DEPLOYED_COMMIT
     after a real sync, never on a dry run, and stops (exit 6) when the record cannot be written.
Usage: python commons-daemon/tests/rollback_guard_check.py <repo root>  -> last line passes=N fails=M; exit 1 on FAIL."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
GUARD = os.path.join(ROOT, "deploy", "rollback_guard.py")
STORE = "liquidgov-platform/libs/liquidgov_file_store/src/file_store.cpp"
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:500]))
    passes += bool(ok)
    fails += not ok


def fwd(p):
    return p.replace("\\", "/")


def clean_env(extra=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and not k.startswith("LIQUIDGOV_")}
    env.update(extra or {})
    return env


def git(cwd, *args):
    r = subprocess.run(["git"] + list(args), cwd=cwd, capture_output=True, text=True, env=clean_env())
    if r.returncode != 0:
        raise RuntimeError("git %s failed: %s" % (" ".join(args), r.stderr.strip()))
    return r.stdout.strip()


def guard(live, repo, extra=None):
    try:
        r = subprocess.run([sys.executable, GUARD, live, repo], capture_output=True, text=True, env=clean_env(extra),
                           stdin=subprocess.DEVNULL, timeout=60)
    except (subprocess.TimeoutExpired, OSError) as e:
        return 99, str(e)
    return r.returncode, (r.stdout + r.stderr).strip()


def loader(**types):
    """A made-up file_store.cpp: types = {name: size check like '!=3' or '<7'}; every type is written and read."""
    body = ["void save() {"] + ['    body << "%s\\t" << x << "\\n";' % t for t in types] + ["}", "void load() {"]
    first = True
    for t, cond in types.items():
        body.append('    %sif (tag == "%s") {' % ("" if first else "} else ", t))
        op, n = re.match(r"([!<>=]+)(\d+)", cond).groups()
        body.append('        if (f.size() %s %s) throw std::runtime_error("LGP1: bad %s record");' % (op, n, t))
        first = False
    body += ["    }", "}"]
    return "\n".join(body) + "\n"


tmp = tempfile.mkdtemp(prefix="rollback-guard-check-")
try:
    check("precondition: deploy/rollback_guard.py exists in the tree under test", os.path.exists(GUARD), GUARD)

    # ---- 1. rules on made-up loaders
    repo = os.path.join(tmp, "rules")
    os.makedirs(os.path.join(repo, os.path.dirname(STORE)))
    git(repo, "init", "-q")
    git(repo, "config", "user.email", "t@t")
    git(repo, "config", "user.name", "t")
    git(repo, "config", "core.autocrlf", "false")

    def version(text, msg):
        with open(os.path.join(repo, STORE), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        git(repo, "add", "-A")
        git(repo, "commit", "-q", "--allow-empty", "-m", msg)
        return git(repo, "rev-parse", "HEAD")

    def case(name, old, new, want_rc, must=(), must_not=(), extra=None):
        live = version(old, "live " + name)
        version(new, "new " + name)
        rc, out = guard(live, repo, extra)
        ok = rc == want_rc and all(m in out for m in must) and not any(m in out for m in must_not)
        check(name, ok, (rc, out))

    base = dict(n="!=3", a="!=5", pain="<8")
    case("unchanged store code -> OK", loader(**base), loader(**base), 0, ["ROLLBACK_GUARD_OK", "unchanged"])
    case("new record type written -> REFUSED, named", loader(**base), loader(**base, ctag="!=3"), 1,
         ["ROLLBACK_GUARD_REFUSED", "ctag: new record type"], ["  n:", "  a:"])
    case("exact field check changed (!=3 -> !=4) -> REFUSED", loader(**base), loader(**dict(base, n="!=4")), 1,
         ["ROLLBACK_GUARD_REFUSED", "n: field-count check changed"])
    case("lower bound rose (<8 -> <9) -> OK (old loader keeps extra fields)", loader(**base), loader(**dict(base, pain="<9")), 0,
         ["ROLLBACK_GUARD_OK"])
    case("lower bound fell (<8 -> <7) -> REFUSED", loader(**base), loader(**dict(base, pain="<7")), 1,
         ["pain: minimum field count fell"])
    case("exact -> lower bound (the Flow case, !=8 -> <8) -> REFUSED", loader(**dict(base, pain="!=8")), loader(**base), 1,
         ["pain: field-count check changed"])
    case("a type the new code no longer writes -> OK", loader(**dict(base, gone="!=2")), loader(**base), 0, ["ROLLBACK_GUARD_OK"])
    case("override -> exit 0, reason AND the problems logged", loader(**base), loader(**base, ctag="!=3"), 0,
         ["ROLLBACK_GUARD_OVERRIDE: strip ctag first", "ctag: new record type"], extra={"LIQUIDGOV_NO_ROLLBACK_OK": "strip ctag first"})
    rc, out = guard("0" * 40, repo)
    check("live commit not in this clone -> refused (exit 2)", rc == 2 and "ROLLBACK_GUARD_REFUSED" in out, (rc, out))
    rc, out = guard("not-a-commit", repo)
    check("bad live commit argument -> usage, exit 2", rc == 2 and "usage" in out, (rc, out))

    # ---- 2. real history (answers from outside the guard)
    # the guard's own git helper: plain git in WSL cannot read a worktree whose .git holds a Windows path
    have = False
    if os.path.exists(GUARD):
        import importlib.util
        spec = importlib.util.spec_from_file_location("rollback_guard", GUARD)
        rg = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(rg)
        try:
            have = all(rg.git(ROOT, "cat-file", "-t", c) .strip() == "commit" for c in ("6ab8234", "dacf8c3", "a8080ee"))
        except RuntimeError as e:
            print("  history lookup failed: %s" % e)
    check("precondition: history commits 6ab8234, dacf8c3, a8080ee are in this clone", have)
    undo = open(os.path.join(ROOT, "deploy", "city-undo.py"), encoding="utf-8").read()
    city = set(re.findall(r'"([a-z]+)"', re.search(r"^CITY_TAGS\s*=\s*\(([^)]*)\)", undo, re.M).group(1)))
    # 2026-11-12 (edited with Brandon's OK, style library): HEAD also writes the style library's types, which every
    # older live commit refuses; deploy/styles-undo.py strips them. Read from there so the two cannot drift.
    sundo = open(os.path.join(ROOT, "deploy", "styles-undo.py"), encoding="utf-8").read()
    style = set(re.findall(r'"([a-z]+)"', re.search(r"^STYLE_TAGS\s*=\s*\(([^)]*)\)", sundo, re.M).group(1)))

    def refused_types(out):
        return set(re.findall(r"^  ([a-z]+): ", out, re.M))

    rc, out = guard("6ab8234", ROOT)
    check("live 6ab8234 (pre-city) refuses exactly city-undo.py's CITY_TAGS + styles-undo.py's STYLE_TAGS (%d)" % len(city | style),
          rc == 1 and refused_types(out) == city | style and len(city) >= 10,
          (rc, sorted(refused_types(out) ^ (city | style)), out[-300:]))
    rc, out = guard("dacf8c3", ROOT)
    check("live dacf8c3 (pre-Flow) refuses exactly pain and mapp + STYLE_TAGS",
          rc == 1 and refused_types(out) == {"pain", "mapp"} | style, (rc, out))
    rc, out = guard("a8080ee", ROOT)
    check("live a8080ee (Flow, forward-compatible loaders) refuses exactly STYLE_TAGS (%d)" % len(style),
          rc == 1 and refused_types(out) == style and len(style) >= 4, (rc, out))

    # ---- 3. sync-to-droplet.sh end to end
    srepo = os.path.join(tmp, "sync")
    for d in ("deploy", os.path.dirname(STORE)):
        os.makedirs(os.path.join(srepo, d), exist_ok=True)
    for f in ("sync-to-droplet.sh", "ci-gate.sh", "rollback_guard.py", "git-in-repo.sh"):
        if os.path.exists(os.path.join(ROOT, "deploy", f)):  # a missing one shows up as the FAILs below, not a crash
            shutil.copy2(os.path.join(ROOT, "deploy", f), os.path.join(srepo, "deploy", f))
    git(srepo, "init", "-q")
    git(srepo, "config", "user.email", "t@t")
    git(srepo, "config", "user.name", "t")
    git(srepo, "config", "core.autocrlf", "false")

    def scommit(text, msg):
        with open(os.path.join(srepo, STORE), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        git(srepo, "add", "-A")
        git(srepo, "commit", "-q", "-m", msg)
        return git(srepo, "rev-parse", "HEAD")

    live_ok = scommit(loader(**base), "live")
    live_bad = scommit(loader(n="!=2", a="!=5", pain="!=8"), "an older live with exact checks")
    head = scommit(loader(**base), "head")  # same store code as live_ok; incompatible with live_bad (n, pain)
    bin_dir = os.path.join(tmp, "bin")
    os.makedirs(bin_dir)
    calls = os.path.join(tmp, "rsync-calls.txt")
    server_file = os.path.join(tmp, "server-DEPLOYED_COMMIT")
    with open(os.path.join(bin_dir, "rsync"), "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\necho "$*" >> "%s"\n'
                'last="${!#}"\n'
                'if [[ "$last" == *DEPLOYED_COMMIT ]]; then\n'
                '  [ "${FAKE_RSYNC_FAIL_REC:-}" = 1 ] && exit 1\n'
                '  cp "${@: -2:1}" "%s"\n'
                'fi\nexit 0\n' % (fwd(calls), fwd(server_file)))
    os.chmod(os.path.join(bin_dir, "rsync"), 0o755)
    fake_ssh = os.path.join(tmp, "fake-ssh.sh")
    with open(fake_ssh, "w", encoding="utf-8", newline="\n") as f:  # "cat <dir>/DEPLOYED_COMMIT" on the fake server
        f.write('#!/usr/bin/env bash\n[ -f "%s" ] && cat "%s"\nexit 0\n' % (fwd(server_file), fwd(server_file)))
    fake_gh = os.path.join(tmp, "fake-gh.sh")
    with open(fake_gh, "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\necho "1 %s completed success"\n' % head)

    def sync(server_has=None, dry=False, extra=None):
        for p in (calls, server_file):
            if os.path.exists(p):
                os.remove(p)
        if server_has:
            with open(server_file, "w", encoding="utf-8", newline="\n") as f:
                f.write(server_has + "\n")
        env = clean_env({"PATH": bin_dir + os.pathsep + os.environ.get("PATH", ""), "LIQUIDGOV_GH": "bash " + fwd(fake_gh),
                         "LIQUIDGOV_DEPLOY_SSH": "bash " + fwd(fake_ssh), "LIQUIDGOV_DEPLOY_KEY": "/nonexistent"})
        if dry:
            env["LIQUIDGOV_DEPLOY_DRY_RUN"] = "1"
        env.update(extra or {})
        try:
            r = subprocess.run([BASH, fwd(os.path.join(srepo, "deploy", "sync-to-droplet.sh")), "nobody@127.0.0.1:/srv/lg/"],
                               capture_output=True, text=True, env=env, stdin=subprocess.DEVNULL, timeout=120)
        except subprocess.TimeoutExpired:
            return 124, "TIMEOUT", []
        lines = open(calls, encoding="utf-8").read().splitlines() if os.path.exists(calls) else []
        return r.returncode, (r.stdout + r.stderr).strip(), lines

    def recorded():
        return open(server_file, encoding="utf-8").read().strip() if os.path.exists(server_file) else ""

    rc, out, lines = sync(server_has=live_ok)
    check("sync: compatible live commit -> code synced, then DEPLOYED_COMMIT = HEAD, previous live as line 2",
          rc == 0 and len(lines) == 2 and lines[1].endswith("/srv/lg/DEPLOYED_COMMIT")
          and recorded().split() == [head, live_ok] and "ROLLBACK_GUARD_OK" in out, (rc, lines, recorded(), out[-400:]))
    # a failed build: line 1 names a commit that never ran (compatible), line 2 the program still running (incompatible)
    rc, out, lines = sync(server_has=live_ok + "\n" + live_bad)
    check("sync: two-line record (failed build) -> checks BOTH, refuses on the older running one, rsync never called",
          rc == 5 and not lines and "ROLLBACK_GUARD_REFUSED live=%s" % live_bad[:7] in out, (rc, lines, out[-400:]))
    rc, out, lines = sync(server_has=head + "\n" + live_ok)
    check("sync: redeploying the recorded commit keeps one line for it (no duplicate)",
          rc == 0 and recorded().split() == [head], (rc, recorded(), out[-300:]))
    rc, out, lines = sync(server_has=live_bad)
    check("sync: incompatible live commit -> refused, rsync never called", rc == 5 and not lines and "ROLLBACK_GUARD_REFUSED" in out,
          (rc, lines, out[-400:]))
    rc, out, lines = sync(server_has=None)
    check("sync: no DEPLOYED_COMMIT on the server -> refused (fails closed), rsync never called",
          rc == 5 and not lines and "live commit is unknown" in out, (rc, lines, out[-400:]))
    rc, out, lines = sync(server_has=None, extra={"LIQUIDGOV_LIVE_COMMIT": live_ok})
    check("sync: LIQUIDGOV_LIVE_COMMIT given (first deploy) -> syncs and records", rc == 0 and len(lines) == 2
          and recorded().split() == [head, live_ok], (rc, lines, recorded(), out[-400:]))
    rc, out, lines = sync(server_has=live_bad, dry=True)
    check("sync: dry run -> not guarded, nothing recorded", rc == 0 and len(lines) == 1 and "ROLLBACK_GUARD" not in out,
          (rc, lines, out[-400:]))
    rc, out, lines = sync(server_has=live_ok, extra={"FAKE_RSYNC_FAIL_REC": "1"})
    check("sync: DEPLOYED_COMMIT cannot be written -> exit 6, says it stopped before build",
          rc == 6 and "could NOT record" in out, (rc, lines, out[-400:]))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
