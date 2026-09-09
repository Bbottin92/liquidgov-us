cmake_minimum_required(VERSION 3.16)
project(liquidgov_platform CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_library(liquidgov_platform
    src/password.cpp
    src/session.cpp
    src/auth.cpp
    src/file_store.cpp
    src/blob.cpp
    src/image_normalize.cpp
    src/avatar.cpp
    src/file_type.cpp
    src/file_transfer.cpp
    src/messenger.cpp
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)
target_include_directories(liquidgov_platform PUBLIC include)
target_include_directories(liquidgov_platform PRIVATE third_party/stb third_party/miniz)
target_link_libraries(liquidgov_platform PUBLIC PkgConfig::SODIUM)

add_executable(liquidgov_platform_auth_test tests/auth_test.cpp)
target_link_libraries(liquidgov_platform_auth_test PRIVATE liquidgov_platform)

add_executable(liquidgov_platform_file_store_test tests/file_store_test.cpp)
target_link_libraries(liquidgov_platform_file_store_test PRIVATE liquidgov_platform)

add_executable(liquidgov_platform_blob_test tests/blob_test.cpp)
target_link_libraries(liquidgov_platform_blob_test PRIVATE liquidgov_platform)

add_executable(liquidgov_platform_avatar_test tests/avatar_test.cpp)
target_link_libraries(liquidgov_platform_avatar_test PRIVATE liquidgov_platform)

add_executable(liquidgov_platform_file_transfer_test tests/file_transfer_test.cpp)
target_include_directories(liquidgov_platform_file_transfer_test PRIVATE third_party/miniz)
target_link_libraries(liquidgov_platform_file_transfer_test PRIVATE liquidgov_platform)

add_executable(liquidgov_platform_messenger_test tests/messenger_test.cpp)
target_link_libraries(liquidgov_platform_messenger_test PRIVATE liquidgov_platform)

# Site process. Linked with blob/avatar/image_normalize for self-portraits,
# file_type/file_transfer (+ miniz) for mail attachments, and messenger for
# group conversations (Slice C — FileStore implements ConversationStore).
add_executable(liquidgov_http
    src/http_server.cpp
    src/auth.cpp
    src/password.cpp
    src/session.cpp
    src/file_store.cpp
    src/blob.cpp
    src/avatar.cpp
    src/image_normalize.cpp
    src/file_type.cpp
    src/file_transfer.cpp
    src/messenger.cpp
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)
target_include_directories(liquidgov_http PRIVATE include third_party/stb third_party/miniz)
target_link_libraries(liquidgov_http PRIVATE PkgConfig::SODIUM)
