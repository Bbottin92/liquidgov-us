#include "presence.hpp"

#include <algorithm>

namespace commons {

PresenceStore::JoinResult PresenceStore::join_locked(const std::string& region, const Occupant& occ) {
    if (total_ >= kGlobalCap) return JoinResult::GlobalFull;
    auto& list = regions_[region];
    if (list.size() >= kPerRegionCap) return JoinResult::RegionFull;
    list.push_back(occ);
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
    auto it = regions_.find(from_region);
    if (it != regions_.end()) {
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                   [&](const Occupant& o) { return o.id == occ.id; }),
                   list.end());
        if (list.empty()) regions_.erase(it);
        if (total_ > 0) --total_; // will be re-added by join_locked below
    }
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
