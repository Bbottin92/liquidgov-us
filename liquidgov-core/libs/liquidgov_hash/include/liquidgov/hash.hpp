#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace liquidgov {

using Hash = std::array<std::uint8_t, 32>;

inline Hash zero_hash() { return {}; }

std::string hex(const Hash& h);
Hash sha256(const void* data, std::size_t len);
inline Hash sha256(std::string_view s) { return sha256(s.data(), s.size()); }

} // namespace liquidgov
