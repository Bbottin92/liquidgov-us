"""hook_env_isolation_check.py: every check that runs the real git hooks in a throwaway repo builds its environment
with scripts/hook_env.py's clean_env() (2026-11-19; lesson editing-tests.md:75, seen 3 times).
Why: each such check once copied its own filter, and a variable the outer hook set leaked into the inner hooks
(GIT_* on 2026-11-12, LG_CHECK_MODE on 2026-11-19). One helper, and this check that nobody goes around it.
  E1 clean_env drops GIT_DIR, GIT_INDEX_FILE and LG_CHECK_MODE, keeps PATH and other LG_* variables
  E2 every tests/repo/*.py that sets core.hooksPath imports hook_env and calls clean_env, and builds no filter of its
     own (no 'if not k.startswith("GIT_")' in it)
  E3 precondition: at least one such check was found (a scan that finds nothing passes vacuously)
Usage: python tests/repo/hook_env_isolation_check.py <repo root>   (exit 0 only when every case passed)"""
import os
import re
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.join(ROOT, "tests", "repo")
sys.path.insert(0, os.path.join(ROOT, "scripts"))  # scripts/hook_env.py (tests/repo holds *_check.py only: naming.toml)
checks = failed = 0


def check(ok, what, detail=""):
    global checks, failed
    checks += 1
    print(("PASS " if ok else "FAIL ") + what + ("" if ok else "  :: %s" % (str(detail)[:400],)))
    failed += not ok


try:
    import hook_env
except ImportError as e:  # err-ok: reported as a FAIL, and every E2 case below still runs
    hook_env = None
    check(False, "E1 scripts/hook_env.py imports", e)

if hook_env:
    base = {"PATH": "/bin", "GIT_DIR": "/x/.git", "GIT_INDEX_FILE": "/x/i", "LG_CHECK_MODE": "--staged",
            "LG_CHECKS_PLATFORM": "win32", "HOME": "/h"}
    env = hook_env.clean_env(base)
    gone = [k for k in ("GIT_DIR", "GIT_INDEX_FILE", "LG_CHECK_MODE") if k in env]
    check(not gone, "E1 clean_env drops GIT_DIR, GIT_INDEX_FILE, LG_CHECK_MODE", gone)
    check(env.get("PATH") == "/bin" and env.get("LG_CHECKS_PLATFORM") == "win32" and env.get("HOME") == "/h",
          "E1 clean_env keeps PATH, HOME and other LG_* variables", env)
    check("GIT_DIR" in base, "E1 clean_env does not change the mapping it was given", base)

SETS_HOOKS = re.compile(r"""["']core\.hooksPath["']\s*,""")  # a git config call, not a mention in a docstring
users = []
for name in sorted(os.listdir(HERE)):
    if not name.endswith(".py") or name == os.path.basename(__file__):
        continue
    with open(os.path.join(HERE, name), encoding="utf-8") as f:
        text = f.read()
    if not SETS_HOOKS.search(text):
        continue
    users.append(name)
    uses = re.search(r"(?m)^\s*(import hook_env|from hook_env import)", text) and "clean_env(" in text
    check(bool(uses), "E2 %s (sets core.hooksPath) builds its env with hook_env.clean_env()" % name)
    # a filter is "if not k.startswith('GIT_')"; a probe that LISTS what it sees (hook_git_env_check's) is not one
    own = re.search(r"""if not \w+\.startswith\(["']GIT_""", text)
    check(not own, "E2 %s has no hand-made GIT_* filter of its own" % name)
check(len(users) >= 1, "E3 precondition: found the checks that set core.hooksPath (%s)" % ", ".join(users), users)
print("hook_env_isolation_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
