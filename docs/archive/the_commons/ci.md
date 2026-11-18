# GitHub Action — live at `.github/workflows/commons.yml`

Runner is **`ubuntu-24.04`** (807b4bd), not `ubuntu-latest`, so the
2026-10-19 Ubuntu 26 cutover does not change cmake or nginx. No secrets.
`fetch-depth: 0`. Do not SSH. Do not compile civic.

`actions/checkout@v4` stays. Node 20 annotation is noise; GitHub already
runs it on 24. Bump to `@v5` only as a named one-liner.

## What it gates

1. `commons-daemon` configures + links (FetchContent sqlite 3.53.4).
2. `schema.sql` matches `docs/archive/the_commons/schema.sql`.
3. Exact `GET /presence/inventory` and `GET /presence/lots` **before**
   the prefix `GET /presence/`.
4. `nginx-commons.conf` parses inside a wrap that defines
   `$connection_upgrade`.
5. Same-commit `http_server.cpp` **and** `commons/index.html` needs
   `civic` or `deploy` in the subject.

## YAML (keep in sync with the workflow file)

```yaml
name: commons

on:
  push:
    branches: [master]
  pull_request:

permissions:
  contents: read

jobs:
  daemon:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v5
        with:
          fetch-depth: 0
```

The remaining steps are in `.github/workflows/commons.yml`. Do not paste
a second copy that can drift.
