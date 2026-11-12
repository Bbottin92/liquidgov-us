#pragma once
// records/accounts.hpp: the accounts record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
// moved, not rewritten). The LGP1 format of every record type stays in file_store.hpp's header comment.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

inline constexpr std::int64_t kPhoneQuarantineSeconds = 90 * 24 * 60 * 60;

// One outstanding password-reset token -- see the "pr" LGP1 tag comment.
struct ResetToken {
    std::string token_hash; // sha256_hex of the raw token; the raw token is never stored
    std::int64_t until_unix = 0;
};

bool valid_username(const std::string& username);

std::string fingerprint_phone(const std::string& e164);

}  // namespace liquidgov::platform
