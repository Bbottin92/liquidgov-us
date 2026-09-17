#include "guest.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>

namespace commons {

namespace {

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// std::random_device on Linux reads from the OS entropy pool -- good
// enough for an unguessable per-session guest token; this is not a
// long-term account credential. Single-threaded daemon (http.hpp), so a
// function-local static generator needs no locking of its own.
std::string random_hex(std::size_t nbytes) {
    static std::random_device rd;
    static std::mt19937_64 rng((static_cast<std::uint64_t>(rd()) << 32) | rd());
    std::uniform_int_distribution<int> byte(0, 255);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < nbytes; ++i) out << std::setw(2) << byte(rng);
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
        if (now - it->second.last_used_ms > kIdleMs) it = entries_.erase(it);
        else ++it;
    }
}

GuestRegistry::Credential GuestRegistry::mint() {
    std::lock_guard<std::mutex> lock(mu_);
    long long now = now_ms();
    reap_locked(now);
    std::string id = "g_" + random_hex(8);   // 16 hex chars, public label
    std::string secret = random_hex(16);     // 32 hex chars, private credential
    entries_[id] = Entry{secret, now};
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
    return true;
}

} // namespace commons
