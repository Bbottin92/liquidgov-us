# FetchContent stamp mechanism

Not a BUILD. Complements `fetchcontent_timestamps.md` and
`fetchcontent_hash.md`. Do not commit `build/_deps/`.

## What a stamp is

A **zero-or-tiny file whose existence means “this step already
succeeded for this declared URL+hash.”** CMake's FetchContent is a thin
wrapper around ExternalProject. The sub-build records each step as a
stamp so the next configure can skip it.

For `sqlite3_amalgamation` after a successful configure you get
something like:

```
build/_deps/sqlite3_amalgamation-src/     # extracted zip (sqlite3.c)
build/_deps/sqlite3_amalgamation-subbuild/
build/_deps/sqlite3_amalgamation-stamp/
    sqlite3_amalgamation-mkdir
    sqlite3_amalgamation-download
    sqlite3_amalgamation-update
    sqlite3_amalgamation-patch
    sqlite3_amalgamation-populate
```

Names vary slightly by CMake version. The idea does not: **no stamp →
run the step; stamp present and inputs unchanged → skip.**

## What counts as “inputs unchanged”

The sub-build's `CMakeLists.txt` is generated from `FetchContent_Declare`.
It embeds `URL` and `URL_HASH`. Change either → generated file
changes → download stamp is stale → re-download → hash check
(`fetchcontent_hash.md`) → new extract → new stamps.

`DOWNLOAD_EXTRACT_TIMESTAMP` is **not** an input to the download stamp.
Flipping TRUE/FALSE does not re-fetch (`cmake_cmp0135.md`).

`rm` of only `*-src/` without `*-stamp/` can leave CMake thinking
populate succeeded with an empty tree. Wipe **both**, or `rm -rf build`.

## FetchContent vs a full ExternalProject

| Step | FetchContent (ours) | ExternalProject |
|---|---|---|
| mkdir / download / extract | yes | yes |
| update (git pull) | n/a (URL zip) | yes |
| configure / build / install of the **dep as its own project** | only if the zip has a CMakeLists we `MakeAvailable` | yes |

The amalgamation zip has **no** CMakeLists. We `add_library(sqlite3_vendored … sqlite3.c)` ourselves. Stamps stop at **populate**. Compiling `sqlite3.c` is *our* target graph, not a stamp.

## Offline

Stamps + src already in this `build/` → no network (`timestamps.md`).
A **new** build dir has no stamps → needs sqlite.org, or
`FETCHCONTENT_SOURCE_DIR_SQLITE3_AMALGAMATION` / a tarball (deploy gate).

## Reviewer fail

- Committing stamp files so CI “skips the hash”
- Touching stamps to force a rebuild of `commons_daemon` (rebuild the
  `.o`; stamps are populate, not compile)
- Deleting `*-src` and keeping stamps

No deploy.
