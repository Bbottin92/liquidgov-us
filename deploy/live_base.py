#!/usr/bin/env python3
"""Which branch should new work be cut from? (q91, 2026-11-17)
Live liquidgov.us has run code that master did not have (the wip/136-139 chain: "live ahead of GitHub"); work started from
master then lacked the live code. This reads the live commit (the first line of DEPLOYED_COMMIT on the server) and says:
  exit 0  master contains it: cut from master.
  exit 1  master does not: it names the local branches that contain it and prints the command to cut a worktree from the
          tip that holds the rest of them.
  exit 2  it could not tell (bad id, commit not in this repo, no master branch, server not reachable): always with a message.
Usage: python deploy/live_base.py [--commit <sha>] [repo root]
  no --commit: read it over ssh like deploy/sync-to-droplet.sh (LIQUIDGOV_DEPLOY_SSH, LIQUIDGOV_DEPLOY_KEY; host
               root@143.198.159.58, file /opt/liquidgov/DEPLOYED_COMMIT). Read-only: one ssh cat.
Tests: tests/repo/live_base_check.py."""
import os
import re
import shlex
import subprocess
import sys

HOST, FILE = "root@143.198.159.58", "/opt/liquidgov/DEPLOYED_COMMIT"


def git(root, *args):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    r = subprocess.run(["git", "-C", root] + list(args), capture_output=True, text=True, env=env, stdin=subprocess.DEVNULL)
    return r.returncode, r.stdout.strip()


def live_commit_from_server():
    key = os.environ.get("LIQUIDGOV_DEPLOY_KEY") or os.path.join(os.path.expanduser("~"), ".ssh", "liquidgov_deploy_key")
    ssh = shlex.split(os.environ.get("LIQUIDGOV_DEPLOY_SSH") or "ssh -i " + key)
    try:
        r = subprocess.run(ssh + [HOST, "cat " + FILE], capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL)
    except (OSError, subprocess.TimeoutExpired) as e:
        return None, "cannot reach the server (%s: %s)" % (type(e).__name__, e)
    lines = r.stdout.split()
    if r.returncode != 0 or not lines:
        return None, "could not read %s on the server (exit %d): %s" % (FILE, r.returncode, r.stderr.strip()[:200])
    return lines[0], ""


def main(argv):
    args, commit = [], None
    it = iter(argv)
    for a in it:
        if a == "--commit":
            commit = next(it, "")
        else:
            args.append(a)
    root = os.path.abspath(args[0]) if args else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if commit is None:
        commit, err = live_commit_from_server()
        if commit is None:
            print("live_base: " + err)
            return 2
    if not re.fullmatch(r"[0-9a-f]{7,40}", commit or ""):
        print("live_base: %r is not a commit id (7 to 40 hex digits)" % (commit,))
        return 2
    if git(root, "cat-file", "-e", commit + "^{commit}")[0] != 0:
        print("live_base: the live commit %s is not in this repo (fetch it, or this is the wrong clone)" % commit[:12])
        return 2
    if git(root, "rev-parse", "--verify", "-q", "refs/heads/master")[0] != 0:
        print("live_base: no local branch named master here, so I cannot say whether master contains %s" % commit[:12])
        return 2
    if git(root, "merge-base", "--is-ancestor", commit, "master")[0] == 0:
        print("live_base: master contains the live commit %s: cut new work from master" % commit[:12])
        return 0
    rc, out = git(root, "branch", "--contains", commit, "--format=%(refname:short)")
    branches = [b for b in out.splitlines() if b]
    if rc != 0 or not branches:
        print("live_base: the live commit %s is in this repo but on no local branch (a detached or remote-only commit): "
              "make a branch at it first" % commit[:12])
        return 2
    # the tip that holds the rest: every other branch's tip is its ancestor
    tip = next((b for b in branches if all(git(root, "merge-base", "--is-ancestor", o, b)[0] == 0 for o in branches)), None)
    if tip is None:
        tip = max(branches, key=lambda b: int(git(root, "rev-list", "--count", b)[1] or 0))
    print("live_base: master does NOT contain the live commit %s (live runs ahead of master)." % commit[:12])
    print("branches that contain it: " + ", ".join(branches))
    print("cut new work from the tip that holds the rest, then merge master into it if master has commits it lacks:")
    print("git worktree add -b wip/<NN>-<name> ../liquidgov-us-<name> " + tip)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
