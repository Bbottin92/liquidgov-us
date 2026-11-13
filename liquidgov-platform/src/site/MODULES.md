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
| `core/` | Shared building blocks: `state` (the shared in-memory state and its mutexes), `adapters_store` (the store, blob store, scanners, social graph globals), `http` (request parsing, responses, cookies), `text`, `html`, `net`, `multipart`, `names_categories`, `route` (what `handle()` hands a feature's routes), `ws_limits` (caps on open WebSockets: every live socket takes a slot with `ws_take_slot` before its 101 and moves it into its session thread; `LIQUIDGOV_WS_PER_IP` / `LIQUIDGOV_WS_MAX`, defaults 20 / 200). |
| `ui/` | The page shell every page uses: `shell` (`page_wrap`, `nav`), `wrap`, `assets`, `favicon_cookies`. The site stylesheet is a real file, `assets/site.css` (linked by `page_wrap` through `asset_url`, so its URL changes whenever it does). |
| `features/<name>/` | One folder per feature: `admin`, `assets`, `auth`, `canvas`, `communities`, `feed`, `here`, `jukebox`, `live`, `mail`, `map`, `notices`, `pages`, `pains`, `people`, `profile`, `proposals`, `push`, `settings`, `styles`, `system`. Each has `<name>_routes.{hpp,cpp}` for its HTTP routes, plus modules for its pages and logic. (`here`, dots on profile pages, also runs a WebSocket: `here_ws_upgrade`, called from `main()`'s accept loop like the other sockets.) |

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
  before it loads (`jukebox.js` reads `window.LG_JUKEBOX`, `canvas.js` reads `window.LG_CANVAS`, `here.js` reads
  `window.LG_HERE`). Nothing is inline any more (canvas_tool_page's engine moved to `assets/canvas.js` 2026-11-13).
- The walker's controls (the Drop button, the touchpad with A/B, WASD/arrows/Enter/Esc) are one module, `assets/walker.js`
  (`window.LG_WALKER`, with the one key/button map `LG_WALKER.CONTROLS`). Pages you walk on load it first and
  mount their own walker: `here.js` (dots on profile pages), `lgmap_walk.js` (the map) and `home_drop.js` (the logged-out home page's hook). The shared walking pieces (what is near, using it, speed, keeping you in view, the teardrop `shape` and the A-with-nothing `hop`) live in walker.js too. Tests read the map from
  the page, never a copy (`tests/browser/walker_controls_check.py`).
- The landing intro (`#lgintro`, `shell.cpp` `intro_overlay_html`) runs `assets/intro.js`; it plays on every visit unless
  turned off in Settings (`intro_setting_card`, /settings and /settings/intro). The pref's one name is `kIntroPref`
  (`shell.hpp`): the account's field, the form field and a logged-out browser's localStorage key. While it plays,
  `tip.js` holds any first-time tip until its `lgintro:done` (`tests/browser/intro_check.py`).

## The store (`libs/liquidgov_file_store`)

Split the same way on 2026-11-12 (was one 3,809-line `file_store.cpp`; code moved, not rewritten; the store's bytes
on disk are unchanged, proven by `tests/store_roundtrip_test.cpp`; redone on 2026-11-13 on top of the styles and here
records, proven again for those by `tests/store_roundtrip_styles_here_test.cpp`). Its file list is `libs/liquidgov_file_store/sources.cmake`
(CMake, `deploy/rollback_guard.py`, `lgp1_consistency.py`, `program_messaging_test.cpp` and `store_split_check.py`
all read it).

| File | What lives there |
|---|---|
| `src/file_store.cpp` | The frame: `load()` (offers each line to every module's `read_*`, then runs the `finish_*` in order) and `flush()` (calls the `write_*` sections in the order the store has always been written). Both orders live here only. |
| `src/core/codec` | The field encoding every module uses (percent-encoding, tab split, number parsing). |
| `src/core/load_pending.hpp` | Rows `load()` parks until every line is read. |
| `src/records/<module>.cpp` | One family of record types: its store methods, `read_<module>`, `finish_*`, `write_*`. Modules: `accounts`, `admin`, `avatars`, `city`, `communities`, `conversations`, `counters`, `jukebox`, `mail`, `notices`, `pains`, `posts`, `prefs`, `profile`, `proposal_comments`, `proposals`, `social`, `styles`, `here`. |
| `include/liquidgov/platform/records/<family>.hpp` | One family's types and helpers (structs, enums, constants, free functions): `accounts`, `admin`, `city`, `communities`, `jukebox`, `mail`, `notices`, `pains`, `posts`, `prefs`, `profile`, `proposals`, `social`, `styles`. `file_store.hpp` includes them all and declares `FileStore`; its header comment still holds every record type's LGP1 format (the checks read it there). |

A new LGP1 record type: document it in `file_store.hpp`'s header comment; put its types in its family's
`records/<family>.hpp`; read it in its module's `read_*` (the
field-count check first, as every type does: the rollback guard reads the first one) and write it in its `write_*`;
a new module goes in `sources.cmake` and its calls in `file_store.cpp`; add rows for it to a
NEW round-trip fixture and test (the existing ones are frozen; store_split_check.py requires a row of every type across
`tests/fixtures/lgp1_roundtrip*/`), with expected answers written by the code before the change; give it a
`deploy/<feature>-undo.py` with a `<NAME>_TAGS` tuple and a `tests/rollback/<feature>_undo.sh` (undo_lists_check.py).
`FileStore` itself is still one class: a new record type changes it, so every file that includes `file_store.hpp`
recompiles. Only the record types' own declarations are split out. `lgp1_consistency.py` checks all of that.

## Build options (liquidgov-platform/CMakeLists.txt)

- `LG_PCH` (default ON): precompiled standard-library headers for `liquidgov_http` and the store. Only standard
  headers, so editing a record or store header never invalidates them. OFF builds exactly as before (CI builds both).
- ccache: used automatically when installed (`CMAKE_CXX_COMPILER_LAUNCHER`); nothing changes without it.
- `LG_STATIC_RUNTIME` (default OFF), `LG_BUILD_COMMIT`, `LG_BUILD_TREE`: for prebuilt deploys
  (`deploy/PLAN-prebuilt.md`, `scripts/build_release.sh`); `liquidgov_http --version` prints the commit and tree.
- Includes: `site/common.hpp` no longer pulls in the store; include `site/core/adapters_store.hpp` (or
  `liquidgov/platform/file_store.hpp`) where `g_store` is used, and only the `records/<family>.hpp` a header names.
  `scripts/build_timing.sh` (run by lgcheck.sh) prints what a one-file touch recompiles.
