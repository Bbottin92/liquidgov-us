# FetchContent timestamp behavior

Not a BUILD. Complements `cmake-cmp0135.md`. The pin is still
`URL_HASH SHA256=`. Timestamps never substitute for it.

## Two clocks

| Clock | Where | What it decides |
|---|---|---|
| **Stamp files** | `build/_deps/<name>-stamp/` (`download`, `extract`, `patch`, …) | Whether CMake **skips** the download/extract on the next configure |
| **Extracted mtimes** | `build/_deps/<name>-src/sqlite3.c` | Whether **Make** thinks that `.c` is newer than `sqlite3.c.o` |

Ninja often hashes content and ignores mtime. Unix `make` does not.
CMP0135 / `DOWNLOAD_EXTRACT_TIMESTAMP` only changes the **second** clock.

## Populate path (ours)

1. Configure sees `FetchContent_Declare` + `MakeAvailable`.
2. If the stamp says this **URL_HASH** was already extracted into that
   build dir → skip network. Offline reconfigure of an existing `build/`
   works even with FetchContent.
3. Else download the zip, verify SHA256, extract, write stamps.
4. Compile `${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c`.

`rm -rf build` throws away stamps **and** the extracted tree → network
again. That is the droplet-offline problem (`GROK_DAEMON_REVIEW.md`),
not CMP0135.

`FETCHCONTENT_FULLY_DISCONNECTED=ON` plus
`FETCHCONTENT_SOURCE_DIR_SQLITE3_AMALGAMATION=/path` is how a box with
no sqlite.org still configures. Do not invent apt.

## `DOWNLOAD_EXTRACT_TIMESTAMP TRUE` (what we ship)

Extracted `sqlite3.c` keeps sqlite.org's date (often older than the
machine). After a clean extract, **make** may still rebuild because the
`.o` does not exist yet. After a no-op reconfigure, make sees an old
`.c` and a newer `.o` → skip — correct, bytes did not change.

If we flipped to FALSE: every re-extract stamps “now”, make rebuilds
sqlite3.c even when the zip hash is unchanged. Harmless, slower, noisy
on a 512 MB `-j1` box. Leave TRUE.

## What does not happen

- A zip mtime does not prove integrity (hash does).
- TRUE does not freeze the amalgamation version.
- Stamps are **per build directory**. A second `build/` is a second
  download unless you share `_deps` or set `SOURCE_DIR`.

## Reviewer fail

- Committing `build/_deps/` to git “so timestamps stick”
- Deleting stamps to “force a hash check” on every save
- Raising `cmake_minimum_required` to 3.24 for this warning

No deploy.
