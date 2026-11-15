#include "site/features/map/map_search.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/http.hpp"
#include "site/core/text.hpp"
#include "site/features/map/map_graph.hpp"
#include "site/features/map/map_rules.hpp"
#include "site/features/profile/portrait.hpp"

#include <cctype>
#include <ctime>

namespace liquidgov::site {

namespace {

std::string lower(std::string t) {
    for (auto& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return t;
}
bool all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}
int to_int(const std::string& s) { return s.size() > 9 ? -1 : std::atoi(s.c_str()); }

// 1-12 for a month's name or its first three letters, else 0
int month_of(const std::string& w) {
    static const char* const names[] = {"january", "february", "march", "april", "may", "june", "july", "august",
                                        "september", "october", "november", "december"};
    if (w == "sept") return 9;
    for (int i = 0; i < 12; ++i) {
        const std::string n = names[i];
        if (w == n || (w.size() == 3 && n.compare(0, 3, w) == 0)) return i + 1;
    }
    return 0;
}
// 0 (sunday) .. 6 for a weekday's name or its first three letters, else -1
int weekday_of(const std::string& w) {
    static const char* const names[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
    for (int i = 0; i < 7; ++i) {
        const std::string n = names[i];
        if (w == n || (w.size() == 3 && n.compare(0, 3, w) == 0)) return i;
    }
    return -1;
}
// "12", "12th", "1st", "2nd", "3rd": a day of the month, else 0
int day_of(const std::string& w) {
    std::string d = w;
    for (const char* suf : {"st", "nd", "rd", "th"})
        if (d.size() > 2 && d.compare(d.size() - 2, 2, suf) == 0) { d.resize(d.size() - 2); break; }
    if (!all_digits(d) || d.size() > 2) return 0;
    const int n = to_int(d);
    return n >= 1 && n <= 31 ? n : 0;
}
// a time of day in minutes after local midnight, and how long it covers (span): "3pm", "3 pm" (that hour), "3:15pm",
// "15:15" (30 minutes from then), "noon", "midnight". -1 if not a time; a bare number is never a time.
int clock_of(const std::string& w, const std::string& next, int& span) {
    if (w == "noon") { span = 60; return 12 * 60; }
    if (w == "midnight") { span = 60; return 0; }
    std::string t = w, ap;
    if (t.size() > 2 && (t.compare(t.size() - 2, 2, "am") == 0 || t.compare(t.size() - 2, 2, "pm") == 0)) {
        ap = t.substr(t.size() - 2); t.resize(t.size() - 2);
    } else if (next == "am" || next == "pm") {
        ap = next;
    }
    int h = -1, m = 0;
    const auto colon = t.find(':');
    if (colon != std::string::npos) {
        const std::string hs = t.substr(0, colon), ms = t.substr(colon + 1);
        if (!all_digits(hs) || !all_digits(ms) || hs.size() > 2 || ms.size() != 2) return -1;
        h = to_int(hs); m = to_int(ms);
        if (m > 59) return -1;
        span = 30;
    } else {
        if (ap.empty() || !all_digits(t) || t.size() > 2) return -1;   // a bare number is not a time
        h = to_int(t);
        span = 60;
    }
    if (!ap.empty()) {
        if (h < 1 || h > 12) return -1;
        if (ap == "am") h = h == 12 ? 0 : h; else h = h == 12 ? 12 : h + 12;
    } else if (h > 23) {
        return -1;
    }
    return h * 60 + m;
}

// unix seconds of local midnight at the start of y-m-d, for an offset of tz minutes east
std::int64_t local_midnight(int y, int mo, int d, int tz) {
    std::tm tm{};
    tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
    return static_cast<std::int64_t>(timegm(&tm)) - tz * 60;
}
struct Ymd { int y, m, d, wday; };
Ymd local_date(std::int64_t t, int tz) {
    std::time_t tt = static_cast<std::time_t>(t + tz * 60);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    return {tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_wday};
}
int days_in(int y, int m) {
    static const int n[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29 : n[m - 1];
}

// a short stretch of text around the first word found, about 90 characters, cut on whole characters
std::string snippet(const std::string& text, const std::vector<std::string>& words) {
    const std::string low = lower(text);
    std::size_t at = 0;
    for (const auto& w : words) { auto p = low.find(w); if (p != std::string::npos) { at = p; break; } }
    std::size_t start = at > 30 ? at - 30 : 0;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) --start;
    std::size_t end = std::min(text.size(), start + 90);
    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) ++end;
    std::string s = text.substr(start, end - start);
    for (auto& c : s) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    return (start ? "\xE2\x80\xA6" : "") + s + (end < text.size() ? "\xE2\x80\xA6" : "");
}

struct Hit { std::int64_t ts; std::string json; };

}  // namespace

MapSearchQuery map_search_parse(const std::string& raw, std::int64_t now, int tz_min) {
    MapSearchQuery q;
    q.tz_min = tz_min < -14 * 60 || tz_min > 14 * 60 ? 0 : tz_min;
    std::vector<std::string> tok;
    {
        std::string cur;
        for (char c : lower(raw)) {
            if (std::isspace(static_cast<unsigned char>(c)) || c == ',') { if (!cur.empty()) tok.push_back(cur); cur.clear(); }
            else cur += c;
        }
        if (!cur.empty()) tok.push_back(cur);
    }
    const Ymd today = local_date(now, q.tz_min);
    const std::int64_t today0 = local_midnight(today.y, today.m, today.d, q.tz_min);
    auto day_range = [&](int y, int m, int d) {
        q.has_when = true; q.from = local_midnight(y, m, d, q.tz_min); q.to = q.from + 86400;
    };
    // a month and day with no year: the most recent one (this year, or last year if it has not come yet)
    auto past_year = [&](int m, int d) { return (m > today.m || (m == today.m && d > today.d)) ? today.y - 1 : today.y; };
    for (std::size_t i = 0; i < tok.size(); ++i) {
        const std::string& w = tok[i];
        const std::string next = i + 1 < tok.size() ? tok[i + 1] : "";
        const std::string next2 = i + 2 < tok.size() ? tok[i + 2] : "";
        if (w == "today") { q.has_when = true; q.from = today0; q.to = today0 + 86400; continue; }
        if (w == "yesterday") { q.has_when = true; q.from = today0 - 86400; q.to = today0; continue; }
        if (int wd = weekday_of(w); wd >= 0) {
            const int back = (today.wday - wd + 7) % 7;
            q.has_when = true; q.from = today0 - back * 86400; q.to = q.from + 86400;
            continue;
        }
        // 2026-11-12
        if (w.size() == 10 && w[4] == '-' && w[7] == '-' && all_digits(w.substr(0, 4)) && all_digits(w.substr(5, 2)) && all_digits(w.substr(8, 2))) {
            const int y = to_int(w.substr(0, 4)), m = to_int(w.substr(5, 2)), d = to_int(w.substr(8, 2));
            if (m >= 1 && m <= 12 && d >= 1 && d <= days_in(y, m)) { day_range(y, m, d); continue; }
        }
        // 11/12, 11/12/26, 11/12/2026
        if (w.find('/') != std::string::npos) {
            std::vector<std::string> p;
            std::string cur;
            for (char c : w) { if (c == '/') { p.push_back(cur); cur.clear(); } else cur += c; }
            p.push_back(cur);
            if ((p.size() == 2 || p.size() == 3) && all_digits(p[0]) && all_digits(p[1]) && (p.size() == 2 || all_digits(p[2]))) {
                const int m = to_int(p[0]), d = to_int(p[1]);
                int y = p.size() == 3 ? to_int(p[2]) : 0;
                if (y && y < 100) y += 2000;
                if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
                    if (!y) y = past_year(m, d);
                    if (d <= days_in(y, m)) { day_range(y, m, d); continue; }
                }
            }
        }
        // nov 12 [2026], november [2026], 12 nov [2026]
        int mo = month_of(w), d = 0, y = 0;
        std::size_t used = 0;
        if (mo && w != "may") {
            if ((d = day_of(next))) { used = 1; if (next2.size() == 4 && all_digits(next2)) { y = to_int(next2); used = 2; } }
            else if (next.size() == 4 && all_digits(next)) { y = to_int(next); used = 1; }
        } else if (mo && w == "may") {   // "may" is also a word: only a date when a day or year comes with it
            if ((d = day_of(next))) { used = 1; if (next2.size() == 4 && all_digits(next2)) { y = to_int(next2); used = 2; } }
            else if (next.size() == 4 && all_digits(next)) { y = to_int(next); used = 1; }
            else mo = 0;
        } else if ((d = day_of(w)) && (mo = month_of(next))) {
            used = 1;
            if (next2.size() == 4 && all_digits(next2)) { y = to_int(next2); used = 2; }
        } else {
            d = 0;
        }
        if (mo) {
            if (d) {
                if (!y) y = past_year(mo, d);
                if (d <= days_in(y, mo)) { day_range(y, mo, d); i += used; continue; }
            } else {
                if (!y) y = mo > today.m ? today.y - 1 : today.y;
                q.has_when = true; q.from = local_midnight(y, mo, 1, q.tz_min); q.to = q.from + days_in(y, mo) * 86400LL;
                i += used; continue;
            }
        }
        // a year on its own: 2000-2100
        if (w.size() == 4 && all_digits(w) && to_int(w) >= 2000 && to_int(w) <= 2100 && !q.has_when) {
            q.has_when = true; q.from = local_midnight(to_int(w), 1, 1, q.tz_min); q.to = local_midnight(to_int(w) + 1, 1, 1, q.tz_min);
            continue;
        }
        int span = 0;
        if (int c = clock_of(w, next, span); c >= 0) {
            q.has_clock = true; q.m0 = c; q.m1 = c + span;
            if ((next == "am" || next == "pm") && w.find("am") == std::string::npos && w.find("pm") == std::string::npos) ++i;
            continue;
        }
        if (w == "am" || w == "pm" || w == "at" || w == "on" || w == "in" || w == "of" || w == "the") continue;   // date glue
        q.words.push_back(w);
    }
    return q;
}

bool map_search_when_ok(const MapSearchQuery& q, std::int64_t ts) {
    if (q.has_when && (ts < q.from || ts >= q.to)) return false;
    if (q.has_clock) {
        const std::int64_t local = ts + q.tz_min * 60;
        const int mins = static_cast<int>(((local % 86400) + 86400) % 86400 / 60);
        if (mins < q.m0 || mins >= q.m1) return false;
    }
    return true;
}

std::string map_search_json(const std::string& viewer, const std::string& raw, int tz_min) {
    std::string out = "{\"results\":[";
    const std::string trimmed = trim_ws(raw);
    if (trimmed.size() < 2 || trimmed.size() > 120) return out + "]}";
    const MapSearchQuery q = map_search_parse(trimmed, static_cast<std::int64_t>(std::time(nullptr)), tz_min);
    const bool dated = q.has_when || q.has_clock;
    if (q.words.empty() && !dated) return out + "]}";
    auto has = [&](const std::string& text) {
        if (q.words.empty()) return true;
        const std::string low = lower(text);
        for (const auto& w : q.words) if (low.find(w) == std::string::npos) return false;
        return true;
    };
    const MapCtx ctx = map_ctx(viewer);
    std::set<std::string> seen;
    std::size_t n = 0;
    const std::size_t kNames = 8, kMax = 20;
    auto add_node = [&](const std::string& id) {
        if (n >= kNames || seen.count(id)) return;
        auto g = map_gnode(id, viewer);
        if (!g) return;
        seen.insert(id);
        out += std::string(n++ ? "," : "") + map_node_json(*g, map_relevance(*g, ctx));
    };
    // names: only for words (a date alone names nothing)
    if (!q.words.empty() && !dated) {
        for (const auto& u : people_hits(viewer, trimmed)) add_node("user:" + u);
        for (const auto& c : g_store.all_communities()) if (has(c.name + " " + c.subcategory)) add_node("group:" + c.slug);
        for (const auto& cu : g_store.cities_in_use()) if (has(cu.name)) add_node("city:" + cu.key);
        auto props = g_store.all_proposals();
        for (auto it = props.rbegin(); it != props.rend(); ++it) if (has(it->title)) add_node("prop:" + std::to_string(it->id));
    }
    // what people wrote, newest first: posts, comments on posts and proposals, and your own messages
    std::vector<Hit> hits;
    auto blocked = [&](const std::string& who) { return who != viewer && (g_store.is_blocked(viewer, who) || g_store.is_blocked(who, viewer)); };
    auto content = [&](const std::string& id, const char* kind, const std::string& page, const std::string& who,
                       std::int64_t ts, const std::string& text) {
        hits.push_back({ts, "{\"id\":\"" + json_escape(id) + "\",\"label\":\"" + json_escape(snippet(text, q.words)) +
                                "\",\"kind\":\"" + kind + "\",\"page\":\"" + json_escape(page) + "\",\"who\":\"" +
                                json_escape(who) + "\",\"ts\":" + std::to_string(ts) + "}"});
    };
    for (const auto& p : g_store.recent_posts(600)) {
        if (!map_search_when_ok(q, p.ts) || !has(p.text)) continue;
        const std::string id = "post:" + std::to_string(p.id);
        if (seen.count(id)) continue;
        if (auto g = map_gnode(id, viewer)) { seen.insert(id); content(id, "post", g->page, p.author, p.ts, p.text); }
    }
    for (const auto& r : g_store.recent_post_replies(1500)) {
        if (!map_search_when_ok(q, r.ts) || !has(r.text) || blocked(r.author)) continue;
        const std::string id = "post:" + std::to_string(r.post_id);
        if (auto g = map_gnode(id, viewer)) content(id, "comment", g->page, r.author, r.ts, r.text);
    }
    for (const auto& r : g_store.recent_proposal_replies(1500)) {
        if (!map_search_when_ok(q, r.ts) || !has(r.text) || blocked(r.author)) continue;
        const std::string id = "prop:" + std::to_string(r.proposal_id);
        if (auto g = map_gnode(id, viewer)) content(id, "comment", g->page, r.author, r.ts, r.text);
    }
    // messages: only your own conversations (1:1 mail either way, and your group chats)
    for (const auto& who : g_store.mail_peers(viewer)) {
        if (who == viewer || blocked(who)) continue;
        for (const auto& m : g_store.mail_with(viewer, who))
            if (map_search_when_ok(q, m.ts) && has(m.text)) content("", "message", "/mail?with=" + url_encode(who), m.from, m.ts, m.text);
    }
    if (auto me = g_store.find_by_username(viewer)) {
        for (const auto& conv : g_store.conversations_for(me->id)) {
            if (!conv.is_group) continue;
            bool active = false;
            for (const auto& mem : conv.members) if (mem.account == me->id && mem.state == MemberState::Active) active = true;
            if (!active) continue;
            for (const auto& m : g_store.messages_in(conv.id)) {
                if (m.deletion != DeletionState::NotDeleted || !map_search_when_ok(q, m.sent_at) || !has(m.text)) continue;
                auto from = g_store.find_by_id(m.sender);
                const std::string who = from ? from->username : "";
                if (blocked(who)) continue;
                content("", "message", "/mail/g/" + std::to_string(conv.id), who, m.sent_at, m.text);
            }
        }
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.ts > b.ts; });
    for (const auto& h : hits) {
        if (n >= kMax) break;
        out += std::string(n++ ? "," : "") + h.json;
    }
    return out + "]}";
}

}  // namespace liquidgov::site
