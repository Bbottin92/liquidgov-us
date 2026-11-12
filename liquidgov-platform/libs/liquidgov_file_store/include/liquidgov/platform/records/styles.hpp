#pragma once
// records/styles.hpp: the styles record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

// A site-wide, unofficial straw poll -- explicitly not a binding vote (the
// genesis-phase copy is clear real governmental authority is a later,
// separate landslide). Any member can open one and any member can cast one
// vote on it, changeable, to gauge sentiment on something not yet decided.
// A published profile style (sty): see the tag's comment above.
struct ProfileStyle {
    std::uint64_t id = 0;
    std::string author;
    std::int64_t created_at = 0;
    std::string name;
    std::string css;
};

// One report of a style (str).
struct StyleReport {
    std::uint64_t style_id = 0;
    std::string reporter;
    std::int64_t at = 0;
    std::string reason;
};

}  // namespace liquidgov::platform
