#pragma once

#include <chrono>
#include <cstdint>

namespace liquidgov::platform {

inline std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace liquidgov::platform
