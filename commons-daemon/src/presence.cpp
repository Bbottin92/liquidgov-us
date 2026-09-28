#include "presence.hpp"

#include <algorithm>
#include <chrono>

namespace commons {

namespace {
long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
} // namespace

void PresenceStore::reap_locked(long long now) {
    for (auto it = regions_.begin(); it != regions_.end(); ) {
        auto& list = it->second;
        auto before = list.size();
        list.erase(std::remove_if(list.begin(), list.end(),
                                   [&](const Occupant& o) { return now - o.last_update_ms > kIdleMs; }),
                   list.end());
        total_ -= (before - list.size());
        if (list.empty()) it = regions_.erase(it); else ++it;
    }
}

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

std::string PresenceStore::disambiguate_name_locked(const std::string& region, const std::string& base_name,
                                                     const std::string& exclude_id) {
    auto it = regions_.find(region);
    if (it == regions_.end()) return base_name;
    auto taken = [&](const std::string& name) {
        for (const auto& o : it->second) {
            if (o.id != exclude_id && o.display_name == name) return true;
        }
        return false;
    };
    if (!taken(base_name)) return base_name;
    for (std::size_t n = 2; n <= kPerRegionCap + 1; ++n) {
        std::string candidate = base_name + " (" + std::to_string(n) + ")";
        if (!taken(candidate)) return candidate;
    }
    return base_name; // region already at cap with every suffix somehow taken -- fall back rather than loop forever
}

PresenceStore::JoinResult PresenceStore::join_locked(const std::string& region, const Occupant& occ,
                                                      std::string* out_display_name) {
    auto it = regions_.find(region);
    if (it != regions_.end()) {
        for (auto& o : it->second) {
            if (o.id == occ.id) {
                o = occ; // same region: replace, do not ++total_
                o.display_name = disambiguate_name_locked(region, occ.display_name, occ.id);
                if (out_display_name) *out_display_name = o.display_name;
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

    Occupant final_occ = occ;
    final_occ.display_name = disambiguate_name_locked(region, occ.display_name, occ.id);
    if (out_display_name) *out_display_name = final_occ.display_name;
    regions_[region].push_back(final_occ);
    ++total_;
    return JoinResult::Ok;
}

PresenceStore::JoinResult PresenceStore::join(const std::string& region, const Occupant& occ,
                                               std::string* out_display_name) {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    return join_locked(region, occ, out_display_name);
}

bool PresenceStore::move(const std::string& region, const std::string& id, double x, double y, int facing) {
    std::lock_guard<std::mutex> lock(mu_);
    long long now = now_ms();
    reap_locked(now);
    auto it = regions_.find(region);
    if (it == regions_.end()) return false;
    for (auto& o : it->second) {
        if (o.id == id) {
            o.x = x; o.y = y; o.facing = facing;
            o.last_update_ms = now; // must-fix: was never written, occupant JSON looked frozen
            return true;
        }
    }
    return false;
}

PresenceStore::JoinResult PresenceStore::rekey(const std::string& from_region,
                                                const std::string& to_region,
                                                const Occupant& occ,
                                                std::string* out_display_name) {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    // join_locked de-dupes globally and refuses dest-full *before* erase,
    // so a failed walk into a packed region keeps the occupant in from_region.
    // from_region is the caller's claim; the store trusts id uniqueness, not
    // that string, so a mismatched from still cannot create a second row.
    (void)from_region;
    return join_locked(to_region, occ, out_display_name);
}

void PresenceStore::leave(const std::string& region, const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
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

bool PresenceStore::update_shirt(const std::string& id, const std::string& hex) {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    for (auto& kv : regions_) {
        for (auto& o : kv.second) {
            if (o.id == id) { o.shirt_hex = hex; return true; }
        }
    }
    return false;
}

std::vector<Occupant> PresenceStore::occupants(const std::string& region) {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    auto it = regions_.find(region);
    if (it == regions_.end()) return {};
    return it->second; // copy out under the lock, caller works on the snapshot
}

std::size_t PresenceStore::global_count() {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    return total_;
}

void PresenceStore::forget(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    erase_id_locked(id);
}

std::optional<std::string> PresenceStore::rebind(const std::string& region, const std::string& old_id,
                                                  const std::string& new_id, const std::string& new_display_name) {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    auto has_old = [&]() {
        auto it = regions_.find(region);
        if (it == regions_.end()) return false;
        for (const auto& o : it->second) if (o.id == old_id) return true;
        return false;
    };
    if (!has_old()) return std::nullopt;

    // Supersede any existing row for new_id first. It can be in another region
    // (or this one); erasing invalidates references, so re-find afterwards.
    erase_id_locked(new_id);
    if (!has_old()) return std::nullopt; // cannot happen (old_id != new_id), but never index a stale iterator

    // Name is picked while the row still carries old_id, so it is excluded from
    // its own collision check.
    std::string name = disambiguate_name_locked(region, new_display_name, old_id);
    for (auto& o : regions_[region]) {
        if (o.id == old_id) {
            o.id = new_id;
            o.display_name = name;
            return name;
        }
    }
    return std::nullopt;
}

std::vector<std::string> PresenceStore::bound_usernames() {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    std::vector<std::string> out;
    for (const auto& kv : regions_) {
        for (const auto& o : kv.second) {
            if (o.id.rfind("u:", 0) == 0) out.push_back(o.id.substr(2));
        }
    }
    return out;
}

bool PresenceStore::contains(const std::string& region, const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    reap_locked(now_ms());
    auto it = regions_.find(region);
    if (it == regions_.end()) return false;
    for (const auto& o : it->second) {
        if (o.id == id) return true;
    }
    return false;
}

} // namespace commons
