# reqjson — built (daemon only; not deployed)

Brandon: "BUILD reqjson: parse "key": not the first quoted token. Daemon only. Name-first body, guest
named x must join. test_reqjson.sh in CI. No nginx. No deploy." (`GROK_REQJSON.md`.)
**Not deployed**: the daemon on the droplet is still the old parser until Brandon says `deploy`.

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
