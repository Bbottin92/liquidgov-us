#!/usr/bin/env python3
"""Idempotent: add /jukebox/meta, /jukebox/people, /u/*/jukebox/history."""
from pathlib import Path

p = Path("/opt/liquidgov/src/http_server.cpp")
s = p.read_text()

NEEDLE = '''        return http_raw("200 OK", "application/json", resp,
            "Cache-Control: no-store\\r\\n");
    }
    if(method=="GET"&&path=="/mail/group/new") {'''

META = r'''        return http_raw("200 OK", "application/json", resp,
            "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/meta") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{}");
        auto jesc = [](const std::string& in) {
            std::string o = "\"";
            for (unsigned char c : in) {
                if (c=='"' || c=='\\') { o += '\\'; o += char(c); }
                else if (c>=32) o += char(c);
            }
            o += "\"";
            return o;
        };
        std::ostringstream os;
        os << "{\"me\":" << jesc(user) << ",\"stations\":[";
        os << "{\"id\":\"\",\"name\":\"Site-wide\",\"kind\":\"site\"}";
        os << ",{\"id\":" << jesc(std::string("u:")+user) << ",\"name\":\"My Juketube\",\"kind\":\"person\"}";
        for (const auto& c : g_store.communities_of(user)) {
            os << ",{\"id\":" << jesc(c.slug) << ",\"name\":" << jesc(c.name) << ",\"kind\":\"group\"}";
        }
        for (const auto& owner : g_store.jukebox_subscriptions_of(user)) {
            os << ",{\"id\":" << jesc(std::string("u:")+owner) << ",\"name\":" << jesc(owner) << ",\"kind\":\"person\"}";
        }
        os << "],\"friends\":[";
        bool first = true;
        for (const auto& n : g_store.friends_of(user)) {
            if (!first) os << ",";
            first = false;
            os << jesc(n);
        }
        os << "],\"groups\":[";
        first = true;
        for (const auto& c : g_store.communities_of(user)) {
            if (!first) os << ",";
            first = false;
            os << "{\"id\":" << jesc(c.slug) << ",\"name\":" << jesc(c.name) << "}";
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str(), "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/people") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto jesc = [](const std::string& in) {
            std::string o = "\"";
            for (unsigned char c : in) {
                if (c=='"' || c=='\\') { o += '\\'; o += char(c); }
                else if (c>=32) o += char(c);
            }
            o += "\"";
            return o;
        };
        auto term = trim_ws(qs["q"]);
        auto hits = people_hits(user, term);
        std::ostringstream os;
        os << "{\"items\":[";
        bool first = true;
        for (const auto& n : hits) {
            if (n == user || g_store.are_friends(user, n)) continue;
            if (!first) os << ",";
            first = false;
            os << "{\"name\":" << jesc(n) << "}";
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str(), "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/mail/group/new") {'''

if "path==\"/jukebox/meta\"" in s:
    print("meta_already 1")
else:
    n = s.count(NEEDLE)
    print("meta_needle", n)
    if n != 1:
        print("STOP meta")
        raise SystemExit(1)
    s = s.replace(NEEDLE, META, 1)
    print("ok_meta 1")

HIST_NEEDLE = '''    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/now")==0) {'''

HIST = r'''    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>19&&
       path.compare(path.size()-16,16,"/jukebox/history")==0) {
        const std::string who = path.substr(3, path.size()-3-16);
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        if (!valid_username(who)) return http_code("404 Not Found", "");
        auto found_h = g_store.find_by_username(who);
        if (!found_h) return http_code("404 Not Found", "");
        if (g_store.is_blocked(who, user)) return http_code("404 Not Found", "");
        auto hp = g_store.prefs_of(who);
        const bool is_owner = user == who;
        const bool is_friend = !is_owner && g_store.are_friends(user, who);
        if (!visible_to(hp.profile_visibility, is_owner, true, is_friend))
            return http_code("404 Not Found", "");
        auto jesc = [](const std::string& in) {
            std::string o = "\"";
            for (unsigned char c : in) {
                if (c=='"' || c=='\\') { o += '\\'; o += char(c); }
                else if (c>=32) o += char(c);
            }
            o += "\"";
            return o;
        };
        auto hist = g_store.jukebox_history_of(who);
        std::reverse(hist.begin(), hist.end());
        std::ostringstream os;
        os << "{\"items\":[";
        bool first = true;
        std::size_t shown = 0;
        for (const auto& h : hist) {
            if (shown++ >= 20) break;
            if (!first) os << ",";
            first = false;
            os << "{\"video_id\":" << jesc(h.video_id)
               << ",\"queued_by\":" << jesc(h.queued_by)
               << ",\"played_at\":" << h.played_at << "}";
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str(), "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/now")==0) {'''

if "jukebox/history" in s:
    print("hist_already 1")
else:
    n = s.count(HIST_NEEDLE)
    print("hist_needle", n)
    if n != 1:
        print("STOP hist")
        raise SystemExit(1)
    s = s.replace(HIST_NEEDLE, HIST, 1)
    print("ok_hist 1")

p.write_text(s)
print("widget_assets", s.count("assets/juketube.js"))
