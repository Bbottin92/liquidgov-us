#pragma once
#include <cctype>
#include <optional>
#include <string>

// Ad-hoc extraction of a handful of known flat fields out of a small POST
// body, e.g. {"id":"g1","region":"1,1","x":10,"y":20.5,"facing":0}.
// Not a JSON parser: no nesting, no arrays, no escapes beyond \" and \\.
// That's a deliberate match to this daemon's actual inputs (presence
// join/move/leave bodies only) -- see json.hpp for the same reasoning on
// the output side.
namespace commons::reqjson {

inline std::optional<std::string> field_str(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    if (pos >= body.size() || body[pos] != '"') return std::nullopt;
    ++pos;
    std::string out;
    while (pos < body.size() && body[pos] != '"') {
        if (body[pos] == '\\' && pos + 1 < body.size()) { out += body[pos + 1]; pos += 2; }
        else { out += body[pos]; ++pos; }
    }
    return out;
}

inline std::optional<double> field_num(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    std::size_t start = pos;
    while (pos < body.size() && (std::isdigit(static_cast<unsigned char>(body[pos])) || body[pos] == '-' || body[pos] == '.')) ++pos;
    if (pos == start) return std::nullopt;
    try { return std::stod(body.substr(start, pos - start)); } catch (...) { return std::nullopt; }
}

} // namespace commons::reqjson
