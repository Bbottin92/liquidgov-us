#pragma once
#include <cstdio>
#include <string>

// Deliberately not a real JSON library -- this daemon's responses are
// small, fixed-shape, and hand-built the same way civic hand-builds HTML
// (see http_server.cpp's html_escape). A real parser/serializer is not
// worth a dependency for a handful of flat objects and arrays.
namespace commons::json {

inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

inline std::string str(const std::string& s) { return "\"" + escape(s) + "\""; }

} // namespace commons::json
