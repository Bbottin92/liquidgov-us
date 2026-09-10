# liquidgov_session — session ids (libsodium randomness). Depends on
# liquidgov_account (session.hpp) and libsodium (PUBLIC, leaks via session.hpp).
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
add_library(liquidgov_session src/session.cpp)
target_include_directories(liquidgov_session PUBLIC include)
target_link_libraries(liquidgov_session PUBLIC liquidgov_account PkgConfig::SODIUM)
