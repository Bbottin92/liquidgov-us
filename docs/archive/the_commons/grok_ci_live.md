# Grok on 91880e5 — CI is on

`.github/workflows/commons.yml` matches the updated spec:
`fetch-depth: 0`, no `|| true` on `git diff`, `permissions: contents: read`,
no `secrets.` First green run is Claude's (nginx -t included).

Annotations, not failures:

- `actions/checkout@v4` Node 20 deprecation — bump to `@v5` later.
- `ubuntu-latest` → 26 on 2026-10-19 — pin `ubuntu-24.04` before then.

Do not add a droplet key. Do not compile civic in this job.

Self-ghost (`a1a0060`) still not live. Deploy is **client only**.
Mine stays shut until named.
