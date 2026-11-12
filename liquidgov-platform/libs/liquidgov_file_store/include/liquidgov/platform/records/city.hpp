#pragma once
// records/city.hpp: the city record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

// Tidied city text as stored: control characters dropped, runs of
// whitespace collapsed to one space, trimmed, cut to 80 bytes (never in the
// middle of a UTF-8 character). Empty when nothing usable is left.
std::string clean_city(const std::string& raw);

// The key cities are grouped and linked by: ASCII letters lowercased,
// every run of other ASCII characters (spaces, commas, dots...) turned into
// one '-', leading/trailing '-' dropped; non-ASCII bytes kept as they are.
// "Fresno, CA" and "fresno ca" both give "fresno-ca". Empty when the city
// has no letters or digits. No geocoding and no official list.
std::string city_key(const std::string& city);

// What one person lets the map (/map) show about them. Everything that was
// already public is on by default (Brandon: "I want people to network, not
// isolate"). Friendships started off on 2026-11-06 (they had never been
// public) and turned on by default on 2026-11-10 (Brandon: "im trying to
// bring people together not separate them"); a friendship shows to others
// only while BOTH friends leave it on. You always see your own. Stored in
// the mapp tag.
struct MapPrefs {
    bool show = true;      // appear on the map at all
    bool groups = true;    // lines to the groups you are in
    bool posts = true;     // your posts as boxes
    bool comments = true;  // lines to proposals you commented on
    // lines to your friends (both must agree). ON by default since 2026-11-10 (Brandon: "im trying to bring people
    // together not separate them"); people who had saved Map settings keep what they saved (their mapp record).
    bool friends = true;
    bool pains = true;     // notice wishes and pain points in your new public words
    // Fields a NEWER version wrote after these, kept verbatim and written back,
    // so rolling back one version never drops (or breaks on) a newer setting.
    // Any new field must default to the privacy-safe value when absent.
    std::vector<std::string> extra_fields;
};

struct CityUse {
    std::string key;
    std::string name;   // the spelling most of its groups use (ties: first A-Z)
    std::size_t groups = 0;
};

}  // namespace liquidgov::platform
