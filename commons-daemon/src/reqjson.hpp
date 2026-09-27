#pragma once
#include <cctype>
#include <optional>
#include <string>
#include <vector>

// Extraction of a handful of known fields out of a small flat POST body, e.g.
// {"id":"g1","region":"1,1","x":10,"y":20.5,"facing":0}.
//
// Not a full JSON parser (no \uXXXX decoding, no numbers-as-exponent policing
// beyond what strtod accepts), but it does walk the object the way JSON reads
// it: key, ':', value, ',' -- in order. A field is found only where a KEY can
// be, never by matching a quoted token anywhere in the text. The earlier
// version searched for the text "key" and so matched a VALUE equal to a field
// name first (a guest named x: {"display_name":"x","x":5} read the name as the
// x coordinate's key). Nested objects/arrays are skipped whole (their inner keys
// are never fields of this body); the first top-level occurrence of a key wins.
// See json.hpp for the same "small and specific" reasoning on the output side.
namespace commons::reqjson {

namespace detail {

inline void skip_ws(const std::string& b, std::size_t& p) {
    while (p < b.size() && std::isspace(static_cast<unsigned char>(b[p]))) ++p;
}

// p is at an opening '"'. Reads to the closing '"' (\" and \\ and any \c yield c),
// leaves p just past it. False if the string is never closed.
inline bool read_string(const std::string& b, std::size_t& p, std::string& out) {
    out.clear();
    ++p;
    while (p < b.size()) {
        char c = b[p];
        if (c == '"') { ++p; return true; }
        if (c == '\\' && p + 1 < b.size()) { out += b[p + 1]; p += 2; }
        else { out += c; ++p; }
    }
    return false;
}

// p is at '{' or '['. Skips the whole balanced value, ignoring brackets inside strings.
inline bool skip_composite(const std::string& b, std::size_t& p) {
    int depth = 0;
    while (p < b.size()) {
        char c = b[p];
        if (c == '"') { std::string s; if (!read_string(b, p, s)) return false; continue; }
        if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') { if (--depth == 0) { ++p; return true; } }
        ++p;
    }
    return false;
}

struct Value {
    bool is_string = false;
    std::string text;   // the unescaped string, or the raw token for numbers/literals ("" for nested values)
    bool nested = false;
};

// The value of the first top-level `key` in a flat object, or nullopt if it is not
// there or the text is not a well-formed object up to that point.
inline std::optional<Value> find_value(const std::string& b, const std::string& key) {
    std::size_t p = 0;
    skip_ws(b, p);
    if (p >= b.size() || b[p] != '{') return std::nullopt;
    ++p;
    while (true) {
        skip_ws(b, p);
        if (p >= b.size() || b[p] != '"') return std::nullopt;   // '}' (no more keys) or garbage
        std::string k;
        if (!read_string(b, p, k)) return std::nullopt;
        skip_ws(b, p);
        if (p >= b.size() || b[p] != ':') return std::nullopt;
        ++p;
        skip_ws(b, p);
        if (p >= b.size()) return std::nullopt;

        Value v;
        if (b[p] == '"') {
            v.is_string = true;
            if (!read_string(b, p, v.text)) return std::nullopt;
        } else if (b[p] == '{' || b[p] == '[') {
            v.nested = true;
            if (!skip_composite(b, p)) return std::nullopt;
        } else {
            std::size_t start = p;
            while (p < b.size() && b[p] != ',' && b[p] != '}' && b[p] != ']' &&
                   !std::isspace(static_cast<unsigned char>(b[p]))) ++p;
            v.text = b.substr(start, p - start);
        }
        if (k == key) return v;

        skip_ws(b, p);
        if (p < b.size() && b[p] == ',') { ++p; continue; }
        return std::nullopt;   // '}' (key not present) or malformed
    }
}

} // namespace detail

inline std::optional<std::string> field_str(const std::string& body, const std::string& key) {
    auto v = detail::find_value(body, key);
    if (!v || !v->is_string) return std::nullopt;
    return v->text;
}

// BUILD studio 1: the side-mix endpoint's {"slugs":["a","b"]} -- a flat array of strings, nothing
// nested. nullopt if the key is missing or is not an array at all (a string/number/object there is
// a caller error, not "zero slugs"); an array containing anything but a string, or more than
// max_len entries, also yields nullopt (the caller rejects the whole request, never silently drops
// entries -- a script probing for a discovery must not get a different, "helpfully truncated" list
// back). An empty array [] is a valid, empty result.
inline std::optional<std::vector<std::string>> field_str_array(const std::string& body, const std::string& key,
                                                                 std::size_t max_len) {
    std::size_t p = 0;
    detail::skip_ws(body, p);
    if (p >= body.size() || body[p] != '{') return std::nullopt;
    ++p;
    while (true) {
        detail::skip_ws(body, p);
        if (p >= body.size() || body[p] != '"') return std::nullopt;
        std::string k;
        if (!detail::read_string(body, p, k)) return std::nullopt;
        detail::skip_ws(body, p);
        if (p >= body.size() || body[p] != ':') return std::nullopt;
        ++p;
        detail::skip_ws(body, p);
        if (p >= body.size()) return std::nullopt;

        if (k == key) {
            if (body[p] != '[') return std::nullopt;
            ++p;
            std::vector<std::string> out;
            detail::skip_ws(body, p);
            if (p < body.size() && body[p] == ']') { ++p; return out; } // []
            while (true) {
                detail::skip_ws(body, p);
                if (p >= body.size() || body[p] != '"') return std::nullopt; // a non-string element
                std::string s;
                if (!detail::read_string(body, p, s)) return std::nullopt;
                if (out.size() >= max_len) return std::nullopt; // too many, whole request rejected
                out.push_back(std::move(s));
                detail::skip_ws(body, p);
                if (p < body.size() && body[p] == ',') { ++p; continue; }
                if (p < body.size() && body[p] == ']') { ++p; return out; }
                return std::nullopt;
            }
        }

        // Not the key we want: skip this value (string, nested, or a bare token) and move on.
        if (body[p] == '"') { std::string s; if (!detail::read_string(body, p, s)) return std::nullopt; }
        else if (body[p] == '{' || body[p] == '[') { if (!detail::skip_composite(body, p)) return std::nullopt; }
        else { while (p < body.size() && body[p] != ',' && body[p] != '}') ++p; }
        detail::skip_ws(body, p);
        if (p < body.size() && body[p] == ',') { ++p; continue; }
        return std::nullopt; // key not present, or malformed
    }
}

inline std::optional<double> field_num(const std::string& body, const std::string& key) {
    auto v = detail::find_value(body, key);
    if (!v || v->is_string || v->nested || v->text.empty()) return std::nullopt;
    const std::string& t = v->text;
    // a JSON number starts with '-' or a digit; anything else (true, null, +5, .5, nan) is not one
    if (!(t[0] == '-' || std::isdigit(static_cast<unsigned char>(t[0])))) return std::nullopt;
    for (char c : t)   // strtod would also take hex floats ("0x10"), "inf", "nan": only JSON-number characters
        if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
            return std::nullopt;
    try {
        std::size_t used = 0;
        double d = std::stod(t, &used);
        if (used != t.size()) return std::nullopt;   // trailing junk: not a number
        return d;
    } catch (...) { return std::nullopt; }
}

} // namespace commons::reqjson
