#include "friends.hpp"

#include <chrono>

#include "civic.hpp"

namespace commons {
namespace {
long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
} // namespace

std::vector<std::string> FriendsCache::get(const std::string& headers_raw) {
    if (!civic::has_session_cookie(headers_raw)) return {};

    const long long now = now_ms();
    auto it = cache_.find(headers_raw);
    if (it != cache_.end() && now - it->second.fetched_ms < kTtlMs) return it->second.friends;

    // Drop expired entries before growing -- and, if the map is still at
    // its cap afterwards, drop it wholesale rather than evicting one by
    // one: at Square scale this is a handful of entries, and a full reset
    // just costs each session one extra civic call.
    if (cache_.size() >= kCap) {
        for (auto e = cache_.begin(); e != cache_.end();) {
            if (now - e->second.fetched_ms >= kTtlMs) e = cache_.erase(e);
            else ++e;
        }
        if (cache_.size() >= kCap) cache_.clear();
    }

    Entry fresh;
    fresh.friends = civic::resolve_friends(headers_raw);
    fresh.fetched_ms = now;
    cache_[headers_raw] = fresh;
    return fresh.friends;
}

} // namespace commons
