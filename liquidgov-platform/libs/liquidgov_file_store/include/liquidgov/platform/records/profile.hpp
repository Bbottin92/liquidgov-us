#pragma once
// records/profile.hpp: the profile record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

// Public profile extras (not login). email_fp is the same blake2 as phone —
// the address itself is not stored. display_name is shown on /u/<user>.
// bio lives in the existing blurb field.
struct Profile {
    std::string display_name;
    std::string email_fp;
    std::string hometown;
    std::string living;
    std::string interests;
    std::string career;
    std::string school;
};

}  // namespace liquidgov::platform
