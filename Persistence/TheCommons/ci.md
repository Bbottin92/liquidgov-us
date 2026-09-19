# GitHub Action — spec, not installed

Brandon asked Grok to specify the YAML. Claude lands
`.github/workflows/commons.yml` from the block below when he says
**BUILD ci**. Do not SSH. Do not deploy. Do not compile civic
(`liquidgov-platform/` has droplet-only deps). Do not add
`credentials: include` tests.

## What it gates

1. `commons-daemon` configures + links (FetchContent sqlite 3.53.4, needs
   runner network; same SHA as `CMakeLists.txt`).
2. `schema.sql` in the daemon tree is still the symlink to
   `Persistence/TheCommons/schema.sql` (or byte-identical if git
   flattened it).
3. Exact `GET /presence/inventory` and `GET /presence/lots` are
   registered **before** the prefix `GET /presence/` in `src/main.cpp`
   (first-match dispatch).
4. `nginx-commons.conf` parses inside a wrap that defines
   `$connection_upgrade` (the live `server{}` already has that map;
   `nginx -t` on the fragment alone would fail on an unknown variable).
5. Same-commit touch of `liquidgov-platform/src/http_server.cpp` **and**
   `commons/index.html` requires the word `civic` or `deploy` in the
   commit subject. Paired civic+client deploys already use those words.

Fail the Action, not the droplet. A red X is cheaper than a 404 on
`/presence/lots` because it was registered after the prefix.

## What it does not

- `systemctl`, rsync, `nginx reload`, droplet curl
- Building `http_server.cpp`
- Running the daemon as a long test (WAL, presence, civic loopback all
  need a cookie and a box). Keep those in Claude's local script.
- Windows symlink semantics. Runner is `ubuntu-latest`.

## Land this file at `.github/workflows/commons.yml`

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
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: schema.sql is the spec copy
        run: |
          set -euo pipefail
          spec="Persistence/TheCommons/schema.sql"
          dmn="commons-daemon/schema.sql"
          test -f "$spec"
          test -e "$dmn"
          if [ -L "$dmn" ]; then
            target=$(readlink "$dmn")
            echo "symlink -> $target"
            # relative from commons-daemon/
            test -f "commons-daemon/$target" || test -f "$target"
          fi
          cmp -s "$spec" "$dmn"

      - name: exact GET /presence/* before the prefix route
        run: |
          set -euo pipefail
          f=commons-daemon/src/main.cpp
          inv=$(grep -n 'server.route("GET", "/presence/inventory"' "$f" | head -1 | cut -d: -f1)
          lots=$(grep -n 'server.route("GET", "/presence/lots"' "$f" | head -1 | cut -d: -f1)
          prefix=$(grep -n 'server.route("GET", "/presence/"' "$f" | grep -v inventory | grep -v lots | head -1 | cut -d: -f1)
          test -n "$inv" && test -n "$lots" && test -n "$prefix"
          echo "inventory@$inv lots@$lots prefix@$prefix"
          test "$inv" -lt "$prefix"
          test "$lots" -lt "$prefix"

      - name: civic+client same commit is named
        run: |
          set -euo pipefail
          # On a PR, compare against base; on push, against the parent.
          if [ "${{ github.event_name }}" = "pull_request" ]; then
            git fetch --depth=1 origin "${{ github.base_ref }}"
            base=$(git merge-base HEAD origin/${{ github.base_ref }})
          else
            base=${{ github.event.before }}
            if [ -z "$base" ] || [ "$base" = "0000000000000000000000000000000000000000" ]; then
              echo "no parent; skip mixed-commit gate"
              exit 0
            fi
          fi
          files=$(git diff --name-only "$base" HEAD || true)
          echo "$files"
          echo "$files" | grep -q '^liquidgov-platform/src/http_server.cpp$' && civic=1 || civic=0
          echo "$files" | grep -q '^commons/index.html$' && client=1 || client=0
          if [ "$civic" = 1 ] && [ "$client" = 1 ]; then
            subj=$(git log -1 --format=%s)
            echo "subject: $subj"
            echo "$subj" | grep -qiE 'civic|deploy' || {
              echo "http_server.cpp and commons/index.html in one commit need 'civic' or 'deploy' in the subject"
              exit 1
            }
          fi

      - name: build commons-daemon
        working-directory: commons-daemon
        run: |
          set -euo pipefail
          cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build -j2
          test -x build/commons_daemon

      - name: nginx -t on a wrapped fragment
        run: |
          set -euo pipefail
          sudo apt-get update -qq
          sudo apt-get install -y -qq nginx
          wrap=$(mktemp --suffix=.conf)
          cat > "$wrap" <<EOF
          pid /tmp/ci-nginx.pid;
          error_log /tmp/ci-nginx.log;
          events {}
          http {
            map \$http_upgrade \$connection_upgrade {
              default upgrade;
              ''      close;
            }
            server {
              listen 127.0.0.1:18080;
              include ${{ github.workspace }}/commons-daemon/deploy/nginx-commons.conf;
            }
          }
          EOF
          # The fragment uses alias /opt/commons/; -t does not need the dir.
          sudo nginx -t -c "$wrap"
```

## After it is green once

Leave it. A later round can add a loopback daemon smoke (`POST /presence/guest`
then `GET /presence/1,1`) if you want; that is not this file.
