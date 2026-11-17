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

std::vector<Avatar> FileStore::portrait_history(AccountId owner) const {
    auto it = portrait_history_.find(owner);
    return it == portrait_history_.end() ? std::vector<Avatar>{} : it->second;
}

std::vector<std::string> FileStore::keep_portrait(AccountId owner, const std::string& blob_id, std::int64_t at) {
    auto& list = portrait_history_[owner];
    Avatar a;
    a.owner = owner;
    a.blob_id = blob_id;
    a.created_at = at;
    list.push_back(a);
    std::vector<std::string> dropped;
    while (list.size() > kPortraitHistoryKeep) {
        dropped.push_back(list.front().blob_id);
        list.erase(list.begin());
    }
    flush();
    return dropped;
}

std::optional<std::string> FileStore::drop_portrait(AccountId owner, std::size_t index) {
    auto it = portrait_history_.find(owner);
    if (it == portrait_history_.end() || index >= it->second.size()) return std::nullopt;
    std::string blob = it->second[index].blob_id;
    it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(index));
    if (it->second.empty()) portrait_history_.erase(it);
    flush();
    return blob;
}

bool FileStore::portrait_history_public(AccountId owner) const {
    auto it = portrait_history_public_.find(owner);
    return it != portrait_history_public_.end() && it->second;
}

void FileStore::set_portrait_history_public(AccountId owner, bool on) {
    if (on) portrait_history_public_[owner] = true;
    else portrait_history_public_.erase(owner);
    flush();
}

std::vector<std::string> FileStore::all_portrait_history_blob_ids() const {
    std::vector<std::string> out;
    for (const auto& [id, list] : portrait_history_)
        for (const auto& a : list) out.push_back(a.blob_id);
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
    if (tag == "avh") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad avh record");
        Avatar av;
        av.owner = parse_u64(f[1], "portrait history owner");
        av.blob_id = pct_decode(f[2]);
        av.created_at = parse_i64(f[3], "portrait history ts");
        if (av.owner == 0 || av.blob_id.empty()) throw std::runtime_error("LGP1: empty avh record");
        portrait_history_[av.owner].push_back(av);   // file order is oldest first
        return true;
    }
    if (tag == "avhs") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad avhs record");
        AccountId id = parse_u64(f[1], "portrait history setting account");
        if (id == 0) throw std::runtime_error("LGP1: empty avhs record");
        portrait_history_public_[id] = parse_bit(f[2], "portrait history public");
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
    for (const auto& [id, list] : portrait_history_) {
        if (!accounts_.count(id)) continue;
        for (const auto& av : list)
            body << "avh\t" << id << "\t" << pct_encode(av.blob_id) << "\t" << av.created_at << "\n";
    }
    for (const auto& [id, on] : portrait_history_public_) {
        if (!accounts_.count(id) || !on) continue;
        body << "avhs\t" << id << "\t1\n";
    }
}

} // namespace liquidgov::platform
