# Vendored stb_image / stb_image_write — https://github.com/nothings/stb, public domain / MIT.
# Header-only; the implementation TU (STB_IMAGE_IMPLEMENTATION) lives in liquidgov_image.
add_library(third_party_stb INTERFACE)
target_include_directories(third_party_stb INTERFACE stb)
