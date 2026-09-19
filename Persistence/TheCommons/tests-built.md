# Tests in CI — built, locally verified, CI is the last check

Brandon: "BUILD tests in CI" (`GROK_NEXT.md` #1: the suite that proved reap, friends, open and
hardening lived in a scratchpad; CI only compiled). **No droplet, no nginx, no daemon/civic/
client code change, no deploy.** Files: `commons-daemon/tests/` and one workflow step.

## What ships

- **`commons-daemon/tests/`**: 12 test files, about **310 real assertions** (pass/fail, not
  output to eyeball), a shared harness (`lib.sh`), `run_all.sh`, frozen old-schema fixtures, a
  `README.md`.
- **`civic_stub.py`**: a stdlib-only in-memory stand-in for civic: signup / friends / accept /
  unfriend / block / delete on the public side, and civic's four same-box `/internal/*` routes
  (`session-user`, `friends`, `blocked/<owner>`, `user-exists/<name>`) with civic's semantics,
  including "fails closed" and the `X-Forwarded-For` 404.
- **The stub is kept honest**: `test_civic_contract` pins the routes' behaviour, and **the whole
  suite passes against BOTH the stub and the real `liquidgov_http`** (`CIVIC_MODE=real`), locally,
  identically (209s and 214s). If civic's `/internal/*` ever changes, change the stub and re-run in
  both modes.
- **Workflow (`.github/workflows/commons.yml`)**: a new step "daemon regression suite (civic
  stub)" after the daemon build (15 min step limit, 25 min job limit), which installs `strace` and
  runs `run_all.sh`. It only starts local processes on loopback: no droplet, no secrets, no nginx.
  Each test file also has a hard per-file timeout, so a regression that hangs the daemon FAILS
  instead of stalling the job.

| test | pins |
|---|---|
| `civic_contract` | the `/internal/*` contract (21) |
| `guest` | `getrandom` secrets (via `strace` when available), the 256 cap (evict, never refuse), bad-secret 403, socket timeouts (29) |
| `cookie_bind` | bound ids from the session, rekey no ghost, rebind in place + guards (29) |
| `house` | durable wall + resume across a real restart, owner-only, inventory, RAM demo, checked-SQLite 500/rollback (37) |
| `visiting` | friends rules, block eviction after the cache window (22) |
| `policy` | friends/locked/open setter, live effect, lots, persistence (27) |
| `open` | open houses, both-direction blocks, discovery, fail-closed (29) |
| `civic_caches` | a real answer is cached, a failure never is; timing with a frozen civic proves "asked" (23) |
| `snapshot` | kill -9 recovery, with a control (7) |
| `orphans` | the reap and every valve incl. backup-first, last look, SUSPECT, kill switch (48) |
| `migrations` | startup migrations on databases from two OLDER schemas, frozen from git history (28) |
| `harness` | the harness itself: no daemon survives `stop_daemon`/`crash_daemon` even under a forking wrapper; `start_daemon` refuses a busy port (8) |

## Does it actually catch regressions? (mutation check)

Yes, checked, not assumed. With the **pre-hardening** daemon source temporarily put back
(`git checkout 3e46981 -- commons-daemon/src`, rebuilt, then restored; the tree was clean before
and after), the suite fails exactly where it should: the registry-cap tests (two), the
friends-cache tests (five: a visitor walked out on a civic hiccup, lots pinned dark, ...), and the
three socket-timeout tests. The first version of that check exposed a real weakness in the suite:
against a daemon with no timeouts the guest test *hung* (the stalled client blocked the daemon
forever and my probe never returned). Probes now use `curl --max-time`, so it fails in ~90s, and
every test file has a hard limit.

## Findings while writing it (all logged, none needing a code change)

- Three of my own test drafts were wrong and were caught by their preconditions, not trusted:
  a forged-id test used a secret value (`x`) equal to a JSON key name; a `dbq` helper printed
  `None` for SQL NULL; and two orphan sections created their rows *before* the daemon was up (so
  "row is gone" meant "never created"). The suite now asserts each section's precondition.
- `reqjson.hpp` is a naive key scan: a hand-forged body whose *value* equals a later key name
  (e.g. `"secret":"x"` then the `x` coordinate) is read wrongly and answers 400. Harmless: the real
  client sends random-hex ids/secrets first and the typed name last, and a body is only ever
  authored by its own sender, so nothing is gained by forging one. Noted, not a bug to fix.

## What the first CI run found (d9d1b51 failed; fixed in the next commit)

10 of 11 files passed on the runner, including the `strace`/`getrandom` check (the branch that could
not run locally) and every timing bound. `test_house` failed 7 assertions, and the cause was the
HARNESS, not the daemon: `test_guest` starts the daemon under `strace -f`, `stop_daemon` killed only
the `strace` wrapper, so the traced daemon survived holding `:8765`. The next file's new daemon died on
`bind()` while `start_daemon`'s health check passed against the survivor, so `test_house` talked to a
different daemon with a different (already deleted) database: DB reads came back empty, injected SQL
triggers never applied. Locally there is no `strace`, so that branch had never run. Fixes:
`stop_daemon`/`crash_daemon` kill the wrapper's children too and then wait for the port to go silent;
`start_daemon` FATALs if anything already answers on the port and if the new process exits during
start; `run_all.sh` sweeps `commons_daemon` and the stub between files whatever the result; and
`test_harness.sh` pins those rules (mutation-checked against the old `lib.sh`). Same lesson as the
precondition rule: a health check proves *a* daemon answered, not *your* daemon.

## Limits, said plainly

- The `getrandom` check needs `strace` (CI installs it); here it is not installed, so that branch
  only ever runs in CI (it did, and passed, on the first run). It skips cleanly if ptrace is denied.
- Timing assertions (a frozen civic costs its 1s timeout; "instant" means < 0.6s) have margins,
  but shared CI runners are slower than this machine. If one flakes, widen its bounds rather than
  deleting the check.
- The suite takes ~3.5 minutes and uses fixed ports (`8080`, `8765`), so tests run one at a time.
- Not covered: the client (`index.html`, no headless browser test), civic itself, nginx.

## Not here

Deploy scripts, nginx `limit_req`, backup retention, splitting `index.html`, `/health` hash:
still unbuilt names.
