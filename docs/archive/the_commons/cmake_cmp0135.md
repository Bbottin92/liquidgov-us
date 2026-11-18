# CMake CMP0135 — FetchContent archive timestamps

Not a BUILD. Complements `commons-daemon/CMakeLists.txt`. Do not bump
`cmake_minimum_required` just to silence a warning.

## What the policy is

CMake 3.24. Archive downloads (`FetchContent`, `ExternalProject`
`URL`) used to **preserve timestamps inside the zip**. A file dated 2015
inside sqlite-amalgamation then looks older than the last configure, so
`make` may skip rebuilds you expected — or the opposite, depending on
clock vs extract.

CMP0135 **NEW** (3.24+): extracted files get the **extraction time**, not
the zip's mtimes. Configure is deterministic; a re-extract is always
“newer.”

CMP0135 **OLD**: keep the zip's mtimes (pre-3.24 behavior).

`DOWNLOAD_EXTRACT_TIMESTAMP` on `FetchContent_Declare` is the per-call
knob (TRUE = preserve zip mtimes, OLD-like; FALSE = now, NEW-like).
CMake's warning on 3.24+ without either is “pick one.”

## What we set

```
FetchContent_Declare(sqlite3_amalgamation
    URL …
    URL_HASH SHA256=…
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
```

TRUE = sqlite.org's own dates on `sqlite3.c`. Fine: we compile that
file as our `sqlite3_vendored` and do not mix it with a previously
extracted tree that CMake thinks is “up to date” across URL changes —
**URL_HASH** already forces a re-download if the zip bytes change.

`cmake_minimum_required(VERSION 3.16)` stays. On 3.16 the keyword is
ignored; on 3.24+ it silences CMP0135 without raising the floor. Do not
`cmake_policy(SET CMP0135 NEW)` and then require 3.24 on the droplet.

## What not to do

- Treat the warning as a reason to vendor 9.5 MB again
- Flip TRUE/FALSE every turn to “see”
- Confuse this with `URL_HASH` (integrity) — timestamps are not a pin
- Confuse SHA256 here with sqlite.org's SHA3-256 (`grok_daemon_review.md`)

No deploy.
