# Site server modules

The site server (`liquidgov_http`) was one 13,000-line file, `src/http_server.cpp`. Since 2026-11-12 (Brandon: "make it
structured, standardized, systematic and most importantly modular") it is split into modules here. The list of
module `.cpp` files, in build order, lives in one place: `sources.cmake` (CMakeLists.txt builds from it, and
`tests/browser/server_source.py` reads it for the checks that look at the server's text).

## Layout

| Folder | What lives there |
|---|---|
| `../http_server.cpp` | `main()` only: sockets, threads, startup. |
| `common.hpp` | The library and standard headers every module uses, and `using namespace liquidgov::platform`. |
| `router.cpp` | `handle()`: parses the request once, then asks each feature's routes in a fixed order; first answer wins; else 404. |
| `core/` | Shared building blocks: `state` (the shared in-memory state and its mutexes), `adapters_store` (the store, blob store, scanners, social graph globals), `http` (request parsing, responses, cookies), `text`, `html`, `net`, `multipart`, `names_categories`, `route` (what `handle()` hands a feature's routes). |
| `ui/` | The page shell every page uses: `shell` (`page_wrap`, `nav`), `wrap`, `assets`, `favicon_cookies`. The site stylesheet is a real file, `assets/site.css` (linked by `page_wrap` through `asset_url`, so its URL changes whenever it does). |
| `features/<name>/` | One folder per feature: `admin`, `assets`, `auth`, `canvas`, `communities`, `feed`, `jukebox`, `live`, `mail`, `map`, `notices`, `pages`, `pains`, `people`, `profile`, `proposals`, `push`, `settings`, `system`. Each has `<name>_routes.{hpp,cpp}` for its HTTP routes, plus modules for its pages and logic. |

## Rules (standard for every module)

- One `.hpp` + `.cpp` per module. The header declares only what other modules use; everything else stays in the `.cpp`.
- All site code is in `namespace liquidgov::site`.
- A module includes the headers it uses, nothing more.
- Default arguments live in the header declaration only.
- Shared globals are defined once (in `core/state.cpp`, `core/adapters_store.cpp`, `ui/favicon_cookies.cpp`,
  `features/jukebox/jukebox_personal.cpp`) and declared `extern` in their header; never define a global with a
  dynamic initializer in a second module (start-up order between files is not defined).
- A new route goes in its feature's `<name>_routes.cpp`. Route order matters only where two routes can match the same
  request; keep such routes in the same feature file, in the order they must be tried.
- A new module: add its `.cpp` to `sources.cmake` (nothing else lists files).
- Page CSS and JavaScript live in `liquidgov-platform/assets/` as real files (`site.css`, `mail_widget.js`,
  `draft_publish.js`, `people_search.js`, `jukebox.js`, ...), linked with `asset_url()`. Don't add new CSS/JS
  inside C++ strings. A script that needs per-page values gets them from a one-line inline settings object set just
  before it loads (`jukebox.js` reads `window.LG_JUKEBOX`). Still inline: `canvas_tool_page`.

## The store (`libs/liquidgov_file_store`)

Split the same way on 2026-11-12 (was one 3,809-line `file_store.cpp`; code moved, not rewritten; the store's bytes
on disk are unchanged, proven by `tests/store_roundtrip_test.cpp`). Its file list is `libs/liquidgov_file_store/sources.cmake`
(CMake, `deploy/rollback_guard.py`, `lgp1_consistency.py`, `program_messaging_test.cpp` and `store_split_check.py`
all read it).

| File | What lives there |
|---|---|
| `src/file_store.cpp` | The frame: `load()` (offers each line to every module's `read_*`, then runs the `finish_*` in order) and `flush()` (calls the `write_*` sections in the order the store has always been written). Both orders live here only. |
| `src/core/codec` | The field encoding every module uses (percent-encoding, tab split, number parsing). |
| `src/core/load_pending.hpp` | Rows `load()` parks until every line is read. |
| `src/records/<module>.cpp` | One family of record types: its store methods, `read_<module>`, `finish_*`, `write_*`. Modules: `accounts`, `admin`, `avatars`, `city`, `communities`, `conversations`, `counters`, `jukebox`, `mail`, `notices`, `pains`, `posts`, `prefs`, `profile`, `proposal_comments`, `proposals`, `social`. |

A new LGP1 record type: document it in `file_store.hpp`'s header comment; read it in its module's `read_*` (the
field-count check first, as every type does: the rollback guard reads the first one) and write it in its `write_*`;
a new module goes in `sources.cmake` and its calls in `file_store.cpp`; add a row to
`tests/fixtures/lgp1_roundtrip/store_all_tags.lgp1` (store_split_check.py requires one) and re-write the expected
answers; if it is new since 6ab8234, add it to `deploy/city-undo.py` (CITY_TAGS and its docstring) and
`tests/e2e/city_undo.sh`. `lgp1_consistency.py` checks all of that.
