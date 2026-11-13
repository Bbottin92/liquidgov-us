# liquidgov_password — argon2id hashing (libsodium). password.hpp declares
# sodium-backed types, so the dependency is PUBLIC.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
add_library(liquidgov_password src/password.cpp)
target_include_directories(liquidgov_password PUBLIC include)
# LG_STATIC_RUNTIME (top-level CMakeLists.txt): the static libsodium.a when it exists, so a prebuilt binary does not
# need libsodium on the server; otherwise the shared library as always.
find_library(LG_SODIUM_STATIC NAMES libsodium.a)
if(LG_STATIC_RUNTIME AND LG_SODIUM_STATIC)
  target_include_directories(liquidgov_password PUBLIC ${SODIUM_INCLUDE_DIRS})
  target_link_libraries(liquidgov_password PUBLIC ${LG_SODIUM_STATIC})
else()
  target_link_libraries(liquidgov_password PUBLIC PkgConfig::SODIUM)
endif()
