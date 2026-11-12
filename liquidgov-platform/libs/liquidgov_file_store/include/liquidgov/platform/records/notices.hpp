#pragma once
// records/notices.hpp: the notices record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

struct Notice {
    std::uint64_t id = 0;
    std::string to;
    std::string type; // friend_request | friend_accept | message | group_invite
    std::string from;
    std::int64_t ts = 0;
    bool read = false;
    std::string extra;
};

}  // namespace liquidgov::platform
