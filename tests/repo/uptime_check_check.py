"""scripts/uptime_check.py, the outside check that liquidgov.us resolves and loads BY NAME (q295; the site was down
2026-11-20 and 2026-11-21 because its nameservers went silent while the server was fine, and nothing said so).
A fake dig and curl (UPTIME_DIG / UPTIME_CURL) replay each case from a JSON scenario file.
  K0  precondition: scripts/uptime_check.py and .github/workflows/uptime.yml exist
  K1  healthy: nameservers known, all answer the same address, public resolvers agree, 200 -> ALL-PASS, exit 0
  K2  the 2026-11-21 outage: the delegated nameservers are silent -> U2 FAIL (names the silent ones), exit 1
  K3  one nameserver answers a different address -> U2 FAIL
  K4  a public resolver gives nothing (cache expired during an outage) -> U3 FAIL
  K5  a slow first load then 200 -> PASS after the retry; 503 twice -> U4 FAIL with both tries
  K6  on GitHub Actions a FAIL is also an ::error:: annotation
  K7  the workflow runs it on a schedule (every 15 min) and by hand, with no paths filter
Usage: python3 tests/repo/uptime_check_check.py   (exit 0 = pass; fails before scripts/uptime_check.py exists)"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, "scripts", "uptime_check.py")
WORKFLOW = os.path.join(ROOT, ".github", "workflows", "uptime.yml")
fails = 0


def check(name, ok, detail=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:500]))
    fails += not ok


check("K0 precondition: scripts/uptime_check.py exists", os.path.isfile(SCRIPT), SCRIPT)
check("K0 precondition: .github/workflows/uptime.yml exists", os.path.isfile(WORKFLOW), WORKFLOW)
if not os.path.isfile(SCRIPT):
    print("uptime_check_check: %d failure(s)" % fails)
    sys.exit(1)

FAKE_DIG = r'''import json, os, sys
s = json.load(open(os.environ["UPTIME_SCENARIO"]))
a = [x for x in sys.argv[1:] if not x.startswith("+")]
rtype, name, server = a[0], a[1], a[2].lstrip("@")
key = "%s %s %s" % (rtype, name, server)
if key not in s["dig"]:
    sys.exit(9)                       # a timeout: dig prints ';; connection timed out' and exits 9
print("\n".join(s["dig"][key]))
'''
FAKE_CURL = r'''import json, os, sys
s = json.load(open(os.environ["UPTIME_SCENARIO"]))
n = os.path.join(os.path.dirname(os.environ["UPTIME_SCENARIO"]), "curl.count")
i = int(open(n).read()) if os.path.exists(n) else 0
open(n, "w").write(str(i + 1))
codes = s["curl"]
sys.stdout.write(codes[min(i, len(codes) - 1)])
'''
D, IP = "liquidgov.us", "143.198.159.58"
NS = ["ns1.digitalocean.com", "ns2.digitalocean.com", "ns3.digitalocean.com"]


def healthy():
    d = {"NS %s 1.1.1.1" % D: [n + "." for n in NS]}
    for s in NS + ["1.1.1.1", "8.8.8.8"]:
        d["A %s %s" % (D, s)] = [IP]
    return {"dig": d, "curl": ["200"]}


tmp = tempfile.mkdtemp(prefix="uptime-check-")
try:
    dig_py, curl_py = os.path.join(tmp, "fake_dig.py"), os.path.join(tmp, "fake_curl.py")
    open(dig_py, "w").write(FAKE_DIG)
    open(curl_py, "w").write(FAKE_CURL)

    def run(scn, gh=False):
        d = tempfile.mkdtemp(dir=tmp)
        path = os.path.join(d, "scenario.json")
        json.dump(scn, open(path, "w"))
        env = dict(os.environ, UPTIME_SCENARIO=path, UPTIME_DIG=json.dumps([sys.executable, dig_py]),
                   UPTIME_CURL=json.dumps([sys.executable, curl_py]), UPTIME_RETRY_S="0")
        env.pop("GITHUB_ACTIONS", None)
        if gh:
            env["GITHUB_ACTIONS"] = "true"
        p = subprocess.run([sys.executable, SCRIPT, D], capture_output=True, text=True, timeout=120, env=env)
        return p.returncode, p.stdout + p.stderr

    code, out = run(healthy())
    check("K1 healthy -> ALL-PASS, exit 0", code == 0 and "VERDICT uptime: ALL-PASS" in out, out[-500:])

    s = healthy()
    for n in NS:
        del s["dig"]["A %s %s" % (D, n)]
    s["dig"]["NS %s 1.1.1.1" % D] = ["nunames41.dnsmaster.io.", "nunames42.dnsmaster.io."]
    s["dig"].pop("A %s 1.1.1.1" % D)
    s["dig"].pop("A %s 8.8.8.8" % D)
    s["curl"] = ["000"]
    code, out = run(s)
    check("K2 the 2026-11-21 outage (silent nameservers) -> U2 FAIL naming them, exit 1",
          code == 1 and re.search(r"^FAIL U2 .*nunames41\.dnsmaster\.io", out, re.M) is not None, out[-600:])

    s = healthy()
    s["dig"]["A %s %s" % (D, NS[2])] = ["10.0.0.1"]
    code, out = run(s)
    check("K3 one nameserver with a different address -> U2 FAIL", code == 1 and re.search(r"^FAIL U2 ", out, re.M) is not None, out[-500:])

    s = healthy()
    s["dig"]["A %s 8.8.8.8" % D] = []
    code, out = run(s)
    check("K4 a public resolver with no answer -> U3 FAIL", code == 1 and re.search(r"^FAIL U3 ", out, re.M) is not None
          and re.search(r"^PASS U2 ", out, re.M) is not None, out[-500:])

    s = healthy()
    s["curl"] = ["000", "200"]
    code, out = run(s)
    check("K5 a slow first load then 200 -> PASS after the retry", code == 0 and re.search(r"^PASS U4 ", out, re.M) is not None, out[-400:])
    s["curl"] = ["503", "503"]
    code, out = run(s)
    check("K5 503 twice -> U4 FAIL listing both tries", code == 1 and re.search(r"^FAIL U4 .*'503', '503'", out, re.M) is not None, out[-400:])

    code, out = run(s, gh=True)
    check("K6 on GitHub Actions a FAIL is also an ::error:: annotation",
          re.search(r"^::error title=liquidgov\.us is not reachable by name::U4 ", out, re.M) is not None, out[-400:])
finally:
    shutil.rmtree(tmp, ignore_errors=True)

wf = open(WORKFLOW, encoding="utf-8").read() if os.path.exists(WORKFLOW) else ""
m = re.search(r"^on:\s*\n(.*?)(?=^\S)", wf, re.M | re.S)
on = m.group(1) if m else ""
check("K7 uptime.yml runs every 15 min and by hand, no paths filter, and runs scripts/uptime_check.py",
      "cron: '*/15 * * * *'" in on and "workflow_dispatch" in on and not re.search(r"^\s+paths", on, re.M)
      and re.search(r"python3 scripts/uptime_check\.py", wf) is not None, on or WORKFLOW)

print("uptime_check_check: %d failure(s)" % fails)
sys.exit(1 if fails else 0)
