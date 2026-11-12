#pragma once
// records/social.hpp: the social record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

inline constexpr int kInnerCircleCap = 12;

enum class Findability : int {
    FriendsOnly = 0,
    ExactMatch = 1,
    Typeahead = 2,
};

// One block, with the context a settings page actually wants to show back
// -- when, and why (freeform, actor's own words, never shown to the
// blocked party). is_blocked/blocked_by stay the cheap boolean/list lookup
// they always were; this is the richer read for rendering the block list.
struct BlockEntry {
    std::string actor;
    std::string other;
    std::int64_t at = 0;
    std::string reason;
};

}  // namespace liquidgov::platform
