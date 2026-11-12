cmake_minimum_required(VERSION 3.16)
project(liquidgov_platform CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# End-to-end check of programs in mail against the real server on port 18080 with a throwaway store:
#   bash tests/e2e/program_mail.sh build/liquidgov_http build/liquidgov_e2e_seed
add_executable(liquidgov_e2e_seed tests/e2e/seed.cpp)
target_link_libraries(liquidgov_e2e_seed PRIVATE liquidgov_auth liquidgov_file_store)

# Site process: src/http_server.cpp is main(); the site's modules are listed in src/site/sources.cmake (one .cpp +
# .hpp per feature under src/site/core, ui and features/<name>). Links every feature library the site talks to.
find_package(Threads REQUIRED)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/site/sources.cmake)
add_executable(liquidgov_http src/http_server.cpp ${LG_SITE_SOURCES})
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
