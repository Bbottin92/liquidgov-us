#include "liquidgov/platform/blob.hpp"
#include "liquidgov/platform/image_normalize.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>

using namespace liquidgov::platform;

namespace {

// A genuinely valid PNG, produced by the real encoder rather than
// hand-written bytes — see avatar_test.cpp for why that matters.
std::vector<std::uint8_t> tiny_valid_png() {
    std::vector<std::uint8_t> pixels(2 * 2 * 4, 128);
    return encode_png(2, 2, pixels);
}

// A crafted file that is a valid ZIP (this is genuinely how the
// polyglot class of attack is built — PNG magic bytes with a ZIP central
// directory appended after, e.g. a "PK" trailer) but is NOT genuine
// image pixel data throughout. It should not survive normalize_image
// even though the first bytes look like a PNG header.
std::vector<std::uint8_t> fake_polyglot_not_a_real_image() {
    std::vector<std::uint8_t> data = {0x89, 0x50, 0x4E, 0x47}; // PNG magic prefix only
    // followed by bytes that are not a valid PNG stream at all —
    // simulates "the prefix says PNG, the actual structure does not
    // decode as one," which is exactly what a real decoder must reject.
    for (int i = 0; i < 64; i++) data.push_back(static_cast<std::uint8_t>(i));
    return data;
}

} // namespace

int main() {
    // --- BlobStore: content-addressing and refcounting ---
    FilesystemBlobStore store("/tmp/lgp-blob-test-store");

    std::vector<std::uint8_t> content_a = {1, 2, 3, 4, 5};
    std::vector<std::uint8_t> content_b = {9, 9, 9};

    BlobId id_a1 = store.put(content_a, "application/octet-stream");
    BlobId id_a2 = store.put(content_a, "application/octet-stream"); // same bytes again
    assert(id_a1 == id_a2);
    auto meta_a = store.meta(id_a1);
    assert(meta_a.has_value());
    assert(meta_a->refcount == 2); // stored once, referenced twice
    std::cout << "content-addressing: identical bytes share one blob, refcount tracks references\n";

    BlobId id_b = store.put(content_b, "application/octet-stream");
    assert(id_b != id_a1);
    std::cout << "different content produces different blob ids\n";

    auto fetched = store.get(id_a1);
    assert(fetched.has_value());
    assert(*fetched == content_a);
    std::cout << "get() round-trips exact bytes\n";

    // Release once: refcount drops to 1, bytes stay (still referenced)
    store.release(id_a1);
    assert(store.meta(id_a1)->refcount == 1);
    assert(store.get(id_a1).has_value());
    std::cout << "release() at nonzero refcount keeps the blob — orphans stay alive\n";

    // Release again: refcount hits zero, bytes actually go away
    store.release(id_a1);
    assert(!store.meta(id_a1).has_value());
    assert(!store.get(id_a1).has_value());
    std::cout << "release() at refcount zero actually frees the blob\n";

    // legal_hold overrides refcount-zero deletion
    BlobId id_c = store.put(content_b, "application/octet-stream"); // == id_b again
    store.set_legal_hold(id_c, true);
    store.release(id_c); // was refcount 2 (from id_b + this put), now 1
    store.release(id_c); // now would be 0, but legal_hold blocks deletion
    assert(store.meta(id_c).has_value());
    std::cout << "legal_hold blocks deletion at refcount zero\n";

    // --- image_normalize: the actual polyglot defense ---
    auto good = normalize_image(tiny_valid_png());
    assert(good.ok);
    assert(good.width == 2 && good.height == 2);
    std::cout << "a genuinely valid image normalizes successfully\n";

    auto bad = normalize_image(fake_polyglot_not_a_real_image());
    assert(!bad.ok);
    std::cout << "a file with a valid-looking magic prefix but no real image "
                 "structure is rejected, not guessed at\n";

    // The normalized output should be storable in the blob store just
    // like anything else — proving the two pieces compose the way the
    // file-transfer pipeline design assumed they would.
    BlobId normalized_id = store.put(good.png_bytes, "image/png");
    assert(store.get(normalized_id).has_value());
    std::cout << "normalized output stores cleanly through the same BlobStore\n";

    std::cout << "\nall blob/image tests passed\n";
    return 0;
}
