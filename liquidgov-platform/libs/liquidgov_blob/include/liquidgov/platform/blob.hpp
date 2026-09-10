#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// One content-addressed blob store, shared across the gallery,
// file-transfer, and self-portrait slices — not a separate abstraction
// per feature. See the design notes: install/publish/upload all reduce
// to put(); moderation and account-deletion cleanup all reduce to
// release() and set_legal_hold().

namespace liquidgov::platform {

using BlobId = std::string; // hex sha256 of the stored bytes

std::string sha256_hex(const std::vector<std::uint8_t>& bytes);

struct BlobMeta {
    BlobId id;
    std::uint64_t size = 0;
    std::string mime_type;
    std::uint32_t refcount = 0;
    // Moderation/legal exemption from refcount-zero deletion — the same
    // field that makes object-level takedown and account-deletion
    // interact correctly: a takedown sets this and purges regardless of
    // refcount; deletion decrements refcount and only actually frees
    // bytes when this is false.
    bool legal_hold = false;
};

// Storage + refcount metadata. A real deployment backs this with real
// object storage and a real database row per blob — same separation
// from this interface as AccountStore is from auth.cpp.
class BlobStore {
public:
    virtual ~BlobStore() = default;

    // Stores bytes if not already present by hash, increments refcount
    // either way. Idempotent by design: installing the same gallery
    // theme twice, or two people sending the same file, does not
    // double-store.
    virtual BlobId put(const std::vector<std::uint8_t>& bytes, const std::string& mime_type) = 0;

    virtual std::optional<std::vector<std::uint8_t>> get(const BlobId& id) const = 0;
    virtual std::optional<BlobMeta> meta(const BlobId& id) const = 0;

    // Decrements refcount; deletes the bytes only at refcount zero and
    // only when legal_hold is false. This is the mechanism behind
    // "orphans stay" everywhere in the design: releasing one reference
    // never removes a blob other references still depend on.
    virtual void release(const BlobId& id) = 0;

    virtual void set_legal_hold(const BlobId& id, bool hold) = 0;
};

// Real, working implementation: bytes on disk under a content-addressed
// path, refcount/legal_hold metadata in memory. A production deployment
// would likely move the metadata to a real database while keeping this
// exact on-disk byte layout — the two concerns are already separated
// here for that reason.
class FilesystemBlobStore : public BlobStore {
public:
    explicit FilesystemBlobStore(std::string root_dir);

    BlobId put(const std::vector<std::uint8_t>& bytes, const std::string& mime_type) override;
    std::optional<std::vector<std::uint8_t>> get(const BlobId& id) const override;
    std::optional<BlobMeta> meta(const BlobId& id) const override;
    void release(const BlobId& id) override;
    void set_legal_hold(const BlobId& id, bool hold) override;

    // After restart, meta_ is empty even if bytes are on disk. Seed one
    // id from an LGP1 av record. Missing file is a no-op — the portrait
    // 404s; we do not fail-closed the site.
    void adopt(const BlobId& id, const std::string& mime_type);

private:
    std::string root_;
    std::map<BlobId, BlobMeta> meta_;
    std::string path_for(const BlobId& id) const;
};

} // namespace liquidgov::platform
