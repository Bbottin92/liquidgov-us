#include "liquidgov/platform/password.hpp"

#include <sodium.h>

#include <stdexcept>

namespace liquidgov::platform {

namespace {

bool ensure_sodium_init() {
    static const bool ok = (sodium_init() >= 0);
    return ok;
}

} // namespace

std::string hash_password(const std::string& plaintext) {
    if (!ensure_sodium_init()) {
        throw std::runtime_error("libsodium init failed");
    }
    char hashed[crypto_pwhash_STRBYTES];
    // OPSLIMIT/MEMLIMIT_INTERACTIVE are libsodium's own calibrated
    // defaults, not invented here. Production tuning against the real
    // login box (target ~100ms) is a config change, not a code change.
    if (crypto_pwhash_str(hashed, plaintext.c_str(), plaintext.size(),
                           crypto_pwhash_OPSLIMIT_INTERACTIVE,
                           crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        // libsodium returns nonzero here specifically on resource
        // exhaustion (out of memory), not on bad input.
        throw std::runtime_error("password hashing failed (resource limit)");
    }
    return std::string(hashed);
}

namespace {
// The plaintext behind dummy_hash(). Google-only accounts store a hash of it as their "no password" placeholder, so it
// must never verify: before 2026-11-11 typing it signed in to any Google-only account (tests/placeholder_password_test).
const char* const kPlaceholderPhrase = "liquidgov-dummy-hash-never-a-real-account-password";
} // namespace

bool verify_password(const std::string& plaintext, const std::string& stored_hash) {
    if (!ensure_sodium_init()) {
        throw std::runtime_error("libsodium init failed");
    }
    const bool match = crypto_pwhash_str_verify(stored_hash.c_str(), plaintext.c_str(),
                                                plaintext.size()) == 0;
    // Do the real verify first either way, so the placeholder costs the same time as any other attempt.
    return match && plaintext != kPlaceholderPhrase;
}

const std::string& dummy_hash() {
    // Real argon2id hash of a fixed, never-issued value. Costs the same
    // shape of work to verify against as any real hash; never verifies (verify_password refuses the phrase).
    static const std::string value = hash_password(kPlaceholderPhrase);
    return value;
}

} // namespace liquidgov::platform
