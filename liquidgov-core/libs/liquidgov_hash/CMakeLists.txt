# liquidgov_hash — SHA-256 hashing utilities. No internal dependencies.
# Self-contained: copy this folder into another project and add_subdirectory it.
add_library(liquidgov_hash src/hash.cpp)
target_include_directories(liquidgov_hash PUBLIC include)
