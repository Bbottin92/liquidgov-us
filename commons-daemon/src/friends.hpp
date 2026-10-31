#pragma once
#include <optional>
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

    // Friends of this cookie's account, from cache if fresh, otherwise fetched
    // from civic (commons::civic::resolve_friends) and remembered. A request with
    // no lg_session cookie is a guest: an EMPTY list (a real answer, never
    // cached, so anonymous traffic can't churn the map).
    //
    // nullopt = civic gave NO answer (down, slow, unreadable) and there is no
    // fresh cached list. That is never stored -- not as `{}`, not as anything --
    // and any expired entry is left exactly as it was. (BUILD hardening: this used
    // to cache a timeout as "no friends" for the whole TTL, which darkened the
    // lots and walked friends-policy visitors out on their next move.) Callers
    // must treat nullopt as "don't know", not as "no friends".
    std::optional<std::vector<std::string>> get(const std::string& headers_raw);

private:
    struct Entry {
        std::vector<std::string> friends;
        long long fetched_ms = 0;
    };
    std::unordered_map<std::string, Entry> cache_;
};

} // namespace commons
