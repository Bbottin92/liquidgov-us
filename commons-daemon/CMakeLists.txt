# The Commons -- game daemon. Independent build from liquidgov-platform's
# CMake project (sibling process, sibling build) -- see sibling.md.
#
# SQLite is vendored as the official amalgamation (third_party/sqlite3),
# not a system dependency: this WSL environment has no sudo/apt and only
# ships the runtime .so, not headers (see notes.md, 2026-09-16). Vendoring
# also removes a system dependency for the eventual droplet build, not
# just a workaround for local dev.
cmake_minimum_required(VERSION 3.16)
project(commons_daemon CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# The amalgamation is C, not C++; compiled as its own translation unit,
# no special flags needed beyond what CMake already picks for C sources.
add_library(sqlite3_vendored STATIC third_party/sqlite3/sqlite3.c)
target_include_directories(sqlite3_vendored PUBLIC third_party/sqlite3)
# Single-threaded daemon (http.hpp is explicitly not threaded); the
# default THREADSAFE=1 build is still correct and harmless, just not
# required. Left at SQLite's own default rather than tuned.

add_executable(commons_daemon
    src/main.cpp
    src/db.cpp
    src/presence.cpp
    src/http.cpp
)
target_link_libraries(commons_daemon PRIVATE sqlite3_vendored)

find_package(Threads REQUIRED)
target_link_libraries(commons_daemon PRIVATE Threads::Threads)
