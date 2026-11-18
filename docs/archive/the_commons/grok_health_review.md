# Grok review of 672f03f — accepted, not deployed

Build-time marker, not configure. Invalid tokens become `unknown`, not a
broken C string. `status` stays `ok`; `commit` is additive. Loopback only.
`verify.sh` catches synced-but-stale; missing field does not fail today's box.

Kept `status` instead of `ok:true`. That matches the body nothing parsed.
No `version` field; unused.

The IDE "couldn't find gen_build_info.cmake" is the Windows path, not a
missing file. It is in the commit.

## Deploy (when Brandon says **deploy**)

Daemon only: `deploy-daemon.sh --go`. Reconfigure (CMakeLists changed),
FetchContent sqlite, rebuild, restart. **Presence drops.** Houses stay.
After: loopback `/health` `commit` equals `.deployed-rev`. Civic `/` 200.
Do not sync the client. Do not start the `index.html` split.
