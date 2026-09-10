# liquidgov_law — ThreeLaws compliance check. No internal dependencies.
add_library(liquidgov_law src/law.cpp)
target_include_directories(liquidgov_law PUBLIC include)
