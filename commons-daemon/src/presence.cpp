#include "presence.hpp"

#include <algorithm>

namespace commons {

bool PresenceStore::erase_id_locked(const std::string& id) {
    for (auto it = regions_.begin(); it != regions_.end(); ++it) {
        auto& list = it->second;
        auto before = list.size();
        list.erase(std::remove_if(list.begin(), list.end(),
                                   [&](const Occupant& o) { return o.id == id; }),
                   list.end());
        if (list.size() != before) {
            if (total_ > 0) --total_;
            if (list.empty()) regions_.erase(it);
            return true; // one id, one row
        }
    }
    return false;
}

PresenceStore::JoinResult PresenceStore::join_locked(const std::string& region, const Occupant& occ) {
    auto it = regions_.find(region);
    if (it != regions_.end()) {
        for (auto& o : it->second) {
            if (o.id == occ.id) {
                o = occ; // same region: replace, do not ++total_
                return JoinResult::Ok;
            }
        }
    }

    const std::size_t dest_n = (it == regions_.end()) ? 0 : it->second.size();
    if (dest_n >= kPerRegionCap) return JoinResult::RegionFull;

    // Moving from another region does not consume a new global slot.
    // Check the cap *before* erase so a RegionFull/GlobalFull leave them
    // where they were (rekey must not drop a walker into the void).
    const bool existed = erase_id_locked(occ.id);
    if (!existed && total_ >= kGlobalCap) return JoinResult::GlobalFull;

    regions_[region].push_back(occ);
    ++total_;
    return JoinResult::Ok;
}

PresenceStore::JoinResult PresenceStore::join(const std::string& region, const Occupant& occ) {
    std::lock_guard<std::mutex> lock(mu_);
    return join_locked(region, occ);
}

bool PresenceStore::move(const std::string& region, const std::string& id, double x, double y, int facing) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = regions_.find(region);
    if (it == regions_.end()) return false;
    for (auto& o : it->second) {
        if (o.id == id) {
            o.x = x; o.y = y; o.facing = facing;
            return true;
        }
    }
    return false;
}

PresenceStore::JoinResult PresenceStore::rekey(const std::string& from_region,
                                                const std::string& to_region,
                                                const Occupant& occ) {
    std::lock_guard<std::mutex> lock(mu_);
    // join_locked de-dupes globally and refuses dest-full *before* erase,
    // so a failed walk into a packed region keeps the occupant in from_region.
    // from_region is the caller's claim; the store trusts id uniqueness, not
    // that string, so a mismatched from still cannot create a second row.
    (void)from_region;
    return join_locked(to_region, occ);
}

void PresenceStore::leave(const std::string& region, const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = regions_.find(region);
    if (it == regions_.end()) return;
    auto& list = it->second;
    auto before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(),
                               [&](const Occupant& o) { return o.id == id; }),
               list.end());
    if (list.size() != before && total_ > 0) --total_;
    if (list.empty()) regions_.erase(it);
}

std::vector<Occupant> PresenceStore::occupants(const std::string& region) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = regions_.find(region);
    if (it == regions_.end()) return {};
    return it->second; // copy out under the lock, caller works on the snapshot
}

std::size_t PresenceStore::global_count() {
    std::lock_guard<std::mutex> lock(mu_);
    return total_;
}

} // namespace commons
