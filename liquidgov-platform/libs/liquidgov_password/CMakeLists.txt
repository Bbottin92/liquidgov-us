# liquidgov_password — argon2id hashing (libsodium). password.hpp declares
# sodium-backed types, so the dependency is PUBLIC.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
add_library(liquidgov_password src/password.cpp)
target_include_directories(liquidgov_password PUBLIC include)
target_link_libraries(liquidgov_password PUBLIC PkgConfig::SODIUM)
