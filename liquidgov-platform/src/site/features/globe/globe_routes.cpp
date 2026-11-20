// The map globes (Brandon 2026-11-20): "there should be a globe in the middle of the friends pool, the events pool, the
// market pool, and one in the middle of the social pool. when you walk up to that pool's globe you can select it and it
// opens a fullscreen map page with a stacked list". His answers: a real street map (OpenStreetMap), all four pools,
// the list nearest first. /globe/<kind> is that page: a street map (assets/leaflet.js, drawn by assets/globe.js) and the
// stacked list under it (beside it on a wide screen). G1: Friends has its list; Events, Market and Social show their
// empty states until they have contents (G2-G4 in Persistence/notes.md).
#include "site/features/globe/globe_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/outside/outside.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/wrap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace liquidgov::site {

namespace {

double miles_between(double lat1, double lon1, double lat2, double lon2) {
    const double r = 3958.8, k = 3.14159265358979323846 / 180.0;
    const double dlat = (lat2 - lat1) * k, dlon = (lon2 - lon1) * k;
    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(lat1 * k) * std::cos(lat2 * k) * std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2 * r * std::asin(std::min(1.0, std::sqrt(a)));
}

std::string num(double v) { char b[32]; std::snprintf(b, sizeof b, "%.5f", v); return b; }

struct Row {
    std::string name, label, city, href;
    bool placed = false; double lat = 0, lon = 0, miles = -1;
};

}  // namespace

std::optional<std::string> globe_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if (method != "GET" || path.rfind("/globe/", 0) != 0) return std::nullopt;
    if (!acc) return need_login();
    const std::string kind = path.substr(7);
    const char* title = kind == "friends" ? "Friends" : kind == "events" ? "Events" : kind == "market" ? "Market"
                      : kind == "social" ? "Social" : nullptr;
    if (!title)
        return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such globe.</p></div>"));

    // Where you are: your own profile's "Where you live now". Up to 3 cities not yet known are looked up per visit
    // (OpenStreetMap allows one request a second); the rest show as not placed yet and are placed on a later visit.
    int fetches = 3;
    auto point = [&](const std::string& city, double& la, double& lo) {
        if (city.empty()) return false;
        if (outside_city_point(city, la, lo, false)) return true;
        if (fetches <= 0) return false;
        --fetches;
        return outside_city_point(city, la, lo, true);
    };
    const std::string my_city = g_store.profile_of(user).living;
    double my_lat = 0, my_lon = 0;
    const bool me_placed = point(my_city, my_lat, my_lon);

    std::vector<Row> rows;
    if (kind == "friends") {
        for (const auto& f : g_store.friends_of(user)) {
            if (g_store.is_blocked(user, f) || g_store.is_blocked(f, user)) continue;
            Row row;
            row.name = f;
            const auto prof = g_store.profile_of(f);
            row.label = prof.display_name.empty() ? f : prof.display_name;
            row.href = "/u/" + url_encode(f);
            // a friend's city only when their profile is visible to a friend (the same rule as their profile page)
            if (visible_to(g_store.prefs_of(f).profile_visibility, false, true, true)) row.city = prof.living;
            row.placed = point(row.city, row.lat, row.lon);
            if (row.placed && me_placed) row.miles = miles_between(my_lat, my_lon, row.lat, row.lon);
            rows.push_back(row);
        }
        // nearest first; then placed but no distance (you have no city), then not placed; each part by name
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
            const int ra = a.miles >= 0 ? 0 : a.placed ? 1 : 2, rb = b.miles >= 0 ? 0 : b.placed ? 1 : 2;
            if (ra != rb) return ra < rb;
            if (ra == 0 && a.miles != b.miles) return a.miles < b.miles;
            return a.label < b.label;
        });
    }

    // the points for the map, as JSON in a data attribute (html_escape keeps it inside the attribute)
    std::string pts = "[";
    for (const auto& row : rows) {
        if (!row.placed) continue;
        std::string nm;
        for (char c : row.label) { if (c == '"' || c == '\\') nm += '\\'; if (static_cast<unsigned char>(c) >= 0x20) nm += c; }
        pts += std::string(pts.size() > 1 ? "," : "") + "{\"lat\":" + num(row.lat) + ",\"lon\":" + num(row.lon) +
               ",\"name\":\"" + nm + "\",\"href\":\"" + row.href + "\"}";
    }
    pts += "]";

    std::string list;
    for (const auto& row : rows) {
        std::string where = row.city.empty() ? "Where they live is not shared" : html_escape(row.city);
        if (row.miles >= 0) where += row.miles < 1 ? " &middot; in your city" : " &middot; about " + std::to_string(static_cast<long>(std::lround(row.miles))) + " mi";
        else if (!row.city.empty() && !row.placed) where += " &middot; not on the map yet";
        list += "<li class=\"globerow\" data-name=\"" + html_escape(row.name) + "\">" + portrait_tag(row.name, "sm") +
                "<div class=\"globerow-text\"><a href=\"" + row.href + "\"><strong>" + html_escape(row.label) + "</strong></a>"
                "<span class=\"muted\">" + where + "</span></div>"
                "<a class=\"btn ghost\" href=\"/mail?with=" + html_escape(row.name) + "\">Message</a></li>";
    }
    std::string empty;
    if (rows.empty()) {
        if (kind == "friends") empty = "No friends yet. <a href=\"/friends\">Find people</a> and they show here, nearest first.";
        else if (kind == "events") empty = "No events yet. Events near you will show here, soonest and nearest first.";
        else if (kind == "market") empty = "Nobody is offering anything here yet. People and shops near you will show here, nearest first.";
        else empty = "Nothing here yet. Places and get-togethers near you will show here, nearest first.";
    }

    std::string html =
        "<link rel=\"stylesheet\" href=\"" + asset_url("leaflet.css") + "\">"
        "<div class=\"globepage\" data-kind=\"" + kind + "\">"
        "<div class=\"globehead\"><h1>" + std::string(title) + "</h1>"
        "<p class=\"muted\">" + (me_placed ? "Around " + html_escape(my_city) + "."
            : my_city.empty() ? std::string("Add where you live on <a href=\"/me\">your profile</a> to see what is nearest.")
            : "Finding " + html_escape(my_city) + " on the map; look again in a moment.") + "</p></div>"
        "<div class=\"globewrap\">"
        "<div id=\"globemap\" class=\"globemap\" role=\"img\" aria-label=\"Street map\"" +
            (me_placed ? " data-center=\"" + num(my_lat) + "," + num(my_lon) + "\"" : std::string()) +
            " data-points=\"" + html_escape(pts) + "\"></div>"
        "<div class=\"globelist\">" +
            (empty.empty() ? "<ol class=\"plain\">" + list + "</ol>" : "<p class=\"empty\">" + empty + "</p>") +
        "</div></div></div>"
        "<script src=\"" + asset_url("leaflet.js") + "\"></script>"
        "<script src=\"" + asset_url("globe.js") + "\"></script>";
    return pg(title, html);
}

}  // namespace liquidgov::site
