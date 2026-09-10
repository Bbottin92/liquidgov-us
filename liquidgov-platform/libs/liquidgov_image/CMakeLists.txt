# liquidgov_image — stb-based decode/re-encode to PNG. third_party_stb is
# PRIVATE: image_normalize.cpp defines STB_IMAGE_IMPLEMENTATION itself and
# the stb headers aren't exposed via image_normalize.hpp.
add_library(liquidgov_image src/image_normalize.cpp)
target_include_directories(liquidgov_image PUBLIC include)
target_link_libraries(liquidgov_image PRIVATE third_party_stb)
