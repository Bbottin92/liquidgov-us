# FetchContent alternatives

Not a BUILD. Complements `fetchcontent-hash.md` / `stamps.md`.
`commons-daemon` already FetchContents sqlite amalgamation 3.53.4 with
`URL_HASH SHA256=`. Do not swap the mechanism this week.

## What we are solving

Need `sqlite3.c` + `sqlite3.h` **without** apt/`libsqlite3-dev`, without
9.5 MB in git, with a **byte pin**, on a 512 MB box that may be offline
at configure.

## Menu

| Mechanism | Pin | Network at cmake | Repo weight | Fit |
|---|---|---|---|---|
| **FetchContent URL + URL_HASH** (shipped) | SHA256 of zip | first populate only | tiny | default; droplet needs cache or SOURCE_DIR |
| **In-tree amalgamation** (ba296e8) | git itself | never | 9.5 MB blob | we left this; legal, painful |
| **`FETCHCONTENT_SOURCE_DIR_SQLITE3_AMALGAMATION`** | same CMake | never if path exists | 0 | **offline deploy** without changing CMakeLists |
| **Tarball beside the tree** + `URL=file://` | SHA256 | never | gitignored zip | same pin, scp once |
| **ExternalProject_Add** | URL_HASH | at **build** of the dep | tiny | extra sub-project; amalgamation has no CMakeLists |
| **find_package(SQLite3)** / pkg-config | distro | never | 0 | needs headers we do not have; version not pinned to 3.53.4 |
| **git submodule** of sqlite | git SHA | clone | history | sqlite is Fossil; unofficial GitHub mirrors |
| **CPM.cmake / Hunter / vcpkg / Conan** | their lockfiles | yes | extra tooling | overkill for two files |

## What to use when

- **WSL with network:** FetchContent as shipped. Stamps skip the next configure.
- **Droplet cmake, no sqlite.org:** do **not** revert the blob and do **not**
  apt. Either:
  1. `cmake -DFETCHCONTENT_SOURCE_DIR_SQLITE3_AMALGAMATION=/opt/src/sqlite-amalgamation-3530400 …` after unpacking the same zip once, or
  2. `FETCHCONTENT_FULLY_DISCONNECTED=ON` plus that SOURCE_DIR, or
  3. copy the zip next to the tree and point `URL` at `file://` (still hashed).
- **Air-gap forever, hate cmake flags:** vendor `sqlite3.c` again. Last resort. The 9.5 MB was honest, not a bug.

ExternalProject does not buy a pin we lack; it only moves download from
configure-time to build-time. `find_package` silently takes Debian's
SQLite and drops the amalgamation flags. Submodules pull a mirror, not
sqlite.org.

## Reviewer fail

- Adding CPM to fetch nlohmann_json “while we are here”
- `apt-get install libsqlite3-dev` on the droplet as a game task
- Switching to system `.so` and keeping amalgamation compile flags
- A second SQLite in `liquidgov-platform/`

No deploy.
