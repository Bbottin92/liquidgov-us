#pragma once
// map_search.hpp: the map's search bar (Brandon 2026-11-15: "a search bar at the top where you can search for stuff",
// then "it should also search for matching word patterns in like posts or comments or messages", then "also date's and
// maybe even times"). GET /map/search?q=<words>&tz=<minutes east of UTC> answers {"results":[...]}.
#include "site/common.hpp"

namespace liquidgov::site {

// What a search asks for: the words (lowercased; all must be in a match, in any order) and, if the search named a day,
// month, year or time, when (unix seconds, [from, to)) and/or a time of day (minutes after local midnight, [m0, m1)).
struct MapSearchQuery {
    std::vector<std::string> words;
    bool has_when = false;
    std::int64_t from = 0, to = 0;
    bool has_clock = false;
    int m0 = 0, m1 = 0;
    int tz_min = 0;   // the searcher's offset from UTC, minutes east
};
// now: unix seconds. Date and time words are taken out of the words: today, yesterday, monday..sunday, a month with or
// without a day and year (nov 12, 12 november 2026, november), 2026-11-12, 11/12, 11/12/26, a year (2026), and times
// (3pm, 3:15pm, 15:00, 3 pm, noon, midnight).
MapSearchQuery map_search_parse(const std::string& raw, std::int64_t now, int tz_min);
// Does a moment match the query's when and time of day?
bool map_search_when_ok(const MapSearchQuery& q, std::int64_t ts);
// The JSON body for a viewer's search (at most 20 results).
std::string map_search_json(const std::string& viewer, const std::string& raw, int tz_min);

}  // namespace liquidgov::site
