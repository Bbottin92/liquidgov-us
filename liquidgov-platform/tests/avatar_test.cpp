#include "liquidgov/platform/avatar.hpp"

#include "liquidgov/platform/image_normalize.hpp"

#include <cassert>
#include <iostream>
#include <map>

using namespace liquidgov::platform;

namespace {

// Real, validly-encoded test images — generated with the actual PNG
// encoder rather than hand-written byte arrays. A hand-crafted "PNG"
// with guessed CRC/zlib bytes is exactly the kind of fragile fixture
// that looks plausible and isn't actually valid; using the real encoder
// avoids that failure mode entirely.
std::vector<std::uint8_t> solid_color_png(int width, int height, std::uint8_t r,
                                           std::uint8_t g, std::uint8_t b) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
        pixels[i + 3] = 255;
    }
    return encode_png(width, height, pixels);
}

std::vector<std::uint8_t> not_an_image() { return {0x00, 0x01, 0x02, 0x03}; }

class InMemoryAvatarStore : public AvatarStore {
public:
    std::optional<Avatar> get(AccountId owner) const override {
        auto it = data_.find(owner);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }
    void set(const Avatar& avatar) override { data_[avatar.owner] = avatar; }
    void erase(AccountId owner) override { data_.erase(owner); }

private:
    std::map<AccountId, Avatar> data_;
};

class AlwaysClean : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>&) const override { return true; }
};

class AlwaysDirty : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>&) const override { return false; }
};

} // namespace

int main() {
    FilesystemBlobStore blobs("/tmp/lgp-avatar-test-store");
    InMemoryAvatarStore avatars;
    AlwaysClean clean_scanner;

    auto first_drawing = solid_color_png(4, 4, 200, 50, 50);   // red-ish
    auto second_drawing = solid_color_png(4, 4, 50, 50, 200);  // blue-ish, genuinely different

    // 1. Saving a real drawing works
    auto r1 = save_avatar(blobs, avatars, clean_scanner, /*owner=*/1, first_drawing);
    assert(r1.error == SaveAvatarError::None);
    assert(r1.avatar.has_value());
    BlobId first_blob = r1.avatar->blob_id;
    assert(blobs.meta(first_blob)->refcount == 1);
    std::cout << "save_avatar stores a valid drawing\n";

    // 2. Replacing with a different drawing releases the old blob
    auto r2 = save_avatar(blobs, avatars, clean_scanner, /*owner=*/1, second_drawing);
    assert(r2.error == SaveAvatarError::None);
    assert(r2.avatar->blob_id != first_blob);
    assert(!blobs.meta(first_blob).has_value()); // old one released, refcount hit zero
    std::cout << "replacing an avatar releases the previous blob\n";

    // 3. Self-referential edge case: re-uploading the SAME image the
    //    account already has as its avatar must not delete it out from
    //    under itself (transient refcount-to-zero during the swap).
    BlobId current_blob = r2.avatar->blob_id;
    auto r3 = save_avatar(blobs, avatars, clean_scanner, /*owner=*/1, second_drawing);
    assert(r3.error == SaveAvatarError::None);
    assert(r3.avatar->blob_id == current_blob); // same content -> same blob id
    assert(blobs.get(current_blob).has_value()); // still there, not deleted
    std::cout << "re-uploading the current avatar's own image does not delete it\n";

    // 4. Non-image bytes rejected regardless of any claimed type
    auto r4 = save_avatar(blobs, avatars, clean_scanner, /*owner=*/2, not_an_image());
    assert(r4.error == SaveAvatarError::NotAnImage);
    std::cout << "non-image bytes rejected by normalize_image\n";

    // 5. A real image that fails the malware scan is rejected even
    //    though it decoded fine
    AlwaysDirty dirty_scanner;
    auto r5 = save_avatar(blobs, avatars, dirty_scanner, /*owner=*/3, first_drawing);
    assert(r5.error == SaveAvatarError::FailedScan);
    std::cout << "a real image that fails the malware scan is still rejected\n";

    // 6. Oversized byte payload rejected before decode
    std::vector<std::uint8_t> huge(kAvatarMaxUploadBytes + 1, 0);
    auto r6 = save_avatar(blobs, avatars, clean_scanner, /*owner=*/4, huge);
    assert(r6.error == SaveAvatarError::TooLarge);
    std::cout << "oversized upload rejected\n";

    // 7. clear_avatar releases the blob
    clear_avatar(blobs, avatars, /*owner=*/1);
    assert(!avatars.get(1).has_value());
    std::cout << "clear_avatar drops the row\n";

    std::cout << "\nall avatar tests passed\n";
    return 0;
}
