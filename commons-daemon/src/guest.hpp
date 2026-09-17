#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

namespace commons {

// Server-issued guest credentials -- closes GROK_DAEMON_REVIEW.md's
// should-fix #3: "client-chosen id + display_name is impersonation once
// nginx is in front." `id` is the public occupant label (broadcast in
// every /presence/<region> GET so other clients can render/track it);
// `secret` is a private bearer credential returned only once, at mint
// time, and never echoed back by any GET. join/move/rekey/leave all
// require a matching (id, secret) pair -- without this, any client that
// merely *observes* another occupant's public id (which it must, to
// render them) could replay it to hijack that occupant's row.
class GuestRegistry {
public:
    struct Credential { std::string id; std::string secret; };

    // Generates a fresh, unguessable (id, secret) pair and remembers it.
    Credential mint();

    // True iff `id` was minted here and `secret` matches (constant-time
    // compare -- this is a bearer credential, not a display value). On
    // success, refreshes the entry's last-used time so an active guest's
    // credential never expires out from under it while they keep calling
    // in; mirrors PresenceStore's own idle-reap window (kIdleMs).
    bool verify(const std::string& id, const std::string& secret);

private:
    static constexpr long long kIdleMs = 30000;

    struct Entry { std::string secret; long long last_used_ms; };
    std::mutex mu_;
    std::unordered_map<std::string, Entry> entries_; // guarded by mu_

    void reap_locked(long long now); // callers must hold mu_
};

} // namespace commons
