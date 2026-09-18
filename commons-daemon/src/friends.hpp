#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace commons {

// glue.md "Friends query": "Game-side caching: short RAM TTL (seconds, not
// persisted)... never written into schema.sql. There is no friends table in
// this project's schema and there must never be one." This is that cache
// and nothing more -- a per-session copy of civic's answer that expires on
// its own, so polling the lot doors never turns into one civic round trip
// per poll.
//
// Keyed by the forwarded Cookie header itself (RAM only, never logged or
// persisted), not a username: the username is only known *after* asking
// civic, and keying by the session is what makes "friends of *that*
// session" hold for the whole TTL. A revoked session can therefore linger
// for at most kTtlMs -- the same order of staleness the 5s client poll
// already has.
//
// No mutex: single-threaded daemon (http.hpp), same reasoning as HouseStore.
class FriendsCache {
public:
    static constexpr long long kTtlMs = 15000;
    static constexpr std::size_t kCap = 256;

    // Friends of this cookie's account, from cache if fresh, otherwise
    // fetched from civic (commons::civic::resolve_friends) and remembered.
    // Empty for a request with no lg_session cookie -- guests are never
    // cached, so anonymous traffic can't churn the map.
    std::vector<std::string> get(const std::string& headers_raw);

private:
    struct Entry {
        std::vector<std::string> friends;
        long long fetched_ms = 0;
    };
    std::unordered_map<std::string, Entry> cache_;
};

} // namespace commons
