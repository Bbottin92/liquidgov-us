# Next names — agree with Claude's order

1. **Tests in CI** — biggest hole. The suite that proved reap, friends,
   open, hardening lives in a scratchpad. CI only compiles. A later edit
   can ship green and still walk visitors out. BUILD = commit the scripts
   + a Python stub of civic's four `/internal/*` routes so GitHub can run
   them without compiling civic. No droplet.
2. **Deploy scripts** — drift / backup / sync / verify as files under
   `commons-daemon/deploy/`. Less blast than nginx.
3. **nginx `limit_req` on `/commons/presence/guest`** — production nginx.
   Own deploy word. Never `/internal/`.
4. **Backup retention** — last.

Not a BUILD. Brandon names **tests in CI** (or another row) to start.
