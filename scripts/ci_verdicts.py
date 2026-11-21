#!/usr/bin/env python3
"""The browser checks' VERDICT lines, read in one place (q291, Brandon 2026-11-21 "build q291 ci_verdicts.py now").

Why: the browser checks run as report-only CI steps, so a run says success while a verdict FAILS. On 2026-11-21 intro,
walker-valve-types, sticky-ci, herebubble-ci and bar-ci had failed on master since 2026-11-20 and nobody saw it until
someone read the log (lessons/merging-or-pushing.md:80). This file is the one parser of those lines; ship.py reads
verdicts through it, CI prints a run's failures as warnings and in the job summary, and `latest` says at a glance
whether master's newest built run is clean.

A verdict line, as every tests/browser check prints it:   VERDICT <name>: ALL-PASS  (<n> checks...)
                                                      or:   VERDICT <name>: FAILS: <cases>  (<n> checks...)
The failing cases are NOT split out of the verdict line (case names hold commas: "... at its edge, in no box); ...");
they are the "FAIL <case>  :: <detail>" lines the same job printed since its previous VERDICT. `gh run view --log`
prefixes each line with "<job>\\t<step>\\t<timestamp> " (the step is often "UNKNOWN STEP", so jobs, not steps, group).

Usage:
  ci_verdicts.py parse <log file | ->  [--fails-only] [--github]
        one line per verdict; --github also prints a ::warning:: annotation per failing verdict and appends a table
        to $GITHUB_STEP_SUMMARY when it is set. Exit 0 (report-only, like the checks themselves).
  ci_verdicts.py history [--branch B] [--limit N] [--cache DIR] [--name V ...]
        fetch the platform runs' logs once into DIR (default ~/.cache/liquidgov-ci-logs), skip runs that built
        nothing (no VERDICT line), and print each verdict's pass count, fail count and the cases of its newest failure,
        most-failing first. Exit 0.
  ci_verdicts.py latest [--branch master] [--limit N] [--cache DIR]
        master's newest completed platform run that built: its FAILS verdicts, or "clean". Exit 1 when one FAILS.
gh is `gh` unless CI_VERDICTS_GH holds a JSON list (the tests' fake)."""
import json
import os
import re
import subprocess
import sys

GH = json.loads(os.environ["CI_VERDICTS_GH"]) if os.environ.get("CI_VERDICTS_GH") else ["gh"]
GH_REPO = ["--repo", "Bbottin92/liquidgov-us"]
PLATFORM_NAME = "platform"   # the `name:` of .github/workflows/platform.yml (ship.py's PLATFORM_NAME)
STAMP = re.compile(r"^\d{4}-\d\d-\d\dT[\d:.]+Z ")   # gh's timestamp before each line's text
VERDICT = re.compile(r"\bVERDICT ([A-Za-z0-9_.-]+): (.*)$")
FAIL = re.compile(r"^FAIL (.*?)(?:\s+::\s.*)?$")


class Fail(Exception):
    pass


def split(line):
    """(job, text) of one log line: gh's '<job>\\t<step>\\t<stamp> <text>', or a plain line (a local record)."""
    parts = line.rstrip("\r\n").split("\t", 2)
    if len(parts) == 3:
        return parts[0], STAMP.sub("", parts[2], count=1)
    return "", line.rstrip("\r\n")


def verdicts(log):
    """Every VERDICT in a log, in order: {name, ok, line, cases, job}. cases: the FAIL lines that job printed since its
    previous VERDICT (empty for a pass; for a failure with no FAIL line, empty too: the verdict line still says FAILS)."""
    out, pending = [], {}
    for raw in log.splitlines():
        job, text = split(raw)
        m = FAIL.match(text)
        if m:
            pending.setdefault(job, []).append(m.group(1).strip())
            continue
        v = VERDICT.search(text)
        if v:
            rest = v.group(2)
            out.append({"name": v.group(1), "ok": rest.startswith("ALL-PASS"), "job": job,
                        "line": text[v.start():].strip(), "cases": [] if rest.startswith("ALL-PASS") else pending.get(job, [])})
            pending[job] = []
    return out


def report(vs, fails_only=False):
    lines = []
    for v in vs:
        if v["ok"] and fails_only:
            continue
        lines.append(("PASS " if v["ok"] else "FAILS ") + v["name"] + ("" if v["ok"] else ": " + (
            "; ".join(v["cases"]) if v["cases"] else v["line"])))
    fails = [v for v in vs if not v["ok"]]
    lines.append("ci_verdicts: %d verdict(s), %d FAILS%s" % (len(vs), len(fails), (": " + ", ".join(v["name"] for v in fails)) if fails else ""))
    return lines


def github(vs):
    """::warning:: annotations (one per failing verdict) and a job-summary table; returns the annotation lines."""
    ann = []
    for v in vs:
        if not v["ok"]:
            msg = "; ".join(v["cases"]) or v["line"]
            ann.append("::warning title=VERDICT %s FAILS::%s" % (v["name"], msg.replace("%", "%25").replace("\r", "").replace("\n", " ")))
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        fails = [v for v in vs if not v["ok"]]
        with open(path, "a", encoding="utf-8") as f:
            f.write("### Browser check verdicts: %d, %d FAILS\n\n" % (len(vs), len(fails)))
            if fails:
                f.write("| verdict | failing cases |\n|---|---|\n")
                for v in fails:
                    f.write("| %s | %s |\n" % (v["name"], ("; ".join(v["cases"]) or v["line"]).replace("|", "\\|")))
            else:
                f.write("All browser checks passed.\n")
    return ann


def gh(args, what):
    try:
        p = subprocess.run(GH + args + GH_REPO, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
    except (OSError, subprocess.SubprocessError) as e:
        raise Fail("%s could not run: %s" % (what, e))
    if p.returncode != 0:
        raise Fail("%s failed (exit %d): %s" % (what, p.returncode, (p.stderr or p.stdout).strip()[:300]))
    return p.stdout


def runs(branch, limit):
    out = gh(["run", "list", "--workflow", PLATFORM_NAME, "--limit", str(limit),
              "--json", "databaseId,headSha,headBranch,status,createdAt"] + (["--branch", branch] if branch else []), "gh run list")
    try:
        return [r for r in json.loads(out or "[]") if r.get("status") == "completed"]
    except ValueError:
        raise Fail("gh run list printed something that is not JSON: %r" % out[:200])


def log_of(run_id, cache):
    """The run's whole log, from the cache when it is there (a completed run's log never changes)."""
    path = os.path.join(cache, "%s.log" % run_id)
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    text = gh(["run", "view", str(run_id), "--log"], "gh run view %s --log" % run_id)
    os.makedirs(cache, exist_ok=True)
    with open(path + ".part", "w", encoding="utf-8") as f:
        f.write(text)
    os.replace(path + ".part", path)
    return text


def history(branch, limit, cache, names):
    tally, seen = {}, 0
    for r in runs(branch, limit):   # newest first
        vs = verdicts(log_of(r["databaseId"], cache))
        if not vs:
            continue   # the run built nothing (ci_scope skipped it)
        seen += 1
        for v in vs:
            if names and v["name"] not in names:
                continue
            t = tally.setdefault(v["name"], {"pass": 0, "fail": 0, "newest_fail": None})
            t["pass" if v["ok"] else "fail"] += 1
            if not v["ok"] and t["newest_fail"] is None:
                t["newest_fail"] = "%s %s: %s" % (r["headSha"][:7], r.get("headBranch"), "; ".join(v["cases"]) or v["line"])
    lines = ["%d run(s) that built, newest %d listed%s" % (seen, limit, " on " + branch if branch else "")]
    for name, t in sorted(tally.items(), key=lambda kv: (-kv[1]["fail"], kv[0])):
        lines.append("%-28s %3d pass %3d fail%s" % (name, t["pass"], t["fail"],
                                                     ("   newest fail " + t["newest_fail"][:220]) if t["newest_fail"] else ""))
    return lines


def latest(branch, limit, cache):
    for r in runs(branch, limit):
        vs = verdicts(log_of(r["databaseId"], cache))
        if vs:
            fails = [v for v in vs if not v["ok"]]
            head = "%s run %s (%s, %s): " % (branch, r["databaseId"], r["headSha"][:7], r.get("createdAt"))
            if not fails:
                return [head + "clean, %d verdicts ALL-PASS" % len(vs)], 0
            return [head + "%d of %d verdicts FAIL" % (len(fails), len(vs))] + report(fails, fails_only=True)[:-1], 1
    return ["%s: no completed platform run that built in the newest %d" % (branch, limit)], 0


def opt(argv, name, default=None, many=False):
    vals = [argv[i + 1] for i, a in enumerate(argv[:-1]) if a == name]
    return vals if many else (vals[-1] if vals else default)


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    cmd, rest = argv[0], argv[1:]
    cache = opt(rest, "--cache", os.path.join(os.path.expanduser("~"), ".cache", "liquidgov-ci-logs"))
    try:
        if cmd == "parse":
            src = next((a for a in rest if not a.startswith("--")), None)
            if src is None:
                print("ci_verdicts parse: name a log file or - for stdin", file=sys.stderr)
                return 2
            text = sys.stdin.read() if src == "-" else open(src, encoding="utf-8", errors="replace").read()
            vs = verdicts(text)
            if "--github" in rest:
                for a in github(vs):
                    print(a)
            print("\n".join(report(vs, "--fails-only" in rest)))
            return 0
        if cmd == "history":
            print("\n".join(history(opt(rest, "--branch"), int(opt(rest, "--limit", "40")), cache, opt(rest, "--name", many=True))))
            return 0
        if cmd == "latest":
            lines, code = latest(opt(rest, "--branch", "master"), int(opt(rest, "--limit", "20")), cache)
            print("\n".join(lines))
            return code
    except Fail as e:
        print("ci_verdicts: %s" % e, file=sys.stderr)
        return 2
    print("ci_verdicts: unknown command %r (parse, history, latest; --help)" % cmd, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
