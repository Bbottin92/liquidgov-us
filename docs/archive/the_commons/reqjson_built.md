# reqjson — built (daemon only; not deployed)

Brandon: "BUILD reqjson: parse "key": not the first quoted token. Daemon only. Name-first body, guest
named x must join. test_reqjson.sh in CI. No nginx. No deploy." (`grok_reqjson.md`.)
**Deployed 2026-09-19 (~15:25Z), Brandon: `deploy` twice (plan, then `--go`)**: see "Deployed" at the bottom.

## The bug (latent, reproduced before fixing)

`reqjson::field_str/field_num` searched for the text `"key"` anywhere in the body. A quoted *value*
that spells a field name matched first. With `display_name` before the fields, a guest named `x`/`y`
got 400 `missing_field`, `secret` got 403, `id`/`facing` joined under a wrong "(2)" name. The shipped
client sends `display_name` LAST (`region,x,y,facing,display_name`), so it never bit -- luck of JS
object-key order, one reordered or appended field away from breaking real players.

## The fix (`commons-daemon/src/reqjson.hpp`, same API, same 10 call sites)

`detail::find_value` walks the flat object the way JSON reads it: `{`, then key, `:`, value, `,`, ...
A field is found only in KEY position, so no value can ever be mistaken for a key. Values are read as
they are: strings (same `\"` `\\` handling as before), nested `{}`/`[]` (skipped whole, brackets inside
strings ignored; their inner keys are not fields of this body), or a raw number/literal token. First
top-level occurrence of a key wins. `field_num` is stricter than the old digit-scan: the token must be
a JSON-number (`-` or digit start, only `0-9 . e E + -`, fully consumed), so `0x10`, `5abc`, `+5`,
`.5`, `inf`, `nan`, `1e999`, `true`, `null`, `"5"` are all "missing", where the old code read a numeric
prefix. A body that is not an object, or is malformed before the key, is "missing" (400 from the
handlers, as before) -- never a crash.

Behaviour change to know: an *unterminated* string used to return its text to the end of the body; it is
now "missing". Nothing legitimate sends one (the daemon reads the full Content-Length).

## Tests (in CI: the workflow already runs `run_all.sh`, so no workflow change)

- `tests/reqjson_unit.cpp` (compiled by `test_reqjson.sh` with `g++ -std=c++17 -Wall -Wextra`): **201
  checks**. Name-first bodies for 12 field names, a chain of values that are field names, escaped
  quotes carrying forged `"x":999`, whitespace, nesting, brackets in strings, absent/empty/non-object,
  type mismatches, number edge cases, duplicates, malformed-before-key.
- `tests/test_reqjson.sh`: 102 assertions in all. The same bodies through the real daemon: each of the
  12 names joins / moves / leaves with `display_name` FIRST, under its own name at the coordinates sent
  (not the id, not "(2)"); rekey; `id:"secret"` is a proper 403; `region:"x"` reads x correctly; a name
  carrying forged coordinates stays a name; garbage/empty/array bodies are 400 and the daemon lives.
- **Mutation-checked**: with the old header restored the test fails (see the numbers in the commit / the
  run); with the new one, all 12 files pass in the full suite.

## Left alone, on purpose

`civic.cpp` `resolve_friends` has its own quote-to-quote scan of civic's reply
(`{"username":..,"friends":[..]}`). It does not share this bug: `"friends"` is the first KEY in civic's
reply, and a user named "friends" still lands on the real list (checked by reading the code, not
changed). `field_str(.., "username")` on civic's `session-user` reply now uses the new walker; the suite
passes against both the stub and the real civic, which is what pins that.

## Not here

Deploy scripts, nginx `/guest`, backup retention, `index.html` split, `/health` hash: still unbuilt names.

## Deployed (2026-09-19, ~15:25Z; first real use of the deploy scripts)

`deploy-daemon.sh --baseline 60f6745` (plan only) first: no drift, and exactly 10 files to change
(`src/reqjson.hpp` and nine under `deploy/`; nothing under `tests/`; civic diff since `60f6745` empty). Then
`deploy-daemon.sh --go --baseline 60f6745 --guest-name x`, daemon only (no client, no civic, no nginx), deploying
`a56eba3`. Exit 0:

1. Drift check clean. 2. Online backup
`/var/lib/commons/backups/commons.sqlite.pre-a56eba3-20260919T152437Z`, `integrity_check` ok (counts: inventory 3,
items 3, map_instances 2, maps 14, placed 0, players 1, recipe_inputs 0, recipe_outputs 3, recipes 3).
3. Synced; `.deployed-rev` written (so the NEXT deploy needs no `--baseline`). 4. `-j1` build, no reconfigure
(five objects rebuilt, linked). 5. Restarted (drops RAM presence and guest credentials; houses are durable).
6. `verify.sh --compare-backup --guest-name x`: **32 passed, 0 failed**: both services active, `NRestarts=0`,
journal shows listening and no error lines, civic `/` 200, `/commons/` 200, public `/internal/` 404, lots and
region 200, **a real guest named `x` minted, joined (under the name `x`, at the coordinates sent), moved, a bad
secret 403, left and gone**, `integrity_check` ok, every table's row count identical to the backup.

That is the real-Square proof of the reqjson fix: a guest named `x` joins through nginx against the live daemon.
It is also the first run of the deploy scripts over real ssh/rsync/systemd, which the fake droplet could not prove.
