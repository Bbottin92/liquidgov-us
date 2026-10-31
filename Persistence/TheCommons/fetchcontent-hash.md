# FetchContent hash verification

Not a BUILD. Complements `fetchcontent-timestamps.md` and
`GROK_DAEMON_REVIEW.md`. The pin in `commons-daemon/CMakeLists.txt` is
`URL_HASH SHA256=1e71ddf9…` of the **zip**, not sqlite.org's SHA3-256 of
the same file.

## When CMake checks

1. `FetchContent_MakeAvailable` sees no stamp for this URL+hash in
   `build/_deps/` → download the zip to a temp name.
2. Hash the **whole archive** with the algorithm in `URL_HASH`.
3. Mismatch → configure **fails**, zip is not extracted, `sqlite3.c`
   never compiles. That is the whole guarantee.
4. Match → extract, write stamps. Later configures skip to the stamp
   (timestamps.md). They do **not** re-hash the zip unless you wipe
   `_deps` or change `URL_HASH`.

No hash → CMake 3.24+ warns; older versions will happily extract a
MITM zip. We do not ship an unhashed URL.

## Algorithms CMake 3.16 actually has

`URL_HASH <ALGO>=<hex>`: MD5, SHA1, SHA224, **SHA256**, SHA384, SHA512.
Not SHA3-256. sqlite.org's download page is SHA3-256 (`628a44cf…`).
Pasting that into `URL_HASH SHA256=` fails every configure. Claude
hashed the zip with SHA256 after download; that pin is the correct one
for this CMake floor.

`file(SHA3_256)` exists on newer CMake. Do not raise
`cmake_minimum_required` to use it.

## What is hashed

The **zip bytes**, not `sqlite3.c` inside. A zip that contains the
right `.c` with extra junk still changes SHA256. A recompressed zip of
identical sources fails the pin. That is intended.

`EXPECTED_HASH` on `file(DOWNLOAD)` is the same idea; FetchContent is
that plus extract + stamps.

## Failures that are not a “wrong pin”

- sqlite.org 404 / year-folder URL (`/2026/sqlite-amalgamation-3530400.zip`)
  — network, not hash
- `FETCHCONTENT_FULLY_DISCONNECTED` without `SOURCE_DIR` — no file to hash
- Changing only `DOWNLOAD_EXTRACT_TIMESTAMP` — does not re-verify

## Reviewer fail

- Replacing SHA256 with the page's SHA3
- Dropping `URL_HASH` because “stamps exist”
- Hashing `sqlite3.c` and putting that in `URL_HASH` (wrong object)

No deploy.
