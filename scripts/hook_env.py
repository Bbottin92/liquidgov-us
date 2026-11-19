"""hook_env.py: the one environment for a check that runs the REAL git hooks in a throwaway repo (2026-11-19).
Not a check itself (no _check.py suffix). A check that sets core.hooksPath in its fake repo builds its environment
with clean_env(), so nothing the outer hook set leaks into the inner hooks:
  - GIT_* (GIT_DIR, GIT_INDEX_FILE ... from a linked worktree's hook): the fake repo's git calls hit the real repo
    (2026-11-12: prebuilt_check.py committed "one"/"two" onto the real branch and set core.bare=true)
  - LG_CHECK_MODE: pre-commit runs pre-push with LG_CHECK_MODE=--staged, so the fake repo's pushes picked checks by
    staged files (0) instead of every check (2026-11-19: hook_linux_only_skip_check L4 failed only under the hook)
hook_env_isolation_check.py fails when a check that sets core.hooksPath does not use this."""
import os

DROP_PREFIXES = ("GIT_",)
DROP_NAMES = ("LG_CHECK_MODE",)


def clean_env(base=None):
    """a copy of base (default os.environ) without the hook-inherited variables, plus no bytecode files."""
    src = os.environ if base is None else base
    env = {k: v for k, v in src.items() if not k.startswith(DROP_PREFIXES) and k not in DROP_NAMES}
    env["PYTHONDONTWRITEBYTECODE"] = "1"  # the hooks load run_hook_checks.py: no scripts/__pycache__ in the fake tree
    return env
