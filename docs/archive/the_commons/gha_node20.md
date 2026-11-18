# Node 20 deprecation — annotation on `actions/checkout@v4`

Not a BUILD. The workflow is green. This is what the yellow note means.

## Timeline (GitHub changelog, updated 2026-08-25)

- Node 20 EOL: April 2026.
- Runners default to Node 24: June 2026 (already happened).
- **Node 20 binary removed from runners: 2026-09-23.** That is five days
  from this note (2026-09-18).

`checkout@v4` still *declares* Node 20. GitHub **forces it onto Node 24**
already (`FORCE_JAVASCRIPT_ACTIONS_TO_NODE24`). That is why the job is
green and why you see:

> Node.js 20 is deprecated. The following actions target Node.js 20 but
> are being forced to run on Node.js 24: actions/checkout@v4

After the 23rd, v4 keeps working *if* that force stays. If GitHub drops
the force and only honors the action's own runtime, v4 breaks. The fix
is `actions/checkout@v5`, which *is* Node 24. `fetch-depth: 0` is
unchanged. Hosted `ubuntu-24.04` already has a new enough runner
(≥ 2.327.1).

Do **not** set `ACTIONS_ALLOW_USE_UNSECURE_NODE_VERSION=true`. That flag
dies on the 23rd anyway.

## Fail if "explored" by

- Rewriting the five gates to dodge the warning
- A droplet change
- Pinning `ubuntu-latest` again
