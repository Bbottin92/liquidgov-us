cmake_minimum_required(VERSION 3.16)
project(liquidgov_platform CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Faster builds (Brandon 2026-11-13, "MODULARIZE EVERYTHING": a deploy should recompile only what it touches).
# LG_PCH: precompile the standard library headers for liquidgov_http and the store (only standard headers, so editing a
# record or store header never invalidates the precompiled header). OFF builds exactly as before; lgcheck.sh builds an
# OFF tree too on CI so both keep working.
# LG_STATIC_RUNTIME: link libstdc++ and libgcc statically (and libsodium, when its .a exists) so a binary built on
# GitHub needs only the server's glibc. OFF by default: local and CI builds are unchanged.
option(LG_STATIC_RUNTIME "Link libstdc++/libgcc (and libsodium if possible) statically into liquidgov_http" OFF)
option(LG_PCH "Precompile standard headers for liquidgov_http and liquidgov_file_store" ON)
set(LG_PCH_HEADERS <algorithm> <atomic> <cctype> <chrono> <cmath> <cstdint> <cstdio> <cstdlib> <cstring> <ctime> <deque> <fstream> <functional> <iostream> <map> <mutex> <optional> <set> <sstream> <stdexcept> <string> <string_view> <thread> <unordered_map> <utility> <vector>)
# ccache, when it is installed: repeated builds of unchanged files come from its cache. Nothing changes without it.
find_program(LG_CCACHE ccache)
if(LG_CCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER "${LG_CCACHE}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${LG_CCACHE}")
endif()

add_subdirectory(libs/third_party_miniz)
add_subdirectory(libs/third_party_stb)

add_subdirectory(libs/liquidgov_clock)
add_subdirectory(libs/liquidgov_account)
add_subdirectory(libs/liquidgov_scanner)
add_subdirectory(libs/liquidgov_password)
add_subdirectory(libs/liquidgov_session)
add_subdirectory(libs/liquidgov_blob)
add_subdirectory(libs/liquidgov_image)
add_subdirectory(libs/liquidgov_file_type)
add_subdirectory(libs/liquidgov_auth)
add_subdirectory(libs/liquidgov_avatar)
add_subdirectory(libs/liquidgov_messenger)
add_subdirectory(libs/liquidgov_file_transfer)
add_subdirectory(libs/liquidgov_file_store)
add_subdirectory(libs/liquidgov_websocket)

add_executable(liquidgov_platform_auth_test tests/auth_test.cpp)
target_link_libraries(liquidgov_platform_auth_test PRIVATE liquidgov_auth)
# The "no password" placeholder never signs anyone in (2026-11-11).
add_executable(liquidgov_platform_placeholder_password_test tests/placeholder_password_test.cpp)
target_link_libraries(liquidgov_platform_placeholder_password_test PRIVATE liquidgov_auth)

add_executable(liquidgov_platform_file_store_test tests/file_store_test.cpp)
target_link_libraries(liquidgov_platform_file_store_test PRIVATE liquidgov_auth liquidgov_file_store)

# City features, map privacy, problems and solutions, forward-compatible records (2026-11-06/07): their own file,
# since committed tests stay frozen and a new check goes in a new test file (hard rules #6847b, #1bcfd).
add_executable(liquidgov_platform_city_store_test tests/city_store_test.cpp)
target_link_libraries(liquidgov_platform_city_store_test PRIVATE liquidgov_auth liquidgov_file_store)

# Dots on profile pages: the stored choice (here, 2026-11-12): a new file, committed tests stay frozen.
add_executable(liquidgov_platform_here_store_test tests/here_store_test.cpp)
target_link_libraries(liquidgov_platform_here_store_test PRIVATE liquidgov_auth liquidgov_file_store)
# The profile style library's store records (sty/styn/stu/str, 2026-11-12): a new file, committed tests stay frozen.
add_executable(liquidgov_platform_style_store_test tests/style_store_test.cpp)
target_link_libraries(liquidgov_platform_style_store_test PRIVATE liquidgov_auth liquidgov_file_store)

add_executable(liquidgov_platform_blob_test tests/blob_test.cpp)
target_link_libraries(liquidgov_platform_blob_test PRIVATE liquidgov_blob liquidgov_image)

add_executable(liquidgov_platform_avatar_test tests/avatar_test.cpp)
target_link_libraries(liquidgov_platform_avatar_test PRIVATE liquidgov_avatar liquidgov_image)

add_executable(liquidgov_platform_file_transfer_test tests/file_transfer_test.cpp)
target_link_libraries(liquidgov_platform_file_transfer_test PRIVATE liquidgov_file_transfer liquidgov_image third_party_miniz)

add_executable(liquidgov_platform_messenger_test tests/messenger_test.cpp)
target_link_libraries(liquidgov_platform_messenger_test PRIVATE liquidgov_messenger)

add_executable(liquidgov_platform_program_messaging_test tests/program_messaging_test.cpp)
target_link_libraries(liquidgov_platform_program_messaging_test PRIVATE
    liquidgov_auth liquidgov_file_store liquidgov_file_transfer liquidgov_file_type liquidgov_image third_party_miniz)
target_compile_definitions(liquidgov_platform_program_messaging_test PRIVATE LG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# The store reads and writes the same bytes as before the file_store.cpp split (2026-11-12): every record type, every
# field-count variant, bad rows and their error messages, against answers written by the code before the split.
add_executable(liquidgov_platform_store_roundtrip_test tests/store_roundtrip_test.cpp)
target_link_libraries(liquidgov_platform_store_roundtrip_test PRIVATE liquidgov_auth liquidgov_file_store)
target_compile_definitions(liquidgov_platform_store_roundtrip_test PRIVATE LG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
# Same check for the record types added after that fixture (styles, here), answers by master bb6e046: a new file,
# the first round-trip test is frozen.
add_executable(liquidgov_platform_store_roundtrip_styles_here_test tests/store_roundtrip_styles_here_test.cpp)
target_link_libraries(liquidgov_platform_store_roundtrip_styles_here_test PRIVATE liquidgov_auth liquidgov_file_store)
target_compile_definitions(liquidgov_platform_store_roundtrip_styles_here_test PRIVATE LG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# End-to-end check of programs in mail against the real server on port 18080 with a throwaway store:
#   bash tests/e2e/program_mail.sh build/liquidgov_http build/liquidgov_e2e_seed
add_executable(liquidgov_e2e_seed tests/e2e/seed.cpp)
target_link_libraries(liquidgov_e2e_seed PRIVATE liquidgov_auth liquidgov_file_store)

# Site process: src/http_server.cpp is main(); the site's modules are listed in src/site/sources.cmake (one .cpp +
# .hpp per feature under src/site/core, ui and features/<name>). Links every feature library the site talks to.
find_package(Threads REQUIRED)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/site/sources.cmake)
add_executable(liquidgov_http src/http_server.cpp ${LG_SITE_SOURCES})
if(LG_PCH)
  target_precompile_headers(liquidgov_http PRIVATE ${LG_PCH_HEADERS})
endif()
# Build identity for --version (a prebuilt deploy checks it; deploy/PLAN-prebuilt.md). Empty -> "unknown".
set(LG_BUILD_COMMIT "" CACHE STRING "commit liquidgov_http is built from (printed by --version)")
set(LG_BUILD_TREE "" CACHE STRING "liquidgov-platform tree hash liquidgov_http is built from (printed by --version)")
if(LG_BUILD_COMMIT)
  target_compile_definitions(liquidgov_http PRIVATE LG_BUILD_COMMIT="${LG_BUILD_COMMIT}")
endif()
if(LG_BUILD_TREE)
  target_compile_definitions(liquidgov_http PRIVATE LG_BUILD_TREE="${LG_BUILD_TREE}")
endif()
if(LG_STATIC_RUNTIME)
  target_link_options(liquidgov_http PRIVATE -static-libstdc++ -static-libgcc)
endif()
target_include_directories(liquidgov_http PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(liquidgov_http PRIVATE
    liquidgov_auth
    liquidgov_avatar
    liquidgov_blob
    liquidgov_clock
    liquidgov_file_store
    liquidgov_file_transfer
    liquidgov_file_type
    liquidgov_messenger
    liquidgov_websocket
    Threads::Threads
)
