#include "liquidgov/platform/session.hpp"

#include <sodium.h>

#include <array>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

namespace {

bool ensure_sodium_init() {
    static const bool ok = (sodium_init() >= 0);
    return ok;
}

constexpr std::size_t kSessionIdBytes = 32; // 256 bits of entropy

std::string random_hex_token(std::size_t n_bytes) {
    if (!ensure_sodium_init()) {
        throw std::runtime_error("libsodium init failed");
    }
    std::vector<unsigned char> buf(n_bytes);
    randombytes_buf(buf.data(), buf.size());

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(n_bytes * 2);
    for (unsigned char b : buf) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0f]);
    }
    return out;
}

} // namespace

SessionId generate_session_id() {
    return random_hex_token(kSessionIdBytes);
}

SessionId rotate_session_id(Session& session) {
    session.id = generate_session_id();
    return session.id;
}

} // namespace liquidgov::platform
