# liquidgov_blob — content-addressed blob store (sha256 via libsodium, PRIVATE:
# only used inside blob.cpp, not declared in blob.hpp).
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
add_library(liquidgov_blob src/blob.cpp)
target_include_directories(liquidgov_blob PUBLIC include)
target_link_libraries(liquidgov_blob PRIVATE PkgConfig::SODIUM)
