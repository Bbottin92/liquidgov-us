// codec: the LGP1 field encoding shared by every record module (percent-encoding, tab split, number
// parsing, the stored number format). Split out of file_store.cpp 2026-11-12 (code moved, not rewritten).
#include "core/codec.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace liquidgov::platform::store_detail {

namespace {

int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

} // namespace

std::string pct_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    o.reserve(s.size());
    for (unsigned char c : s) {
        if (c == '%' || c == '\t' || c == '\n' || c == '\r') {
            o.push_back('%');
            o.push_back(hex[c >> 4]);
            o.push_back(hex[c & 0x0f]);
        } else {
            o.push_back(static_cast<char>(c));
        }
    }
    return o;
}

std::string pct_decode(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%') {
            if (i + 2 >= s.size()) throw std::runtime_error("LGP1: truncated percent escape");
            const int hi = hex_val(s[i + 1]);
            const int lo = hex_val(s[i + 2]);
            if (hi < 0 || lo < 0) throw std::runtime_error("LGP1: bad percent escape");
            o.push_back(static_cast<char>((hi << 4) | lo));
            i += 2;
        } else {
            o.push_back(s[i]);
        }
    }
    return o;
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> f;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            f.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    f.push_back(cur);
    return f;
}

std::uint64_t parse_u64(const std::string& s, const char* what) {
    if (s.empty()) throw std::runtime_error(std::string("LGP1: empty ") + what);
    std::size_t idx = 0;
    std::uint64_t v = 0;
    try {
        v = std::stoull(s, &idx, 10);
    } catch (...) {
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    }
    if (idx != s.size()) throw std::runtime_error(std::string("LGP1: trailing junk in ") + what);
    return v;
}

std::int64_t parse_i64(const std::string& s, const char* what) {
    if (s.empty()) throw std::runtime_error(std::string("LGP1: empty ") + what);
    std::size_t idx = 0;
    std::int64_t v = 0;
    try {
        v = std::stoll(s, &idx, 10);
    } catch (...) {
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    }
    if (idx != s.size()) throw std::runtime_error(std::string("LGP1: trailing junk in ") + what);
    return v;
}

bool parse_bit(const std::string& s, const char* what) {
    const auto v = parse_u64(s, what);
    if (v > 1) throw std::runtime_error(std::string("LGP1: bad ") + what);
    return v == 1;
}

double parse_dbl(const std::string& s, const char* what) {
    if (s.empty()) throw std::runtime_error(std::string("LGP1: empty ") + what);
    std::size_t idx = 0;
    double v = 0;
    try {
        v = std::stod(s, &idx);
    } catch (...) {
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    }
    if (idx != s.size() || !std::isfinite(v))
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    return v;
}

std::string fmt_stored(double v) {
    std::ostringstream o;
    o.precision(12);
    o << v;
    return o.str();
}

std::string lower_copy(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

} // namespace liquidgov::platform::store_detail
