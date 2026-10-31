# The Commons -- game daemon. Independent build from liquidgov-platform's
# CMake project (sibling process, sibling build) -- see sibling.md.
#
# SQLite is the official amalgamation, fetched at configure time and
# pinned by SHA256 rather than committed to git: this WSL environment has
# no sudo/apt and only ships the runtime .so, not headers (see notes.md,
# 2026-09-16), so a vendored source is still required -- FetchContent just
# keeps the ~9.5MB blob out of the repo (GROK_DAEMON_REVIEW.md should-fix)
# while the SHA256 still pins the exact bytes, same guarantee a committed
# blob gave. This does mean the build needs network access at configure
# time (a real tradeoff, not free); the droplet build will need the same.
cmake_minimum_required(VERSION 3.16)
project(commons_daemon CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(sqlite3_amalgamation
    URL https://sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA256=1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3_amalgamation)

# The amalgamation is C, not C++; compiled as its own translation unit,
# no special flags needed beyond what CMake already picks for C sources.
add_library(sqlite3_vendored STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_vendored PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
# Single-threaded daemon (http.hpp is explicitly not threaded); the
# default THREADSAFE=1 build is still correct and harmless, just not
# required. Left at SQLite's own default rather than tuned.

# The commit this binary was built from, for GET /health. A custom TARGET (no OUTPUT), so it runs on EVERY
# `cmake --build`, not just at configure: the droplet deploys with no reconfigure and is not a git clone, so the value
# comes from the .deployed-rev marker the deploy sync writes before the build (cmake/gen_build_info.cmake). The header
# is only rewritten when it changes; only src/buildinfo.cpp includes it.
set(COMMONS_BUILD_INFO_H ${CMAKE_CURRENT_BINARY_DIR}/build_info.h)
add_custom_target(commons_build_info
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${COMMONS_BUILD_INFO_H}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_build_info.cmake
    BYPRODUCTS ${COMMONS_BUILD_INFO_H}
    COMMENT "commons build info (the commit for /health)"
    VERBATIM)

add_executable(commons_daemon
    src/main.cpp
    src/buildinfo.cpp
    src/db.cpp
    src/presence.cpp
    src/guest.cpp
    src/placed.cpp
    src/http.cpp
    src/civic.cpp
    src/house.cpp
    src/friends.cpp
    src/studio.cpp
)
target_link_libraries(commons_daemon PRIVATE sqlite3_vendored)
add_dependencies(commons_daemon commons_build_info)
target_include_directories(commons_daemon PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

find_package(Threads REQUIRED)
target_link_libraries(commons_daemon PRIVATE Threads::Threads)
