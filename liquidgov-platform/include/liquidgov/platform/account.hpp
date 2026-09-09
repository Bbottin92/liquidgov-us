#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Platform account layer. Deliberately separate from liquidgov-core's
// CitizenId/IdentityId — a platform account may never link to a verified
// kernel identity at all. See account.verified_link.

namespace liquidgov::platform {

using AccountId = std::uint64_t;

struct PlatformAccount {
    AccountId id = 0;
    std::string username;

    // argon2id, produced by hash_password() in password.hpp. Never
    // plaintext, never a fast hash alone.
    std::string password_hash;

    // Required before account creation (moved up from gating profile
    // creation specifically, per the settled design) but is a login
    // *binding*, not the daily credential. See auth.hpp.
    bool phone_verified = false;

    // Absent until passkey support is layered in later — additive, not
    // a redesign of this struct when it lands.
    std::optional<std::string> passkey_credential;

    // Pointer into the kernel's identity/citizenship layer. This is a
    // pointer only: this struct must never gain a field that writes
    // CitizenId, GrantCitizenship, or anything else kernel-side.
    std::optional<std::uint64_t> verified_kernel_identity;

    std::int64_t created_at = 0; // unix seconds
};

} // namespace liquidgov::platform
