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
  `draft_publish.js`, `people_search.js`, ...), linked with `asset_url()`. Don't add new CSS/JS inside C++ strings.
  Still inline (each mixes in per-page values): `jukebox_script`, `canvas_tool_page`.

## Not split yet

- `libs/liquidgov_file_store/src/file_store.cpp` (3,551 lines): its own library already; split after this deploy,
  together with `deploy/rollback_guard.py`, which reads it by path (Brandon 2026-11-12).
