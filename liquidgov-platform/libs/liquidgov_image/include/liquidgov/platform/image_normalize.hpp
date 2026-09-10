#pragma once

#include <cstdint>
#include <vector>

// The actual mechanism behind "normalize images, decode with a real
// decoder, re-encode to a single allowed type, store that" from the
// file-transfer design. This is the polyglot kill for the image class:
// only genuine decoded pixel data ever reaches the Blob store. A file
// crafted to be simultaneously valid as an image and something else
// (a GIFAR-class attack) cannot survive a real decode — if it does not
// decode as exactly one codec, this rejects it outright rather than
// guessing which interpretation was intended.

namespace liquidgov::platform {

struct NormalizeResult {
    bool ok = false;
    std::vector<std::uint8_t> png_bytes; // only meaningful when ok == true
    int width = 0;
    int height = 0;
};

NormalizeResult normalize_image(const std::vector<std::uint8_t>& input_bytes);

// Encodes raw RGBA pixel data to a real PNG. Exposed primarily so tests
// (and anything else that legitimately has raw pixels, like the drawing
// canvas export path itself) can produce genuinely valid image bytes
// rather than hand-writing them — the failure mode that produces is
// exactly "looks like a plausible fixture, isn't actually a valid file,"
// which is a bad way to test a decoder.
std::vector<std::uint8_t> encode_png(int width, int height,
                                      const std::vector<std::uint8_t>& rgba_pixels);

} // namespace liquidgov::platform
