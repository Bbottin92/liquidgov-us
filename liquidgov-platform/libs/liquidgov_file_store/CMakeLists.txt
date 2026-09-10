# liquidgov_file_store — LGP1 snapshot store (accounts, sessions, avatars,
# messenger conversations, jukebox, etc). Depends on liquidgov_auth,
# liquidgov_avatar, liquidgov_messenger (PUBLIC, file_store.hpp);
# liquidgov_clock + libsodium are PRIVATE (file_store.cpp only).
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
add_library(liquidgov_file_store src/file_store.cpp)
target_include_directories(liquidgov_file_store PUBLIC include)
target_link_libraries(liquidgov_file_store
    PUBLIC liquidgov_auth liquidgov_avatar liquidgov_messenger
    PRIVATE liquidgov_clock PkgConfig::SODIUM
)
