# liquidgov_avatar — normalize + scan + blob for self-portraits. Depends on
# liquidgov_account, liquidgov_blob, liquidgov_scanner (PUBLIC, avatar.hpp);
# liquidgov_clock, liquidgov_image are PRIVATE (avatar.cpp only).
add_library(liquidgov_avatar src/avatar.cpp)
target_include_directories(liquidgov_avatar PUBLIC include)
target_link_libraries(liquidgov_avatar
    PUBLIC liquidgov_account liquidgov_blob liquidgov_scanner
    PRIVATE liquidgov_clock liquidgov_image
)
