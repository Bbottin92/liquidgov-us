# Prebuilt deploys: build on GitHub, ship the finished program

Brandon 2026-11-13: "industry standard" (production servers don't compile). Today every `<name>-go.sh` syncs
`liquidgov-platform/` and the server runs `cmake --build /opt/liquidgov/build -j1`, one file at a time on a small
droplet. This plan replaces that one step with "install a binary GitHub already built and tested". Nothing here is
used until Brandon says so; the compile-on-server path stays the default and the fallback.

## 1. The artifact

Built by `scripts/build_release.sh` (Release, stripped, `-static-libstdc++ -static-libgcc`, libsodium linked
statically when `libsodium.a` is present) into `dist/`:

| File | What |
|---|---|
| `liquidgov_http` | the server program |
| `liquidgov_http.sha256` | `sha256sum` of it |
| `BUILD-INFO` | `commit=`, `tree=` (the `liquidgov-platform` tree hash, the same key `deploy/ci-gate.sh` matches CI runs by), `os=` (the runner's `/etc/os-release` PRETTY_NAME), `glibc_needed=` (the highest `GLIBC_x.y` symbol version the binary references, from `objdump -T`) |

The program itself answers `liquidgov_http --version` with `liquidgov_http <commit> tree <tree>` (compile
definitions `LG_BUILD_COMMIT` / `LG_BUILD_TREE`; a local build says `unknown`), so the server can check it is
running exactly what was meant.

GitHub workflow `release-binary.yml` (in `deploy/ci/` until someone with workflow access moves it to
`.github/workflows/`; this session's token cannot write workflow files): on every push to `master` that changes
`liquidgov-platform/**`, and by hand (`workflow_dispatch`), on `ubuntu-24.04` (the image `platform.yml` is already
pinned to), runs `build_release.sh` and uploads `dist/` as the artifact `liquidgov_http-<tree>` with
`retention-days: 14`.

## 2. How a go fetches and verifies it

On Brandon's PC, before the go (no server contact): `deploy/fetch-prebuilt.sh` finds the newest successful
`release-binary` run whose head commit has the same `liquidgov-platform` tree hash as HEAD (the same rule as
`ci-gate.sh`), downloads its artifact with `gh run download`, and refuses unless:
1. `sha256sum -c liquidgov_http.sha256` passes,
2. `BUILD-INFO`'s `tree=` equals HEAD's tree,
3. `./liquidgov_http --version` (run in WSL) prints that same tree.
It leaves the three files in `liquidgov-platform/.prebuilt/`, which the normal sync then copies to
`/opt/liquidgov/.prebuilt/` (rsync, as today; the server pulls nothing from GitHub and needs no GitHub token).

On the server, the prebuilt remote script (instead of `cmake --build`):
1. `sha256sum -c` again (the copy that arrived), `--version` tree == the tree in the go script,
2. glibc check: `ldd --version` on the server must be >= `glibc_needed` (else refuse: "built on a newer system";
   fall back to the compile path),
3. `cp -p build/liquidgov_http liquidgov_http.pre-<name>` (as today), then `install -m 0755 .prebuilt/liquidgov_http
   build/liquidgov_http`, `systemctl restart liquidgov`, the same health checks as today.

## 3. ABI and libc

The droplet's OS version is not written down in this repo (only its address); this plan does not contact it to find
out. So the check is done at deploy time instead of assumed:
- libstdc++ and libgcc are linked statically, libsodium statically when available: the binary then needs only glibc
  (and the kernel), which is backward compatible: a binary built against glibc 2.39 (Ubuntu 24.04) runs on any
  system with glibc >= the newest symbol version it references (often lower than 2.39; `glibc_needed` says exactly).
- Measured (a local `LG_STATIC_RUNTIME=ON` Release build on Ubuntu 24.04, 2026-11-13): `ldd` lists only `libc` and
  `libm`; the newest symbol it needs is `GLIBC_2.38`. So the droplet needs glibc >= 2.38 (Ubuntu 24.04 has 2.39).
- Pin: build on `ubuntu-24.04` (already the CI image). If the droplet turns out older (e.g. 22.04, glibc 2.35) and
  the step-2 check refuses, switch the build job to `container: ubuntu:22.04` (one line) so `glibc_needed` <= 2.35.

## 4. The swap and rollback

Unchanged in shape: the old program is kept as `liquidgov_http.pre-<name>` before anything is replaced; a rollback
is `mv liquidgov_http.pre-<name> build/liquidgov_http` + `systemctl restart liquidgov` (plus the store strip the
rollback guard asks for, exactly as today).

## 5. Changes to the deploy tools

- `deploy/make-go.py`: `LIQUIDGOV_PREBUILT=1` picks the new `prebuilt` template pair instead of `care` for a change
  that needs a restart; everything else (naming, records, the dry run, the CI gate) is the same.
- `deploy/prebuilt-go.sh` / `deploy/prebuilt-remote.sh`: copies of the `care` pair with the build step replaced by
  the step-2 verification and install; the go script runs `fetch-prebuilt.sh` before the sync.
- `deploy/ci-gate.sh`: unchanged (it already gates on the platform run for the exact tree). The prebuilt go also
  requires `fetch-prebuilt.sh` to have found a release run for that tree.
- `deploy/rollback_guard.py`: unchanged; it reads the store source of the live and new commits, which is the same
  whether the binary was built on GitHub or on the server.

## 6. What stays the same

The store file and its backups (`.pre-<name>-<time>`), the sync's exclusions and allow-list, nginx, systemd, the
dry run and "go" words, the CI gate, the rollback guard, ship.py's checks.

## 7. Failure cases

| Case | What happens |
|---|---|
| No release run for this tree (not merged yet, or the run failed) | fetch refuses; deploy with the compile path or wait |
| Checksum or tree mismatch (PC or server) | refuse before anything changes |
| Server glibc older than `glibc_needed` | refuse before anything changes; use the compile path; move the build to an older container |
| The new program does not start | the same as today: `.pre-<name>` is still there; restore it and restart |
| GitHub down | compile path (unchanged) |

## 8. Cost

- Actions minutes: one Release build per master push that touches `liquidgov-platform/` (about 5 minutes on 2 cores
  today; less with the precompiled headers of wip/62). At ~2-3 site deploys a day that is ~300-450 min/month, inside
  the 2,000 free minutes a private repo gets (the platform workflow already uses most of its share per push; watch
  the total in Settings > Billing).
- Artifact storage: the stripped binary is a few MB; 14-day retention keeps well under the 500 MB free storage.

## 9. What is built (wip/63)

`scripts/build_release.sh`; `deploy/ci/release-binary.yml` (to move into `.github/workflows/`); `deploy/fetch-prebuilt.sh`;
the template pair `deploy/prebuilt-go.sh` / `deploy/prebuilt-remote.sh`; `deploy/make-go.py` (`LIQUIDGOV_PREBUILT=1`);
`naming.toml` (prebuilt-remote may restart); `.gitignore` (`.prebuilt/`, `dist/`, `build-release/`); CMake options
`LG_STATIC_RUNTIME`, `LG_BUILD_COMMIT`, `LG_BUILD_TREE`; `liquidgov_http --version`. Tests: `tests/repo/prebuilt_check.py`,
`liquidgov-platform/tests/e2e/version_flag.sh`.

## 10. Not done here

Not wired into `ship.py` (Brandon's call after reading this). Not tried against the real server (not contacted).
The droplet's OS version is unknown to this repo; the first prebuilt deploy's glibc check will show it.
