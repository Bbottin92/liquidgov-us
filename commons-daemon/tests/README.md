# commons-daemon regression tests

Real assertions, run in CI on every push and pull request (`.github/workflows/commons.yml`,
step "daemon regression suite (civic stub)").

```
commons-daemon/tests/run_all.sh                  # every test against the civic stub (what CI does)
CIVIC_MODE=real  commons-daemon/tests/run_all.sh # the same tests against the real liquidgov_http
commons-daemon/tests/run_all.sh test_guest       # one test
```

Needs the built daemon (`commons-daemon/build/commons_daemon`, or `COMMONS_DAEMON_BIN`),
`python3`, `curl`, `rsync`, `git`, `g++`, and (optionally, always in CI) `nginx` and `node` (`test_reqjson` compiles its unit cases; `test_deploy` drives the deploy scripts). `strace` is optional (one extra check, skipped when absent).

## Why a civic stub

The daemon only ever talks to civic over loopback `:8080`, and civic cannot be compiled on a
GitHub runner (droplet-only dependencies). `civic_stub.py` is a small in-memory stand-in for
exactly the routes the daemon and the tests use: signup/friends/block/delete on the public side
and civic's four same-box `/internal/*` routes with civic's semantics (including "fails closed").

**The stub is kept honest** by `test_civic_contract.sh` and by running the whole suite against
the real civic (`CIVIC_MODE=real`): both must pass. If civic's `/internal/*` behaviour changes,
change the stub and re-run the suite in both modes.

## The tests

| file | what it pins |
|---|---|
| `test_civic_contract` | the `/internal/*` contract (same-box gate, fail-closed `blocked`, `user-exists` never false for a malformed name) |
| `test_guest` | OS-random guest secrets, the 256 cap (evict, never refuse), bad-secret 403, socket timeouts (a stalled client cannot wedge the loop) |
| `test_cookie_bind` | a bound id comes from the session; rekey leaves no ghost; rebind relabels a guest in place; guards |
| `test_house` | durable wall + resume across a real restart, owner-only placement, inventory, RAM-only demo house, checked-SQLite (500, rollback, daemon survives) |
| `test_visiting` | friends visiting rules and block eviction after the 15s cache |
| `test_policy` | friends/locked/open setter, live effect, lots, persistence |
| `test_open` | open houses, blocks in both directions, discovery, fail-closed with civic frozen |
| `test_civic_caches` | session/friends caches: a real answer is cached, a failure is never cached (timing with a frozen civic proves hit vs asked) |
| `test_snapshot` | kill -9 crash recovery from the periodic snapshot (with a control that proves the test can tell) |
| `test_orphans` | the reap and every valve (two passes, cap 3, backup first, last look, SUSPECT, kill switch, online skipped) |
| `test_migrations` | startup migrations on databases from OLDER schemas (`fixtures/` are frozen from git history) |
| `test_netmint` | the client's guest-credential mint: the shipped `netMint` block run under node with a fake clock and scripted fetch (88 checks): credential only from a 200 `{id, secret}`, backoff + Retry-After, single-flight, re-auth |
| `test_retention` | backup retention: 378 unit checks on the pure decision (incl. 300 random directories with sidecars and orphans) + the real script against a fake backups directory with real `-wal`/`-shm` files (dry run by default, exact deletions, sidecar pairing, newest-first, refusals) |
| `test_nginx_guest` | the `/guest` rate limit: config text checks (anywhere) + a real nginx in front of the real daemon (CI): burst absorbed, then 429, per address, only minting limited, refills |
| `test_deploy` | the deploy scripts against a FAKE droplet (temp dir behind a shim ssh, stub systemctl/cmake): drift, dirty/unpushed refusals, backup, -j1 build, ordered `--go`, verify |
| `test_reqjson` | request fields are found as KEYS: 200+ parser cases (`reqjson_unit.cpp`) and name-first bodies through the daemon (a guest named `x` joins) |
| `test_harness` | the harness itself: `stop_daemon` kills what a wrapper (strace) launched; `start_daemon` refuses when something already answers on the port |

## Writing a test

Source `lib.sh`; it starts/stops civic and the daemon in a temp dir and cleans up. Two rules the
suite learned the hard way: **print/assert the precondition** (that the accounts and rows the test
acts on really exist) before trusting a "nothing bad happened" result, and **start the daemon
before creating rows through it**. A timing assertion against a frozen civic (`freeze_civic` /
`thaw_civic`) is how "was civic asked?" is proven.
