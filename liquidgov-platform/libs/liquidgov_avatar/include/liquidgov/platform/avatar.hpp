#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "liquidgov/platform/account.hpp"
#include "liquidgov/platform/blob.hpp"
#include "liquidgov/platform/scanner.hpp"

namespace liquidgov::platform {

inline constexpr std::size_t kAvatarMaxUploadBytes = 2 * 1024 * 1024;
inline constexpr int kAvatarMaxEdgePx = 1024;

struct Avatar {
    AccountId owner = 0;
    BlobId blob_id;
    std::int64_t created_at = 0;
};

enum class SaveAvatarError {
    None,
    NotAnImage,
    FailedScan,
    TooLarge,
};

struct SaveAvatarResult {
    SaveAvatarError error = SaveAvatarError::None;
    std::optional<Avatar> avatar;
};

class AvatarStore {
public:
    virtual ~AvatarStore() = default;
    virtual std::optional<Avatar> get(AccountId owner) const = 0;
    virtual void set(const Avatar& avatar) = 0;
    virtual void erase(AccountId owner) = 0;
};

SaveAvatarResult save_avatar(BlobStore& blobs, AvatarStore& avatars, MalwareScanner& scanner,
                              AccountId owner, const std::vector<std::uint8_t>& exported_bytes);

void clear_avatar(BlobStore& blobs, AvatarStore& avatars, AccountId owner);

} // namespace liquidgov::platform
