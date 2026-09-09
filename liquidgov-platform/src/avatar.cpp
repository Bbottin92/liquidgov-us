#include "liquidgov/platform/avatar.hpp"

#include "liquidgov/platform/clock.hpp"
#include "liquidgov/platform/image_normalize.hpp"

namespace liquidgov::platform {

SaveAvatarResult save_avatar(BlobStore& blobs, AvatarStore& avatars, MalwareScanner& scanner,
                              AccountId owner, const std::vector<std::uint8_t>& exported_bytes) {
    if (exported_bytes.size() > kAvatarMaxUploadBytes) {
        return {SaveAvatarError::TooLarge, std::nullopt};
    }

    auto normalized = normalize_image(exported_bytes);
    if (!normalized.ok) {
        return {SaveAvatarError::NotAnImage, std::nullopt};
    }
    if (normalized.width > kAvatarMaxEdgePx || normalized.height > kAvatarMaxEdgePx) {
        return {SaveAvatarError::TooLarge, std::nullopt};
    }

    if (!scanner.is_clean(normalized.png_bytes)) {
        return {SaveAvatarError::FailedScan, std::nullopt};
    }

    BlobId new_blob = blobs.put(normalized.png_bytes, "image/png");

    auto existing = avatars.get(owner);
    if (existing) {
        blobs.release(existing->blob_id);
    }

    Avatar avatar;
    avatar.owner = owner;
    avatar.blob_id = new_blob;
    avatar.created_at = now();
    avatars.set(avatar);

    return {SaveAvatarError::None, avatar};
}

void clear_avatar(BlobStore& blobs, AvatarStore& avatars, AccountId owner) {
    auto existing = avatars.get(owner);
    if (!existing) return;
    blobs.release(existing->blob_id);
    avatars.erase(owner);
}

} // namespace liquidgov::platform
