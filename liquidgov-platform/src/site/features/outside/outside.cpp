#include "site/features/outside/outside.hpp"
#include "site/core/http.hpp"

#include <chrono>
#include <deque>
#include <future>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <thread>
#include <algorithm>

namespace liquidgov::site {

namespace {

struct Entry { bool ok = false; std::string title, extract, url; std::chrono::steady_clock::time_point exp; };
std::mutex g_mu;
std::map<std::string, Entry> g_cache;
std::deque<std::chrono::steady_clock::time_point> g_recent;   // when the last new requests went out

std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32); return s; }

// the string value of "key":"..." (first top-level-looking occurrence), JSON escapes decoded to UTF-8
bool json_string(const std::string& j, const std::string& key, std::string& out) {
    // "key" then optional spaces, a colon, optional spaces, an opening quote (the service may or may not pad its JSON)
    const std::string pat = "\"" + key + "\"";
    std::size_t at = j.find(pat);
    std::size_t start = std::string::npos;
    while (at != std::string::npos && start == std::string::npos) {
        std::size_t p = at + pat.size();
        while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\n')) ++p;
        if (p < j.size() && j[p] == ':') {
            ++p;
            while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\n')) ++p;
            if (p < j.size() && j[p] == '"') start = p + 1;
        }
        if (start == std::string::npos) at = j.find(pat, at + 1);
    }
    if (start == std::string::npos) return false;
    out.clear();
    auto hex4 = [&](std::size_t p, unsigned& v) {
        if (p + 4 > j.size()) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = j[p + i]; v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else return false;
        }
        return true;
    };
    auto put = [&](unsigned cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    };
    for (std::size_t i = start; i < j.size(); ++i) {
        char c = j[i];
        if (c == '"') return true;
        if (c != '\\') { out += c; continue; }
        if (++i >= j.size()) return false;
        switch (j[i]) {
            case 'n': out += ' '; break;      // a newline in a summary becomes a space
            case 't': out += ' '; break;
            case 'r': break;
            case 'b': case 'f': break;
            case 'u': {
                unsigned cp = 0;
                if (!hex4(i + 1, cp)) return false;
                i += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < j.size() && j[i + 1] == '\\' && j[i + 2] == 'u') {
                    unsigned lo = 0;
                    if (hex4(i + 3, lo) && lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 6; }
                }
                put(cp);
                break;
            }
            default: out += j[i]; break;   // \" \\ \/
        }
    }
    return false;   // never closed
}

// cut to about `max` bytes at the end of a sentence if there is one past half, else at a word, never mid-character
std::string shorten(const std::string& s, std::size_t max) {
    if (s.size() <= max) return s;
    std::size_t cut = s.rfind(". ", max);
    if (cut != std::string::npos && cut > max / 2) return s.substr(0, cut + 1);
    cut = s.rfind(' ', max);
    if (cut == std::string::npos || cut < max / 2) cut = max;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;   // back to a character start
    std::string o = s.substr(0, cut);
    while (!o.empty() && (o.back() == ' ' || o.back() == ',' || o.back() == ';')) o.pop_back();
    return o + "\xE2\x80\xA6";
}

std::string clean_term(const std::string& in) {
    std::string o;
    for (unsigned char c : in) {
        if (c < 0x20 || c == 0x7f) continue;
        if (c == ' ' || c == '\t') { if (!o.empty() && o.back() != '_') o += '_'; continue; }
        o += static_cast<char>(c);
    }
    while (!o.empty() && o.back() == '_') o.pop_back();
    return o;
}

}  // namespace

bool outside_wikipedia(const std::string& term, std::string& title, std::string& extract, std::string& url) {
    const std::string t = clean_term(trim_ws(term));
    if (t.size() < 2 || t.size() > 60) return false;
    bool letter = false;
    for (unsigned char c : t) if (std::isalpha(c) || c >= 0x80) { letter = true; break; }
    if (!letter) return false;
    const std::string key = lower(t);
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_cache.find(key);
        if (it != g_cache.end() && it->second.exp > now) {
            if (!it->second.ok) return false;
            title = it->second.title; extract = it->second.extract; url = it->second.url;
            return true;
        }
    }
    {   // at most 30 new requests a minute for the whole site, so typing in the search bar can never hammer Wikipedia
        std::lock_guard<std::mutex> lock(g_mu);
        while (!g_recent.empty() && now - g_recent.front() > std::chrono::seconds(60)) g_recent.pop_front();
        if (g_recent.size() >= 30) return false;
        g_recent.push_back(now);
    }
    const char* base_env = std::getenv("LIQUIDGOV_WIKI_BASE");
    const std::string base = base_env && *base_env ? base_env : "https://en.wikipedia.org/api/rest_v1/page/summary/";
    // url_encode leaves only [A-Za-z0-9._~%-], so the term cannot break out of the single-quoted argument. A generic
    // client name (no contact details, no account, no cookies); redirects followed (a lowercase title lands on the article).
    const std::string cmd = "curl -sS -L --max-time 3 --max-filesize 200000 -A 'LiquidGov-Reader/1.0' '" + base + url_encode(t) + "' 2>/dev/null";
    std::string resp;
    if (FILE* fp = popen(cmd.c_str(), "r")) {
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0 && resp.size() < 300000) resp.append(buf, n);
        pclose(fp);
    }
    Entry e;
    std::string type, ttl, ext, page;
    if (!resp.empty() && resp[0] == '{' && json_string(resp, "extract", ext) && !trim_ws(ext).empty()) {
        json_string(resp, "type", type);
        json_string(resp, "title", ttl);
        if (type != "disambiguation") {
            e.ok = true;
            e.title = ttl.empty() ? t : ttl;
            e.extract = shorten(trim_ws(ext), 600);
            // the article address: Wikipedia's own, built from the title it gave (never an address taken from the reply)
            std::string slug = e.title;
            for (char& c : slug) if (c == ' ') c = '_';
            e.url = "https://en.wikipedia.org/wiki/" + url_encode(slug);
        }
    }
    e.exp = now + (e.ok ? std::chrono::hours(24) : std::chrono::hours(1));
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_cache.size() >= 500) g_cache.clear();
        g_cache[key] = e;
    }
    if (!e.ok) return false;
    title = e.title; extract = e.extract; url = e.url;
    return true;
}

void outside_fill_interest(GNode& n) {
    if (n.kind != "interest" || !n.body.empty()) return;
    std::string title, extract, url;
    if (!outside_wikipedia(n.label, title, extract, url)) return;
    n.body = extract;
    n.by = "Wikipedia";
    n.page = url;   // Wikipedia's own address, built from the title it returned; the map opens it in a new tab (lgmap.js isOut)
    n.sub = lower(title) == lower(n.label) ? "from Wikipedia" : "from Wikipedia: " + title;   // only a different name is worth showing
}

void outside_fill_interests(std::vector<GLink>& links) {
    std::vector<std::future<void>> jobs;
    for (auto& l : links) {
        if (l.to.kind != "interest" || !l.to.body.empty() || jobs.size() >= 12) continue;
        GNode* n = &l.to;
        jobs.push_back(std::async(std::launch::async, [n] { outside_fill_interest(*n); }));
    }
    for (auto& j : jobs) j.get();
}

std::string outside_topic_id(const std::string& typed) {
    std::string t = trim_ws(typed);
    if (t.size() < 3 || t.size() > 60) return "";
    int words = 1; bool letter = false;
    for (unsigned char c : t) {
        if (c == ' ') { ++words; continue; }
        if (std::isalpha(c) || c >= 0x80) { letter = true; continue; }
        if (c == '-' || c == '\'' || c == '.') continue;
        return "";   // digits and other characters: not a topic name (dates and times are handled elsewhere)
    }
    if (!letter || words > 4) return "";
    std::string id = "topic:";
    for (char c : t) id += c == ' ' ? '_' : c;
    return id;
}

bool outside_topic(const std::string& rest, GNode& out) {
    if (rest.size() < 3 || rest.size() > 60) return false;
    for (unsigned char c : rest)
        if (!(std::isalnum(c) || c >= 0x80 || c == '_' || c == '-' || c == '\'' || c == '.' || c == '(' || c == ')' || c == ',')) return false;
    std::string term = rest;
    for (char& c : term) if (c == '_') c = ' ';
    std::string title, extract, url;
    if (!outside_wikipedia(term, title, extract, url)) return false;
    std::string slug = title;
    for (char& c : slug) if (c == ' ') c = '_';
    out = GNode{"topic:" + slug, title, "interest", url, "from Wikipedia"};
    out.body = extract; out.by = "Wikipedia";
    return true;
}


// ---- City points for the globe pages (map globes G1, Brandon 2026-11-20: "a real street map", "nearest first") ----
// A city name as people typed it ("Fresno, CA") -> its latitude and longitude, from OpenStreetMap's Nominatim search
// through the system curl, the same way as the Wikipedia summary above: a generic client name, no cookies, nothing
// about the visitor; only the city text is sent. Nominatim's rules: at most one request a second for the whole site
// (a caller waits for its turn, at most about a second), and results are kept (30 days found, 1 hour not found, 2000
// cities). may_fetch=false answers from the cache only. LIQUIDGOV_GEO_BASE points tests at a local stand-in.
namespace {
struct Point { bool ok = false; double lat = 0, lon = 0; std::chrono::steady_clock::time_point exp; };
std::mutex g_geo_mu;
std::map<std::string, Point> g_geo;
std::chrono::steady_clock::time_point g_geo_last{};
}  // namespace

bool outside_city_point(const std::string& city, double& lat, double& lon, bool may_fetch) {
    const std::string t = trim_ws(city);
    if (t.size() < 2 || t.size() > 80) return false;
    const std::string key = lower(t);
    {
        std::lock_guard<std::mutex> lock(g_geo_mu);
        auto it = g_geo.find(key);
        if (it != g_geo.end() && it->second.exp > std::chrono::steady_clock::now()) {
            if (!it->second.ok) return false;
            lat = it->second.lat; lon = it->second.lon;
            return true;
        }
        if (!may_fetch) return false;
        // one request a second, site-wide: take the next free second (wait for it below, outside the lock)
        const auto now = std::chrono::steady_clock::now();
        const auto slot = std::max(now, g_geo_last + std::chrono::milliseconds(1050));
        if (slot - now > std::chrono::milliseconds(1100)) return false;   // others are queued: try again on a later visit
        g_geo_last = slot;
        if (slot > now) std::this_thread::sleep_until(slot);
    }
    const char* base_env = std::getenv("LIQUIDGOV_GEO_BASE");
    const std::string base = base_env && *base_env ? base_env : "https://nominatim.openstreetmap.org/search";
    // url_encode leaves only [A-Za-z0-9._~%-], so the city cannot break out of the single-quoted argument
    const std::string cmd = "curl -sS --max-time 3 --max-filesize 200000 -A 'LiquidGov-Reader/1.0' '" + base +
                            "?format=jsonv2&limit=1&q=" + url_encode(t) + "' 2>/dev/null";
    std::string resp;
    if (FILE* fp = popen(cmd.c_str(), "r")) {
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0 && resp.size() < 300000) resp.append(buf, n);
        pclose(fp);
    }
    Point p;
    std::string a, o;
    if (!resp.empty() && resp[0] == '[' && json_string(resp, "lat", a) && json_string(resp, "lon", o)) {
        try {
            std::size_t ia = 0, io = 0;
            p.lat = std::stod(a, &ia); p.lon = std::stod(o, &io);
            p.ok = ia == a.size() && io == o.size() && p.lat >= -90 && p.lat <= 90 && p.lon >= -180 && p.lon <= 180;
        } catch (...) { p.ok = false; }
    }
    p.exp = std::chrono::steady_clock::now() + (p.ok ? std::chrono::hours(24 * 30) : std::chrono::hours(1));
    {
        std::lock_guard<std::mutex> lock(g_geo_mu);
        if (g_geo.size() >= 2000) g_geo.clear();
        g_geo[key] = p;
    }
    if (!p.ok) return false;
    lat = p.lat; lon = p.lon;
    return true;
}

}  // namespace liquidgov::site
