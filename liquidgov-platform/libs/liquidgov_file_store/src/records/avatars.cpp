// avatars: Avatars (AvatarStore).
// LGP1 record types: av (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <optional>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

std::optional<Avatar> FileStore::get(AccountId owner) const {
    auto it = avatars_.find(owner);
    if (it == avatars_.end()) return std::nullopt;
    return it->second;
}

void FileStore::set(const Avatar& avatar) {
    avatars_[avatar.owner] = avatar;
    flush();
}

void FileStore::erase(AccountId owner) {
    avatars_.erase(owner);
    flush();
}

std::vector<Avatar> FileStore::all_avatars() const {
    std::vector<Avatar> out;
    out.reserve(avatars_.size());
    for (const auto& [id, av] : avatars_) out.push_back(av);
    return out;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_avatars(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "av") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad av record");
        Avatar av;
        av.owner = parse_u64(f[1], "avatar owner");
        av.blob_id = pct_decode(f[2]);
        av.created_at = parse_i64(f[3], "avatar ts");
        if (av.owner == 0 || av.blob_id.empty()) throw std::runtime_error("LGP1: empty av record");
        avatars_[av.owner] = av;
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_avatars(std::ostream& body) {
    for (const auto& [id, av] : avatars_) {
        if (!accounts_.count(id)) continue;
        body << "av\t" << av.owner << "\t" << pct_encode(av.blob_id) << "\t" << av.created_at << "\n";
    }
}

} // namespace liquidgov::platform
