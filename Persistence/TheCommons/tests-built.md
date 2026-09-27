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
| `deploy` | the deploy scripts against a fake droplet: drift, refusals, backup, -j1 build, ordered `--go`, verify (119) |
| `nginx_guest` | the `/guest` rate limit: config text + a real nginx in front of the daemon (CI) |
| `retention` | backup retention: 378 unit checks + the script against a fake backups directory incl. sidecars (76) |
| `netmint` | the client's guest-credential mint (the shipped block under node, fake clock + fetch): 88 checks |
| `health` | `/health`'s commit: the generator's rules, the no-reconfigure refresh on the real build dir, the endpoint (59) |
| `client_files` | the split client's load order: the real files evaluated in tag order under a stubbed DOM, effects checked, wrong orders must throw; BUILD studio 3: `STUDIO_GATHER_SPOTS`/`LAB_RECIPES` match the catalog, bench/notebook/gather entry points exist, a guest never calls them (96) |
| `netstatus` | the connection badge: the shipped state machine (fake clock, 31 checks) + its wiring, markup and CSS (29) |
| `world_scatter` | the extra region props: refused if not plain ground, none on the spine / seams / doors / labels / spawn, flood fill leaves nothing sealed; BUILD studio 3: the thirteen studio-gather markers and the workshop bench/notebook, each clear of every other prop's 1-tile margin (31 + wiring, 9 mutations) |
| `pwa` | Commons manifest + icons, Social/Civic manifests (Mail control), start_url = the shells' redirect targets, the picker, sync excludes (71 checks, 16 mutations) |
| `nginx_www` | www -> apex: the transform on the live config copy (34, 9 mutations) + a real nginx's redirects with a control (17) |
| `studio` | the Studio's frozen catalog (acyclic, no stoichiometry, C1's ash gone), gather/bench/mix/collect server-authoritative incl. a multi-input partial-rollback case, mix's rate limit; BUILD studio 3: a discovery's own display_name/real_range_note/history_note, recipe_count (92 checks, 2 mutations) |
| `studio_orphans` | the reap extension: a reaped account's batches/discoveries/cooldowns are gone, a kept one's are not (10 checks, 1 mutation: the reap ORDER, studio before house) |
| `painting` | BUILD studio 2: the portrait stub, visit-checked read, ceil cost with no refund, locked index (60 checks, 2 mutations) |
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

## Findings while writing it (one, `reqjson`, is a real latent bug: see below)

- Three of my own test drafts were wrong and were caught by their preconditions, not trusted:
  a forged-id test used a secret value (`x`) equal to a JSON key name; a `dbq` helper printed
  `None` for SQL NULL; and two orphan sections created their rows *before* the daemon was up (so
  "row is gone" meant "never created"). The suite now asserts each section's precondition.
- `reqjson.hpp` is a naive key scan (`body.find("\"key\"")`), so a *value* equal to a field name matches
  first. I first called that harmless; **Grok was right that it is a latent bug**. Reproduced live: with
  `display_name` placed BEFORE the fields, a guest named `x`/`y` gets 400 `missing_field`, `secret` gets 403,
  `id`/`facing` join under a wrong "(2)" name. The shipped client sends `display_name` LAST
  (`region,x,y,facing,display_name`), so all seven names joined fine today -- but that is luck of JS
  object-key order, not a guarantee. Not built yet; named as `BUILD reqjson` (see `GROK_TESTS_REVIEW.md`).

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
