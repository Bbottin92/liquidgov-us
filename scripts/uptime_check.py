#!/usr/bin/env python3
"""Is liquidgov.us reachable BY NAME from outside? (q295; 2026-11-20 and 2026-11-21 the site was down for everyone
while the server was fine: the domain's nameservers at NuNames stopped answering, and nothing told anyone.)

Runs from a GitHub-hosted runner every 15 minutes (.github/workflows/uptime.yml), so it sees what a visitor sees.
Each look, in order:
  U1  public DNS knows the domain's nameservers (dig NS via 1.1.1.1)
  U2  EVERY one of those nameservers answers an A record for the domain itself (the 2026-11-21 failure: the
      delegated nameservers were silent), and they all give the same address
  U3  1.1.1.1 and 8.8.8.8 resolve the domain to that same address
  U4  https://<domain>/ loads by name with HTTP 200 (two tries, 10 s apart, so one slow answer is not an outage)
No address is written down here: the nameservers' own answer is the expected one, so a planned server move does not
need this file changed.
Prints PASS/FAIL lines and "VERDICT uptime: ALL-PASS" or the failures; on GitHub Actions each FAIL is also an
::error:: annotation. Exit 1 on a FAIL (the run turns red and GitHub emails about the failed scheduled run).
Usage: python3 scripts/uptime_check.py [<domain>]      (default liquidgov.us)
The tests swap the tools with UPTIME_DIG / UPTIME_CURL (JSON lists) and UPTIME_RETRY_S."""
import json
import os
import re
import subprocess
import sys
import time

DIG = json.loads(os.environ["UPTIME_DIG"]) if os.environ.get("UPTIME_DIG") else ["dig"]
CURL = json.loads(os.environ["UPTIME_CURL"]) if os.environ.get("UPTIME_CURL") else ["curl"]
RETRY_S = float(os.environ.get("UPTIME_RETRY_S") or 10)
PUBLIC = ["1.1.1.1", "8.8.8.8"]
IPV4 = re.compile(r"^\d{1,3}(\.\d{1,3}){3}$")


def run(cmd, timeout=40):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout
    except (OSError, subprocess.SubprocessError) as e:
        return 99, "could not run %s: %s" % (cmd[0], e)


def dig(name, rtype, server):
    """The answer lines of dig +short (records only), or [] when the server gave nothing."""
    code, out = run(DIG + ["+short", "+time=3", "+tries=2", rtype, name, "@" + server])
    lines = [l.strip().rstrip(".") for l in out.splitlines() if l.strip() and not l.startswith(";")]
    return lines if code == 0 else []


def addresses(lines):
    return sorted({l for l in lines if IPV4.match(l)})


def main(argv):
    domain = argv[0] if argv else "liquidgov.us"
    fails, n = [], [0]
    gh = os.environ.get("GITHUB_ACTIONS") == "true"

    def check(name, ok, detail=""):
        n[0] += 1
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]), flush=True)
        if not ok:
            fails.append(name.split(" ", 1)[0])
            if gh:
                print("::error title=%s is not reachable by name::%s %s" % (domain, name, str(detail)[:300]), flush=True)

    ns = sorted(set(dig(domain, "NS", PUBLIC[0])))
    check("U1 public DNS knows %s's nameservers" % domain, bool(ns), {"via": PUBLIC[0], "ns": ns})
    auth = {}
    for s in ns:
        auth[s] = addresses(dig(domain, "A", s))
    answering = {s: a for s, a in auth.items() if a}
    expected = sorted({tuple(a) for a in answering.values()})
    check("U2 every nameserver answers for %s, all the same address" % domain,
          bool(ns) and len(answering) == len(ns) and len(expected) == 1,
          {"silent": sorted(set(ns) - set(answering)), "answers": auth})
    want = list(expected[0]) if len(expected) == 1 else None
    pub = {r: addresses(dig(domain, "A", r)) for r in PUBLIC}
    check("U3 %s resolve %s to the nameservers' address" % (" and ".join(PUBLIC), domain),
          want is not None and all(a == want for a in pub.values()), {"want": want, "got": pub})
    code_seen = []
    for attempt in range(2):
        code, out = run(CURL + ["-s", "-o", "/dev/null", "-w", "%{http_code}", "--max-time", "15", "https://%s/" % domain])
        code_seen.append(out.strip() or "exit %d" % code)
        if out.strip() == "200":
            break
        if attempt == 0:
            time.sleep(RETRY_S)
    check("U4 https://%s/ loads by name (200)" % domain, code_seen[-1] == "200", {"tries": code_seen})
    print("VERDICT uptime: %s  (%d checks)" % ("ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), n[0]), flush=True)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
