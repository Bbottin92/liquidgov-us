#include "liquidgov/platform/blob.hpp"

#include <sodium.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace liquidgov::platform {

namespace {
bool ensure_sodium_init() {
    static const bool ok = (sodium_init() >= 0);
    return ok;
}
} // namespace

std::string sha256_hex(const std::vector<std::uint8_t>& bytes) {
    if (!ensure_sodium_init()) {
        throw std::runtime_error("libsodium init failed");
    }
    unsigned char digest[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(digest, bytes.data(), bytes.size());

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(sizeof(digest) * 2);
    for (unsigned char b : digest) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0f]);
    }
    return out;
}

FilesystemBlobStore::FilesystemBlobStore(std::string root_dir) : root_(std::move(root_dir)) {
    fs::create_directories(root_);
}

std::string FilesystemBlobStore::path_for(const BlobId& id) const {
    // Two-level fan-out (root/ab/abcdef...) so a real deployment doesn't
    // put millions of files in one directory. Harmless at test scale,
    // real at production scale.
    return root_ + "/" + id.substr(0, 2) + "/" + id;
}

BlobId FilesystemBlobStore::put(const std::vector<std::uint8_t>& bytes,
                                 const std::string& mime_type) {
    BlobId id = sha256_hex(bytes);

    auto it = meta_.find(id);
    if (it == meta_.end()) {
        fs::path p = path_for(id);
        fs::create_directories(p.parent_path());
        // Written to a temp file, checked, then renamed: on a full disk the
        // write fails and nothing is recorded, instead of a cut-off blob
        // kept as good (review 2026-10-27). The caller's request fails.
        fs::path tmp = p;
        tmp += ".part";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size()));
            out.close();
            if (!out) {
                std::error_code ec;
                fs::remove(tmp, ec);
                throw std::runtime_error("blob write failed: " + p.string());
            }
        }
        std::error_code rename_ec;
        fs::rename(tmp, p, rename_ec);
        if (rename_ec) {
            fs::remove(tmp, rename_ec);
            throw std::runtime_error("blob rename failed: " + p.string());
        }

        BlobMeta m;
        m.id = id;
        m.size = bytes.size();
        m.mime_type = mime_type;
        m.refcount = 1;
        meta_[id] = m;
    } else {
        it->second.refcount++;
    }
    return id;
}

std::optional<std::vector<std::uint8_t>> FilesystemBlobStore::get(const BlobId& id) const {
    if (!meta_.count(id)) return std::nullopt;
    std::ifstream in(path_for(id), std::ios::binary);
    if (!in) return std::nullopt;
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

std::optional<BlobMeta> FilesystemBlobStore::meta(const BlobId& id) const {
    auto it = meta_.find(id);
    if (it == meta_.end()) return std::nullopt;
    return it->second;
}

void FilesystemBlobStore::release(const BlobId& id) {
    auto it = meta_.find(id);
    if (it == meta_.end()) return;
    if (it->second.refcount > 0) it->second.refcount--;

    // Deletes at refcount zero UNLESS legal_hold — a moderation/legal
    // exemption takes priority over ordinary refcount cleanup.
    if (it->second.refcount == 0 && !it->second.legal_hold) {
        std::error_code ec;
        fs::remove(path_for(id), ec);
        meta_.erase(it);
    }
}

void FilesystemBlobStore::set_legal_hold(const BlobId& id, bool hold) {
    auto it = meta_.find(id);
    if (it != meta_.end()) it->second.legal_hold = hold;
}

void FilesystemBlobStore::adopt(const BlobId& id, const std::string& mime_type) {
    if (id.size() < 8) return;
    auto it = meta_.find(id);
    if (it != meta_.end()) {
        it->second.refcount++;
        return;
    }
    fs::path p = path_for(id);
    if (!fs::exists(p) || !fs::is_regular_file(p)) return;
    BlobMeta m;
    m.id = id;
    m.size = fs::file_size(p);
    m.mime_type = mime_type.empty() ? "application/octet-stream" : mime_type;
    m.refcount = 1;
    meta_[id] = m;
}

} // namespace liquidgov::platform
