#pragma once

#include <string>

namespace liquidgov::platform {

// Argon2id via libsodium's high-level API. Never plaintext, never a fast
// hash alone. OPSLIMIT/MEMLIMIT are calibration knobs (see password.cpp) —
// belong in config once this runs on a real login box, not hardcoded
// forever as a code comment.
std::string hash_password(const std::string& plaintext);
bool verify_password(const std::string& plaintext, const std::string& stored_hash);

// A stable, validly-formatted argon2id hash of a fixed, never-issued
// value. Verifying against this always fails, but costs the same shape
// of work as verifying a real hash. login() in auth.cpp always calls one
// of hash_password/verify_password — real account or not — so the
// unknown-username path and the wrong-password path are indistinguishable
// by response time. Returning early on an unknown username is the timing
// leak this exists to close, not a shortcut this replaces.
const std::string& dummy_hash();

// Left abstract deliberately: a real deployment plugs in a k-anonymity
// style breach-list check (e.g. an HIBP range-query client) here. This
// module does not fake that with a local stub that looks like a real
// check — an always-false implementation is a lie, not a placeholder.
class BreachChecker {
public:
    virtual ~BreachChecker() = default;
    virtual bool is_breached(const std::string& plaintext_password) const = 0;
};

} // namespace liquidgov::platform
