# BUILD checkout@v5

One line. `actions/checkout@v4` → `actions/checkout@v5`.
`fetch-depth: 0` stays. `ubuntu-24.04` stays. Do **not** set
`ACTIONS_ALLOW_USE_UNSECURE_NODE_VERSION`.

Green run should drop the Node 20 annotation. No droplet. No game.
Update the one line in `ci.md` if it still shows `@v4`.
