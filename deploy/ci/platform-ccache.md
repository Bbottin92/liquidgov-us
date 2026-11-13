# CI build cache (ccache) for platform.yml

The build already uses ccache when it is installed (wip/62: `find_program(LG_CCACHE ccache)` in CMakeLists.txt), but the
GitHub runner does not have it, so every push compiles all ~107 files from scratch (~70 s). These three edits to
`.github/workflows/platform.yml` keep the compiled objects between runs; a push that changes a few files then rebuilds
only those. This session's GitHub token cannot write workflow files, so someone with workflow access applies them.

1. Under `jobs.check`, before `steps:`:

```yaml
    env:
      CCACHE_DIR: ${{ github.workspace }}/.ccache
      CCACHE_MAXSIZE: 500M
      CCACHE_BASEDIR: ${{ github.workspace }}
```

2. In the step "tools the build and the suites call", change the install line to:

```yaml
          sudo apt-get install -y -qq libsodium-dev pkg-config ccache
```

3. A new step right before "build":

```yaml
      - name: restore the compile cache
        uses: actions/cache@v4
        with:
          path: .ccache
          key: ccache-${{ runner.os }}-${{ github.ref_name }}-${{ github.sha }}
          restore-keys: |
            ccache-${{ runner.os }}-${{ github.ref_name }}-
            ccache-${{ runner.os }}-master-
            ccache-${{ runner.os }}-
```

`CCACHE_BASEDIR` makes the cache hit even though each run checks out into the same path; the restore keys fall back to
this branch's last run, then master's. Cache storage counts against the repo's 10 GB Actions cache (500 MB cap here).
`lgcheck.sh` prints `TIMING` lines on CI; compare the "build" step time before and after.
