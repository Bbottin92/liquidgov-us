#include "guest.hpp"

#include <sys/random.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace commons {

namespace {

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Every byte straight from the kernel (getrandom), no user-space generator in
// between. This used to be a Mersenne Twister seeded with 64 bits: its output
// is not a secret once enough consecutive values are known, and a client can
// mint its own credentials to collect them. The guest secret is a bearer
// credential, so it must not be predictable from anyone's ids or secrets.
std::string random_hex(std::size_t nbytes) {
    unsigned char buf[64];
    if (nbytes > sizeof(buf)) throw std::runtime_error("random_hex: request too large");
    std::size_t got = 0;
    while (got < nbytes) {
        ssize_t r = getrandom(buf + got, nbytes - got, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("getrandom failed");
        }
        got += static_cast<std::size_t>(r);
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < nbytes; ++i) out << std::setw(2) << static_cast<int>(buf[i]);
    return out.str();
}

// Constant-time compare -- secret is a bearer credential, not a display
// value, so an early-exit != would leak how many leading bytes matched.
bool secure_eq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

} // namespace

void GuestRegistry::reap_locked(long long now) {
    for (auto it = entries_.begin(); it != entries_.end(); ) {
        const long long limit = it->second.used ? kIdleMs : kUnusedIdleMs;
        if (now - it->second.last_used_ms > limit) it = entries_.erase(it);
        else ++it;
    }
}

GuestRegistry::Credential GuestRegistry::mint() {
    std::lock_guard<std::mutex> lock(mu_);
    long long now = now_ms();
    reap_locked(now);
    // Draw first: if the OS entropy source ever fails this throws before the
    // registry is touched.
    std::string id = "g_" + random_hex(8);   // 16 hex chars, public label
    std::string secret = random_hex(16);     // 32 hex chars, private credential
    // Full after reaping: evict rather than refuse. Never-used junk goes first
    // (oldest first), so a flood of mints only ever evicts other junk; only if
    // every entry is a real, used guest is the least recently used one dropped,
    // and that guest simply re-mints on its next 403, the same path a daemon
    // restart already exercises.
    while (entries_.size() >= kMaxEntries) {
        auto victim = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (victim == entries_.end()) { victim = it; continue; }
            const bool it_unused = !it->second.used, v_unused = !victim->second.used;
            if (it_unused != v_unused) { if (it_unused) victim = it; continue; }
            if (it->second.last_used_ms < victim->second.last_used_ms) victim = it;
        }
        if (victim == entries_.end()) break;
        entries_.erase(victim);
    }
    entries_[id] = Entry{secret, now, false};
    return {id, secret};
}

bool GuestRegistry::verify(const std::string& id, const std::string& secret) {
    std::lock_guard<std::mutex> lock(mu_);
    long long now = now_ms();
    reap_locked(now);
    auto it = entries_.find(id);
    if (it == entries_.end()) return false;
    if (!secure_eq(it->second.secret, secret)) return false;
    it->second.last_used_ms = now;
    it->second.used = true;
    return true;
}

} // namespace commons
