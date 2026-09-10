#include "liquidgov/platform/image_normalize.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace liquidgov::platform {

namespace {

void append_to_vector(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(context);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

} // namespace

NormalizeResult normalize_image(const std::vector<std::uint8_t>& input_bytes) {
    NormalizeResult result;

    int width = 0, height = 0, channels = 0;
    // stbi_load_from_memory decodes real pixel data or returns null.
    // It does not care what the caller claimed the type was — a file
    // whose bytes don't actually decode as a real image fails here
    // regardless of extension, declared MIME type, or magic-byte prefix.
    unsigned char* pixels = stbi_load_from_memory(
        input_bytes.data(), static_cast<int>(input_bytes.size()), &width,
        &height, &channels, 4 /* force RGBA */);

    if (pixels == nullptr) {
        result.ok = false;
        return result;
    }

    std::vector<std::uint8_t> encoded;
    // Re-encode from the decoded pixels only — the original input bytes
    // are never stored or forwarded past this point. Whatever markup,
    // metadata, or embedded payload the original file carried does not
    // survive into png_bytes; only pixel data does.
    int written = stbi_write_png_to_func(append_to_vector, &encoded, width, height,
                                          4, pixels, width * 4);
    stbi_image_free(pixels);

    if (!written) {
        result.ok = false;
        return result;
    }

    result.ok = true;
    result.png_bytes = std::move(encoded);
    result.width = width;
    result.height = height;
    return result;
}

std::vector<std::uint8_t> encode_png(int width, int height,
                                      const std::vector<std::uint8_t>& rgba_pixels) {
    std::vector<std::uint8_t> encoded;
    stbi_write_png_to_func(append_to_vector, &encoded, width, height, 4,
                            rgba_pixels.data(), width * 4);
    return encoded;
}

} // namespace liquidgov::platform
