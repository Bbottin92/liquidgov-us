#pragma once
// records/admin.hpp: the admin record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

// An operator's permission for one person to send programs (archives,
// installers, executables -- ProgramKind in file_type.hpp) in mail: which
// kinds and how big. Operators themselves need no grant (is_operator in
// http_server.cpp gives them every kind up to the server ceiling).
struct ProgramGrant {
    std::string username;
    std::uint32_t kinds = 0;      // bitmask of ProgramKind
    std::uint64_t max_bytes = 0;
    std::string granted_by;
    std::int64_t granted_at = 0;
};

// One operator-panel action -- see the "alog" LGP1 tag's own comment for
// what action/target hold.
struct AuditLogEntry {
    std::int64_t ts = 0;
    std::string operator_name;
    std::string action;
    std::string target;
};

inline constexpr std::size_t kAuditLogCap = 500;

}  // namespace liquidgov::platform
