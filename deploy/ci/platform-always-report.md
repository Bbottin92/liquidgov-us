# platform.yml: report a result on every commit

**Problem.** `platform.yml` runs only when a push touches `liquidgov-platform/**`, `scripts/lgcheck.sh` or the
workflow itself. A commit outside those paths (hooks, docs, `deploy/`) gets no platform run at all, and
`ship.py check` stops with "NO platform run for <sha>", because it reads the platform run for exactly the branch head
(the right rule). Until now the workaround was a filler commit inside `liquidgov-platform/` (wip/63's `c8e7ffd`;
wip/64's `81be56e` hit the same wall).

**Standard fix.** Required checks report on every commit; a path filter on the trigger is what GitHub's docs warn
against, because filtered commits never get a status. So: the trigger has no path filter, and a small first job decides
whether the platform changed. When it did not, the heavy job is skipped and the run concludes **success** in seconds.
When it did, the full build and tests run as today.

**What "changed" means** (so a skip is never a weaker check):
- push to `master`: the files changed by the push (`github.event.before..github.sha`); a brand-new ref counts as
  changed;
- any other branch, and pull requests: the files the branch changes against `master` (`git diff origin/master...HEAD`,
  the merge base). So every head of a branch that touches the platform gets the full run, a hooks- or docs-only
  commit on top of it included. Only a branch with no platform changes at all against master is skipped, and then
  `ship.py`'s phone VERDICT step is not required either (it requires ALL-PASS only when site files change).
- The paths are the same three as today's filter.

## The edits (someone with workflow access applies them; this session's token cannot write `.github/workflows/`)

1. Replace the `on:` block with:

```yaml
on:
  push:
    branches: [master, "wip/**"]
  pull_request:
```

2. Add this job as the FIRST entry under `jobs:` (before `check:`):

```yaml
  changes:
    runs-on: ubuntu-24.04
    timeout-minutes: 5
    outputs:
      platform: ${{ steps.diff.outputs.platform }}
    steps:
      - uses: actions/checkout@v5
        with:
          fetch-depth: 0
      - id: diff
        name: did the platform change?
        env:
          EVENT: ${{ github.event_name }}
          REF: ${{ github.ref }}
          BEFORE: ${{ github.event.before }}
        run: |
          set -euo pipefail
          if [ "$EVENT" = push ] && [ "$REF" = refs/heads/master ]; then
            if [ -z "$BEFORE" ] || [ "$BEFORE" = 0000000000000000000000000000000000000000 ]; then
              files="new ref"
            else
              files=$(git diff --name-only "$BEFORE" HEAD)
            fi
          else
            git fetch -q origin master
            files=$(git diff --name-only origin/master...HEAD)
          fi
          if [ "$files" = "new ref" ] || printf '%s\n' "$files" | grep -qE '^(liquidgov-platform/|scripts/lgcheck\.sh$|\.github/workflows/platform\.yml$)'; then
            echo "platform=true" >> "$GITHUB_OUTPUT"; echo "platform changed: full build and tests"
          else
            echo "platform=false" >> "$GITHUB_OUTPUT"; echo "no platform change against the base: build and tests skipped, run reports success"
          fi
```

3. On the existing `check:` job, directly under `check:`, add:

```yaml
    needs: changes
    if: needs.changes.outputs.platform == 'true'
```

Nothing inside `check:`'s steps changes (`workflow_browser_steps_check.py` reads them; they stay as they are).

## How to verify after applying

- Push the workflow change to a wip branch: the workflow file itself changed, so `changes` says true and the full run
  happens (as today).
- A docs-only commit on a branch that has platform changes: full run (merge-base rule).
- A branch with no platform changes (e.g. hooks only, based on current master): the run finishes in well under a minute,
  `check` shows as skipped, the run's conclusion is `success`, and `ship.py check` passes step 4.
- Apply together with `platform-ccache.md` (same file, independent edits) so it is one workflow update.

## Undo

Restore the previous `on:` block with its `paths:` lists, delete the `changes` job and the two lines under `check:`.
