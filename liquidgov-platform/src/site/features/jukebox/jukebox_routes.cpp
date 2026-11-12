#include "site/features/jukebox/jukebox_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/net.hpp"
#include "site/core/state.hpp"
#include "site/core/text.hpp"
#include "site/features/jukebox/jukebox_browse.hpp"
#include "site/features/jukebox/jukebox_personal.hpp"
#include "site/features/jukebox/jukebox_widget.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/ui/shell.hpp"

namespace liquidgov::site {

std::optional<std::string> jukebox_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/jukebox/search") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        // Type-to-search used to embed YOUTUBE_API_KEY in every logged-in
        // page. The key stays on the server now, read straight from the
        // environment and never placed in any response body or log line.
        // Proxied via the system curl binary (popen), not a new HTTP/TLS
        // stack -- this binary doesn't link libcurl, and curl is already
        // on the box. term is run through url_encode() before it ever
        // touches the shell string, which leaves only
        // [A-Za-z0-9._~%-] -- none of those can break out of the
        // single-quoted argument, so this is not shell-injectable.
        auto term = trim_ws(qs["q"]);
        std::string q;
        for (unsigned char ch : term) {
            if (std::isalnum(ch) || ch==' ' || ch=='.' || ch=='_' || ch=='-') q.push_back((char)ch);
        }
        if (q.size() < 3)
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"Type at least 3 letters.\"}",
                "Cache-Control: no-store\r\n");
        std::string cache_key = q;
        for (char& c : cache_key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        const std::int64_t now_unix = now();
        {
            std::lock_guard<std::mutex> lock(g_yt_mu);
            auto it = g_yt_cache.find(cache_key);
            if (it != g_yt_cache.end() && it->second.exp > now_unix) {
                lglog::emit("jukebox", "search-cache qlen="+std::to_string(q.size()));
                return http_raw("200 OK", "application/json", it->second.body,
                    "Cache-Control: no-store\r\n");
            }
        }
        std::string cmd = "timeout 12 python3 /opt/liquidgov/scripts/yt_search.py '" + q + "' 2>/dev/null";
        std::string resp;
        if (FILE* fp = popen(cmd.c_str(), "r")) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) resp.append(buf, n);
            pclose(fp);
        }
        if (resp.empty() || resp[0] != '{' || resp.find("\"items\"") == std::string::npos) {
            lglog::emit("jukebox", "search-bad qlen="+std::to_string(q.size()));
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"Search failed.\"}",
                "Cache-Control: no-store\r\n");
        }
        {
            std::lock_guard<std::mutex> lock(g_yt_mu);
            g_yt_cache[cache_key] = YtCacheEntry{resp, now_unix + 600};
            if (g_yt_cache.size() > 64) g_yt_cache.erase(g_yt_cache.begin());
        }
        lglog::emit("jukebox", "search-ok qlen="+std::to_string(q.size())+" bytes="+std::to_string(resp.size()));
        return http_raw("200 OK", "application/json", resp,
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
    if(method=="GET"&&path=="/jukebox/browse") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto category = qs["category"];
        static const std::set<std::string> kCats = {"all","group","friends","others","self"};
        if (!kCats.count(category)) category = "all";
        return http_raw("200 OK", "application/json",
            jukebox_browse_json(user, category, trim_ws(qs["q"])),
            "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/history") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        // ?station=<id> (bug fix: the History section never showed a shared station's own
        // history -- the standalone jukebox page always sent `station=`, but this route only
        // ever understood `for=`, so it silently fell back to the viewer's OWN personal
        // plays instead. "" is the site-wide station, a real and distinct value from "no
        // station param at all" -- qs.count, not qs[].empty(), is what tells them apart.
        if (qs.count("station")) {
            std::deque<CommunityTrack> hist;
            {
                std::lock_guard<std::mutex> lock(g_community_mu);
                hist = g_community_history_by_station[qs.at("station")];
            }
            std::string out = "{\"items\":[";
            for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
                if (it != hist.rbegin()) out += ",";
                out += "{\"video_id\":\"" + json_escape(it->video_id) + "\",\"queued_by\":\"" +
                       json_escape(it->queued_by) + "\"}";
            }
            out += "]}";
            return http_raw("200 OK", "application/json", out, "Cache-Control: no-store\r\n");
        }
        // ?for=<personal-owner> scopes to whatever personal channel is
        // actively tuned in on the caller's widget; empty (nothing tuned yet)
        // falls back to the viewer's own plays, same as an owner with no
        // plays of their own yet falls back to nothing rather than erroring.
        auto for_owner = trim_ws(qs["for"]);
        auto hist = (!for_owner.empty() && valid_username(for_owner)) ? g_store.jukebox_history_of(for_owner)
                                                                        : std::vector<JukeboxHistoryEntry>{};
        if (hist.empty()) hist = g_store.jukebox_history_of(user);
        std::reverse(hist.begin(), hist.end());
        if (hist.size() > 15) hist.resize(15);
        std::string out = "{\"items\":[";
        for (std::size_t i = 0; i < hist.size(); ++i) {
            if (i) out += ",";
            out += "{\"video_id\":\"" + json_escape(hist[i].video_id) + "\",\"queued_by\":\"" +
                   json_escape(hist[i].queued_by) + "\"}";
        }
        out += "]}";
        return http_raw("200 OK", "application/json", out, "Cache-Control: no-store\r\n");
    }
    // Title/creator for one video in the coverflow carousel (see
    // jukebox_widget's owner-view build). oEmbed needs no API key at all
    // (unlike the Data API v3 search this file already proxies) -- id is
    // never anything but an already-queued, already-validated 11-char
    // YouTube id, checked again here since it's about to go straight into
    // a shell command via popen.
    if(method=="GET"&&path=="/jukebox/channel") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto cid = trim_ws(qs["id"]);
        if (cid.size() < 10)
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"bad channel\"}", "Cache-Control: no-store\r\n");
        for (char ch : cid) {
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch=='_' || ch=='-'))
                return http_raw("200 OK", "application/json",
                    "{\"items\":[],\"error\":\"bad channel\"}", "Cache-Control: no-store\r\n");
        }
        std::string cmd = "timeout 12 python3 /opt/liquidgov/scripts/yt_search.py --channel '" + cid + "' 2>/dev/null";
        std::string resp;
        if (FILE* fp = popen(cmd.c_str(), "r")) {
            char buf[4096]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) resp.append(buf, n);
            pclose(fp);
        }
        if (resp.empty() || resp[0] != '{' || resp.find("\"items\"") == std::string::npos)
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"No videos on that channel.\"}", "Cache-Control: no-store\r\n");
        return http_raw("200 OK", "application/json", resp, "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/oembed") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{}");
        auto id = qs["id"];
        bool valid_id = id.size() == 11;
        if (valid_id) for (unsigned char c : id)
            if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) { valid_id = false; break; }
        if (!valid_id) return http_raw("400 Bad Request", "application/json", "{}");
        std::string url = "https://www.youtube.com/oembed?format=json&url=" +
            url_encode("https://www.youtube.com/watch?v=" + id);
        std::string resp = curl_get(url, 5);
        // Juketube B (Brandon 2026-11-08): YouTube's oEmbed answers 401 "Unauthorized" when the owner blocks
        // embedding and 404 "Not Found" / 400 "Bad Request" for a removed or private video (curl_get keeps the body),
        // so the picker can say so before the room tries it. Empty or anything else = unknown: "{}" as before.
        if (resp.rfind("Unauthorized", 0) == 0 || resp.rfind("Forbidden", 0) == 0)
            return http_raw("200 OK", "application/json", "{\"embeddable\":false}", "Cache-Control: no-store\r\n");
        if (resp.rfind("Not Found", 0) == 0 || resp.rfind("Bad Request", 0) == 0)
            return http_raw("200 OK", "application/json", "{\"embeddable\":false,\"missing\":true}", "Cache-Control: no-store\r\n");
        if (resp.empty() || resp[0] != '{')
            return http_raw("200 OK", "application/json", "{}", "Cache-Control: no-store\r\n");
        auto title = oembed_json_string_field(resp, "title");
        auto author = oembed_json_string_field(resp, "author_name");
        std::string out = "{\"title\":\""+json_escape(title)+"\",\"author_name\":\""+json_escape(author)+"\",\"embeddable\":true}";
        // A video's title/uploader essentially never changes -- fine to
        // cache in the browser across the session instead of re-curling
        // every time the same track scrolls back into focus.
        return http_raw("200 OK", "application/json", out, "Cache-Control: private, max-age=86400\r\n");
    }
    if(method=="GET"&&path=="/listen") {
        // Deliberately not wrap()/pg(): this window IS the player, full-page
        // nav chrome and a second (redundant) floating-widget toggle would
        // just be noise in a small popout.
        if(!acc) return need_login();
        return http_ok(page_wrap("Listen", "",
            "<div style=\"padding:.75rem\">" + jukebox_standalone_page(youtube_api_key_from_env(), user) + "</div>"));
    }
    // Polled by the floating widget every few seconds while a personal
    // channel is the active station (see pollPersonal in jukebox_script) --
    // the widget's substitute for the WebSocket push the site-wide/
    // community stations get, since a personal jukebox is an ordinary HTTP
    // queue with no live sync (jukebox_widget's own comment). Shape matches
    // the community WS snapshot exactly so the client can share one handler.
    // Must come before the generic GET /u/<username> handler just below --
    // that one only expects a bare username after "/u/" and would otherwise
    // 404 this as an invalid username before this more specific route ever
    // gets a look (caught live: fetching this returned the generic
    // "Not found" page instead of JSON).
    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/now")==0) {
        const std::string who = path.substr(3, path.size()-3-12);
        if (!valid_username(who)) return http_code("404 Not Found", "");
        auto found_now = g_store.find_by_username(who);
        if (!found_now) return http_code("404 Not Found", "");
        if (acc && g_store.is_blocked(who, user)) return http_code("404 Not Found", "");
        auto target_prefs = g_store.prefs_of(who);
        const bool is_owner = acc && user == who;
        const bool is_friend = acc && !is_owner && g_store.are_friends(user, who);
        if (!visible_to(target_prefs.profile_visibility, is_owner, acc.has_value(), is_friend))
            return http_code("404 Not Found", "");
        // "Play this Juketube instead" (see /me): who's page this is and
        // whose queue actually plays here can differ, one hop only.
        const std::string effective_owner = g_store.jukebox_effective_owner(who);
        auto tracks = g_store.jukebox_of(effective_owner);
        std::ostringstream os;
        os << "{";
        if (!tracks.empty()) {
            const auto& t = tracks.front();
            os << "\"now\":{\"video_id\":\"" << t.video_id << "\",\"queued_by\":\"" << t.queued_by
               << "\",\"elapsed\":" << personal_jukebox_elapsed("u:"+effective_owner, t.video_id) << "}";
        } else {
            os << "\"now\":null";
        }
        os << ",\"upcoming\":" << (tracks.empty() ? 0 : tracks.size() - 1);
        os << ",\"queue\":[";
        {
            bool qfirst = true;
            for (size_t qi = 1; qi < tracks.size(); ++qi) {
                if (!qfirst) os << ",";
                qfirst = false;
                os << "{\"video_id\":\"" << tracks[qi].video_id << "\",\"queued_by\":\"" << tracks[qi].queued_by << "\"}";
            }
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str());
    }
    if(method=="POST"&&path=="/me/jukebox/add") {
        if(!acc) return need_login();
        auto url = trim_ws(parse_form(body)["url"]);
        auto id = extract_youtube_id(url);
        if (!id) return http_redir("/me?err=url");
        if (!g_store.add_jukebox_track(user, *id, user)) return http_redir("/me?err=full");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/remove") {
        if(!acc) return need_login();
        g_store.remove_jukebox_track(user, parse_form(body)["video_id"]);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/play-settings") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        g_store.set_jukebox_autoplay(user, f["autoplay"]=="1");
        g_store.set_jukebox_relay_target(user, trim_ws(f["relay"]));
        return http_redir("/me?ok=jplay");
    }
    if(method=="POST"&&path=="/me/jukebox/policy") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        int pv = 0;
        try { pv = std::stoi(f["policy"]); } catch (...) { pv = 0; }
        if (pv < 0 || pv > 2) pv = 0;
        auto p = g_store.prefs_of(user);
        p.jukebox_policy = static_cast<JukeboxPolicy>(pv);
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    // A guest's one-shot request on someone else's jukebox. Gated by the
    // owner's JukeboxPolicy, the owner's block list, and one outstanding
    // request per guest -- add_jukebox_track itself only enforces the
    // account/id/cap checks it always has. Also reachable by the owner
    // themselves (posting from their own /u/<username> page rather than
    // /me), in which case none of the guest gates apply -- same as /me/jukebox/add.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/add")==0) {
        const std::string who = path.substr(3, path.size()-3-12);
        if(!acc) return need_login();
        if (!valid_username(who)) return http_redir("/");
        auto found = g_store.find_by_username(who);
        if (!found) return http_redir("/");
        if (who != user) {
            if (g_store.is_blocked(who, user)) return http_redir("/u/"+who+"?err=denied");
            auto policy = g_store.prefs_of(who).jukebox_policy;
            bool allowed = policy == JukeboxPolicy::Anyone ||
                           (policy == JukeboxPolicy::Friends && g_store.are_friends(user, who));
            if (!allowed) return http_redir("/u/"+who+"?err=denied");
            for (const auto& t : g_store.jukebox_of(who)) {
                if (t.queued_by == user) return http_redir("/u/"+who+"?err=pending");
            }
        }
        auto url = trim_ws(parse_form(body)["url"]);
        auto id = extract_youtube_id(url);
        if (!id) return http_redir("/u/"+who+"?err=url");
        if (!g_store.add_jukebox_track(who, *id, user)) return http_redir("/u/"+who+"?err=full");
        return http_redir("/u/"+who);
    }
    // Fired by the floating widget's own player when a personal channel's
    // current track actually finishes -- see pollPersonal's onStateChange
    // handling in jukebox_script. complete_jukebox_track looks up the
    // track's own stored queued_by itself (never trusts anything from the
    // client beyond the video id), so this is safe to call from anyone
    // signed in, owner or visitor. Resolved through the same one-hop
    // relay as /jukebox/now -- a completion has to land on whichever
    // queue is actually playing, not the page it was fired from.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>20&&
       path.compare(path.size()-17,17,"/jukebox/complete")==0) {
        const std::string who = path.substr(3, path.size()-3-17);
        if(!acc) return need_login();
        if (valid_username(who)) {
            auto jf = parse_form(body);
            const auto jb_owner = g_store.jukebox_effective_owner(who);
            if (jf["skip"] == "1") g_store.skip_jukebox_track(jb_owner, jf["video_id"]);
            else g_store.complete_jukebox_track(jb_owner, jf["video_id"]);
        }
        return http_ok("ok");
    }
    // The polling widget's substitute for a WebSocket connect/disconnect
    // on a personal station (see personal_heartbeat_locked/
    // personal_freeze_locked above) -- a personal jukebox has never had a
    // live socket. Signed-in only, unlike listening itself (open to any
    // visitor who can view the profile): an anonymous visitor can still
    // hear it, it just never counts toward who's "still here" for
    // freeze/resume. video_id must match what's actually at the front of
    // the effective queue right now -- same distrust as complete_
    // jukebox_track, so a stale or mismatched client can't stomp another
    // track's saved position.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/pos")==0) {
        const std::string who = path.substr(3, path.size()-3-12);
        if(!acc) return need_login();
        if (!valid_username(who)) return http_ok("ok");
        auto found_pos = g_store.find_by_username(who);
        if (!found_pos) return http_ok("ok");
        const std::string effective_owner = g_store.jukebox_effective_owner(who);
        auto tracks = g_store.jukebox_of(effective_owner);
        if (tracks.empty()) return http_ok("ok");
        auto form = parse_form(body);
        if (form["video_id"] != tracks.front().video_id) return http_ok("ok");
        const std::string station = "u:" + effective_owner;
        const std::string sid = cookie_of(req);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::lock_guard<std::mutex> lock(g_personal_mu);
        if (form["freeze"] == "1") {
            std::optional<std::int64_t> client_offset;
            if (!form["offset"].empty()) {
                try { client_offset = std::stoll(form["offset"]); } catch (...) {}
            }
            personal_freeze_locked(station, tracks.front().video_id, sid, client_offset, now);
        } else {
            personal_heartbeat_locked(station, tracks.front().video_id, sid, now);
        }
        return http_ok("ok");
    }
    // A signed-in viewer follows someone's personal jukebox as a "channel"
    // in their own floating widget (jukebox_script's Follow button). This
    // is a real, server-recorded subscription -- not just this device's
    // own station list -- specifically so the owner can see how many
    // people are tuned in (jukebox_widget's subscriber count, owner_view
    // only). Listening itself was already open to anyone who could view
    // the profile; the same profile_visibility/block gates as the page
    // itself apply here so this can't be used to probe a private profile.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>21&&
       path.compare(path.size()-18,18,"/jukebox/subscribe")==0) {
        const std::string who = path.substr(3, path.size()-3-18);
        if(!acc) return http_code("401 Unauthorized", "");
        if (!valid_username(who) || who == user) return http_code("404 Not Found", "");
        auto found = g_store.find_by_username(who);
        if (!found) return http_code("404 Not Found", "");
        if (g_store.is_blocked(who, user)) return http_code("403 Forbidden", "");
        auto target_prefs = g_store.prefs_of(who);
        if (!visible_to(target_prefs.profile_visibility, false, true, g_store.are_friends(user, who)))
            return http_code("403 Forbidden", "");
        g_store.subscribe_jukebox(user, who);
        return http_ok("ok");
    }
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>23&&
       path.compare(path.size()-20,20,"/jukebox/unsubscribe")==0) {
        const std::string who = path.substr(3, path.size()-3-20);
        if(!acc) return http_code("401 Unauthorized", "");
        if (valid_username(who)) g_store.unsubscribe_jukebox(user, who);
        return http_ok("ok");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
