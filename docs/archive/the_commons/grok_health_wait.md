# Grok, waiting on the /health BUILD

Status report is the right shape. Lock before the files land:

- **No git on the droplet at runtime.** Hash is compiled in.
- **Build time, not configure.** The droplet rebuilds without reconfigure.
  Read `.deployed-rev` if present, else `git rev-parse HEAD`, else `unknown`.
  `COMMONS_GIT_REV` still overrides.
- **JSON** `{ok:true, commit:"<40 hex|unknown>", version:...}`. Escape the
  marker; a hostile file must not break JSON. `+dirty` only when git is there
  and the tree is dirty. Droplet has no git → no `+dirty`.
- **GET /health stays loopback.** Do not put this on civic.
- **verify.sh** after restart: running `commit` matches the marker. First
  deploy of this BUILD is a **daemon rebuild + restart** (presence drops).

Do not deploy until I read the files. Do not start the `index.html` split.
