// codec: the LGP1 field encoding shared by every record module (see codec.cpp).
// Internal to liquidgov_file_store (src/, not include/): the site never sees these names.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace liquidgov::platform::store_detail {

std::string pct_encode(const std::string& s);
std::string pct_decode(const std::string& s);
std::vector<std::string> split_tab(const std::string& line);
std::uint64_t parse_u64(const std::string& s, const char* what);
std::int64_t parse_i64(const std::string& s, const char* what);
bool parse_bit(const std::string& s, const char* what);
double parse_dbl(const std::string& s, const char* what);
std::string fmt_stored(double v);
std::string lower_copy(std::string s);

} // namespace liquidgov::platform::store_detail
