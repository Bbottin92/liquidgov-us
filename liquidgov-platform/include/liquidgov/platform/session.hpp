#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "liquidgov/platform/account.hpp"

namespace liquidgov::platform {

using SessionId = std::string; // opaque random token, never sequential

struct Session {
    SessionId id;
    AccountId account = 0;
    std::int64_t created_at = 0;
    std::int64_t last_seen = 0;
    std::int64_t expires_at = 0; // absolute lifetime cap
    bool revoked = false;
    std::string device_label;

    // WebSocket connection ids currently live on this session. A set,
    // not a bool: two tabs on one session must not double-count or drop
    // presence when only one closes. Presence for an account is an OR
    // across every session's set, not tied to an unexpired cookie alone.
    std::set<std::string> live_connection_ids;

    bool is_live() const { return !revoked && !live_connection_ids.empty(); }
};

// Cryptographically random (libsodium randombytes_buf), hex-encoded.
// Never a counter, never derived from account id or time.
SessionId generate_session_id();

// Rotate on login and on privilege step-up — returns a fresh id, does
// not mutate the session's other fields.
SessionId rotate_session_id(Session& session);

} // namespace liquidgov::platform
