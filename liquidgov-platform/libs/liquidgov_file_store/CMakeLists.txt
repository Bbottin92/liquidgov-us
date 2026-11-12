# liquidgov_file_store — LGP1 snapshot store (accounts, sessions, avatars,
# messenger conversations, jukebox, etc). Depends on liquidgov_auth,
# liquidgov_avatar, liquidgov_messenger (PUBLIC, file_store.hpp);
# liquidgov_clock + libsodium are PRIVATE (the store's .cpp files only).
# The .cpp files are listed in sources.cmake (one list; deploy/rollback_guard.py and the checks read it too).
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake)
add_library(liquidgov_file_store ${LG_STORE_SOURCES})
target_include_directories(liquidgov_file_store PRIVATE src)
target_include_directories(liquidgov_file_store PUBLIC include)
target_link_libraries(liquidgov_file_store
    PUBLIC liquidgov_auth liquidgov_avatar liquidgov_messenger
    PRIVATE liquidgov_clock PkgConfig::SODIUM
)
