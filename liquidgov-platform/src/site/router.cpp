#include "site/router.hpp"
#include "site/core/state.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/text.hpp"
#include "site/core/net.hpp"
#include "site/core/html.hpp"
#include "site/features/profile/profile_css.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/favicon_cookies.hpp"
#include "site/ui/shell.hpp"
#include "site/core/http.hpp"
#include "site/ui/wrap.hpp"
#include "site/core/multipart.hpp"
#include "site/core/names_categories.hpp"
#include "site/features/pains/pains.hpp"
#include "site/features/map/map_rules.hpp"
#include "site/features/map/map_graph.hpp"
#include "site/features/map/map_svg.hpp"
#include "site/features/mail/mail_attachments.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/features/jukebox/jukebox_browse.hpp"
#include "site/features/people/people.hpp"
#include "site/features/notices/notices.hpp"
#include "site/features/mail/mail_pages.hpp"
#include "site/features/canvas/canvas.hpp"
#include "site/features/live/live_sessions.hpp"
#include "site/features/jukebox/jukebox_widget.hpp"
#include "site/features/pages/genesis.hpp"
#include "site/features/map/map_page.hpp"
#include "site/features/pages/home.hpp"
#include "site/features/pages/static_pages.hpp"
#include "site/features/admin/admin.hpp"
#include "site/features/auth/signup.hpp"
#include "site/features/feed/post_block.hpp"
#include "site/features/proposals/proposals.hpp"
#include "site/features/feed/composer.hpp"
#include "site/features/jukebox/jukebox_personal.hpp"
#include "site/features/push/push_subs.hpp"

namespace liquidgov::site {
// `req` is the request line and headers only; the body arrives on its own
// (read_http_request keeps them apart so an upload is never copied).
std::string handle(const std::string& req, const std::string& peer_ip, std::string body) {
    auto line_end=req.find("\r\n");
    if(line_end==std::string::npos) line_end=req.find('\n');
    if(line_end==std::string::npos) return http_ok(wrap("Error","",0,"<p>bad request</p>"));
    std::string method, rawpath; { std::istringstream is(req.substr(0,line_end)); is>>method>>rawpath; }
    if (method == "HEAD") method = "GET";
    std::string path = rawpath;
    auto qpos=path.find('?'); if(qpos!=std::string::npos) path=path.substr(0,qpos);
    // Program uploads come in through /mail/big (nginx lets big bodies only
    // in there, after asking /internal/upload-ok); past the door they are
    // the ordinary /mail and /mail/g/<id>/send posts.
    if (method == "POST" && (path == "/mail/big" || path.rfind("/mail/big/", 0) == 0)) path = "/mail" + path.substr(9);
    auto qs = parse_qs(rawpath);
    std::lock_guard<std::mutex> lock(g_mu);
    auto proto = header_value(req, "X-Forwarded-Proto");
    for (char& c : proto) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    g_https = (proto == "https");
    {
        auto dest = header_value(req, "Sec-Fetch-Dest");
        for (char& c : dest) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        auto eq = qs.find("embed");
        g_embed = dest == "iframe" || (eq != qs.end() && eq->second == "1");
    }
    const std::string ip = forwarded_ip(req, peer_ip);
    g_theme_mode = theme_of(req);
    auto acc = account_from_req(req);
    std::string user = acc ? acc->username : "";
    const std::string sid = cookie_of(req);
    const int unread = acc ? g_store.unread_count(user) : 0;
    auto pg = [&](const std::string& title, const std::string& b) {
        return http_ok(wrap(title, user, unread, b));
    };
    auto need_login = [&]() {
        if (method == "GET") return http_redir(login_url(path));
        return http_redir("/login");
    };
    // A signed-in non-operator gets exactly the same body as a genuinely
    // missing page -- /admin must not be distinguishable from a 404 to
    // anyone it isn't for.
    auto admin_404 = [&]() {
        return http_code("404 Not Found", wrap("Not found", user, unread,
            "<div class=\"card\"><h1>Not found</h1><p>That page is not here.</p></div>"));
    };

    // nginx's auth_request for the /mail/big upload door (nginx blocks
    // /internal/ from outside).
    if (path == "/internal/upload-ok") return upload_ok_response(req);

    if (path.size() > 1 && path.back() == '/') {
        std::string dest = path.substr(0, path.size() - 1);
        auto q = rawpath.find('?');
        if (q != std::string::npos) dest += rawpath.substr(q);
        return http_redir(dest);
    }

    if(method=="GET"&&path=="/robots.txt")
        return http_raw("200 OK", "text/plain; charset=utf-8",
            "User-agent: *\nAllow: /\nDisallow: /settings\nDisallow: /mail\nDisallow: /export\nDisallow: /internal\n");
    if(method=="GET"&&path=="/favicon.ico")
        return http_redir("/favicon.svg");

    // Same-box only: the commons-daemon sibling forwards a visitor's raw
    // Cookie header here to resolve it into an account, exactly the way
    // account_from_req() already does for civic's own pages (cookie-bind.md,
    // Persistence/TheCommons). Never reachable from the public internet --
    // nginx's only path in sets X-Forwarded-For (nginx-liquidgov.us.conf), so
    // its presence means this request came through the proxy, not directly
    // from another loopback process, and is refused the same way /logs
    // refuses a non-operator: a 404 indistinguishable from no route at all.
    if(method=="GET"&&path=="/internal/session-user") {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        return http_raw("200 OK", "application/json",
            acc ? ("{\"username\":\"" + json_escape(user) + "\"}") : std::string("{}"),
            "Cache-Control: no-store\r\n");
    }

    // Same-box only, same gate as /internal/session-user above (refused as
    // a plain 404 if X-Forwarded-For is present or the raw peer isn't
    // loopback). The commons-daemon forwards a visitor's own Cookie here to
    // learn who that session's mutual friends are (glue.md "Friends query").
    // The name comes from the session -- never from a query parameter, so
    // there is no way to ask about anyone but yourself. Read-only: mutual
    // accepts only (friends_of), minus anyone blocked in either direction.
    if(method=="GET"&&path=="/internal/friends") {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        std::string body = "{}";
        if (acc) {
            body = "{\"username\":\"" + json_escape(user) + "\",\"friends\":[";
            bool first = true;
            for (const auto& f : g_store.friends_of(user)) {
                if (g_store.is_blocked(user, f) || g_store.is_blocked(f, user)) continue;
                if (!first) body += ",";
                first = false;
                body += "\"" + json_escape(f) + "\"";
            }
            body += "]}";
        }
        return http_raw("200 OK", "application/json", body, "Cache-Control: no-store\r\n");
    }

    // Same-box only, same gate as /internal/session-user and /internal/friends
    // above (a plain 404 if X-Forwarded-For is present or the raw peer isn't
    // loopback -- nginx's only path in sets X-Forwarded-For, so this can never be
    // reached from the public internet). The commons-daemon asks it whether a
    // visitor may walk into an `open` house: the VIEWER is the session cookie
    // (never a parameter), the path is the house OWNER. {"blocked":true} if
    // either side has blocked the other. It FAILS CLOSED: no session, an unknown
    // owner or a malformed name all answer {"blocked":true} (still a 200, so the
    // daemon can tell it from the public 404), and only a real
    // {"blocked":false} can open a door. Read-only; nothing is written.
    if(method=="GET"&&path.rfind("/internal/blocked/",0)==0) {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        bool blocked = true;
        const std::string owner = path.substr(18);
        if (acc && valid_username(owner)) {
            auto found = g_store.find_by_username(owner);
            if (found)
                blocked = g_store.is_blocked(user, found->username) ||
                          g_store.is_blocked(found->username, user);
        }
        return http_raw("200 OK", "application/json",
            blocked ? std::string("{\"blocked\":true}") : std::string("{\"blocked\":false}"),
            "Cache-Control: no-store\r\n");
    }

    // Same-box only, same gate as /internal/blocked above. Does this username
    // exist as a LiquidGov account right now? The commons-daemon uses it to find
    // game rows whose owner is GONE (orphans: civic lets a deleted username be
    // registered again, and the new owner would otherwise inherit the old
    // player's house and inventory). No cookie and no identity: it asks only
    // about existence. It answers {"exists":false} ONLY for a well-formed name
    // that is not in the store; a malformed name answers {"exists":null}, never
    // false, so nothing weird can ever read as "gone". Read-only.
    if(method=="GET"&&path.rfind("/internal/user-exists/",0)==0) {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        const std::string who = path.substr(std::string("/internal/user-exists/").size());
        std::string exists_body = "{\"exists\":null}";
        if (valid_username(who))
            exists_body = g_store.find_by_username(who) ? "{\"exists\":true}" : "{\"exists\":false}";
        return http_raw("200 OK", "application/json", exists_body, "Cache-Control: no-store\r\n");
    }

    if(method=="GET"&&path=="/logs") {
        // is_operator(), like every other operator page: this read LIQUIDGOV_OPERATOR (singular), which the live
        // service never sets (it sets LIQUIDGOV_OPERATORS), so the page was a 404 for everyone (sweep 2026-11-09).
        if (!acc || !is_operator(user))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        return http_raw("200 OK", "text/plain; charset=utf-8", lglog::dump_text(),
            "Cache-Control: no-store\r\n");
    }
    if(method=="POST"&&path=="/log") {
        if (!acc) return http_raw("401 Unauthorized", "text/plain", "no\n");
        static std::map<std::string, long long> lg_last_ms;
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto& last = lg_last_ms[user];
        if (now_ms - last < 400)
            return http_raw("204 No Content", "text/plain", "");
        last = now_ms;
        lglog::emit("client", user + " " + lglog::clip_client(body));
        return http_raw("204 No Content", "text/plain", "");
    }

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
    if(method=="GET"&&path=="/mail/group/new") {
        if(!acc) return need_login();
        return http_redir("/mail");
    }
    // A day of caching (2026-11-08: it had no Cache-Control, so every page fetched it again; measured in headless Edge).
    // The icon only changes with a deploy, and a day-old tab icon is harmless.
    if(method=="GET"&&path=="/favicon.svg")
        return http_raw("200 OK","image/svg+xml", favicon_svg(), "Cache-Control: public, max-age=86400\r\n");
    if(method=="GET"&&path=="/me/unread") {
        if(!acc) return http_raw("401 Unauthorized","application/json","{\"n\":0,\"m\":0}");
        const int mcnt = g_store.unread_message_count(user);
        const int ncnt = unread>mcnt ? unread-mcnt : 0;
        return http_raw("200 OK","application/json",
            "{\"n\":"+std::to_string(ncnt)+",\"m\":"+std::to_string(mcnt)+"}");
    }
    if(method=="GET"&&path=="/sw.js") {
        std::ifstream in("assets/sw.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            "Service-Worker-Allowed: /\r\nCache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/assets/push.js") {
        std::ifstream in("assets/push.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/push/vapid") {
        std::ifstream in("deploy/vapid.public");
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","no vapid");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/plain; charset=utf-8", ss.str(),
            "Cache-Control: no-store\r\n");
    }
    if(method=="POST"&&path=="/push/subscribe") {
        if(!acc) return http_code("401 Unauthorized", "");
        auto endpoint = extract_json_string_field(body, "\"endpoint\"");
        auto p256dh = extract_json_string_field(body, "\"p256dh\"");
        auto authk = extract_json_string_field(body, "\"auth\"");
        if(!endpoint || !p256dh || !authk) return http_code("400 Bad Request", "bad json");
        if(!push_subs_upsert(user, trim_ws(*endpoint), trim_ws(*p256dh), trim_ws(*authk)))
            return http_code("400 Bad Request", "bad sub");
        return http_raw("200 OK","application/json","{\"ok\":true}");
    }

    if(method=="GET"&&path=="/manifest.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"LiquidGov.US\","
            "\"short_name\":\"LiquidGov\","
            "\"description\":\"A voluntary natural democracy -- civic association and working software.\","
            "\"start_url\":\"/\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#8a6620\","
            "\"icons\":[{\"src\":\"/icon-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"},{\"src\":\"/favicon.svg\",\"sizes\":\"any\",\"type\":\"image/svg+xml\",\"purpose\":\"any\"}]"
            "}");
    }

    // Linked only from a page rendered with title "Messages" (currently
    // just /mail -- see page_wrap's manifest href branch). start_url is
    // /mail, not /, so installing *from that page* on a manifest-reading
    // platform (Chrome/Edge/Android; iOS Safari ignores this file
    // entirely and has no install banner of its own) opens straight back
    // into the inbox instead of the Genesis landing page.
    if(method=="GET"&&path=="/manifest-mail.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Messages\","
            "\"short_name\":\"Messages\","
            "\"description\":\"LiquidGov.US mail and conversations.\","
            "\"start_url\":\"/mail\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#d1a85c\","
            "\"icons\":[{\"src\":\"/icon-mail-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-mail-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }

    // Same pattern as /manifest-mail.webmanifest, linked only from a page rendered with title "Social" or "Civic"
    // (the /app/social and /app/civic install shells, which /apps sends people to). start_url is the real app page,
    // exactly where each shell already sends a standalone launch (see assets/app-social.html / app-civic.html), so an
    // installed Social opens the feed and an installed Civic opens proposals instead of the Genesis landing page.
    // Icons and theme colours are the ones page_wrap already uses for those titles. (iOS ignores manifests.)
    if(method=="GET"&&path=="/manifest-social.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Social\","
            "\"short_name\":\"Social\","
            "\"description\":\"LiquidGov.US feed, people, and groups.\","
            "\"start_url\":\"/feed\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#c17a4a\","
            "\"icons\":[{\"src\":\"/icon-social-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-social-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }
    if(method=="GET"&&path=="/manifest-civic.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Civic\","
            "\"short_name\":\"Civic\","
            "\"description\":\"LiquidGov.US proposals, laws, and who holds control.\","
            "\"start_url\":\"/proposals\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#6b7d5a\","
            "\"icons\":[{\"src\":\"/icon-civic-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-civic-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }

    if(method=="GET"&&(path=="/apple-touch-icon.png"||path=="/apple-touch-icon-precomposed.png"||path=="/icon-192.png"||path=="/icon-512.png"||path=="/assets/icon-192.png"||path=="/assets/icon-512.png"||path=="/assets/apple-touch-icon.png")) {
        std::string file="assets/apple-touch-icon.png";
        if(path.find("192")!=std::string::npos) file="assets/icon-192.png";
        else if(path.find("512")!=std::string::npos) file="assets/icon-512.png";
        std::ifstream in(file, std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","image/png", ss.str(), "Cache-Control: public, max-age=86400\r\n");
    }

    if(method=="GET"&&(path=="/icon-mail.png"||path=="/icon-social.png"||path=="/icon-civic.png"
        ||path=="/icon-mail-192.png"||path=="/icon-social-192.png"||path=="/icon-civic-192.png"
        ||path=="/icon-mail-512.png"||path=="/icon-social-512.png"||path=="/icon-civic-512.png")) {
        std::ifstream in(("assets"+path), std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","image/png", ss.str(), "Cache-Control: public, max-age=86400\r\n");
    }
    if(method=="GET"&&path=="/assets/setup.js") {
        std::ifstream in("assets/setup.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/setup.css") {
        std::ifstream in("assets/setup.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/juketube.js") {
        std::ifstream in("assets/juketube.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/juketube.css") {
        std::ifstream in("assets/juketube.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/intro.mp4") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/intro.mp4", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        const std::string cache_hdr = "Cache-Control: public, max-age=31536000, immutable\r\nAccept-Ranges: bytes\r\n";
        // Real browsers request video with a Range header even on first
        // play; answering 200-whole-body every time (no 206 support) is
        // a genuine small smoothness cost, independent of anything else.
        // Falls straight through to the full 200 response on anything
        // malformed or out of bounds -- never worth 500ing a video load.
        const std::string range = header_value(req, "Range");
        if (range.rfind("bytes=", 0) == 0) {
            auto dash = range.find('-', 6);
            if (dash != std::string::npos) {
                try {
                    std::string start_s = range.substr(6, dash - 6);
                    std::string end_s = range.substr(dash + 1);
                    std::size_t start = start_s.empty() ? 0 : std::stoul(start_s);
                    std::size_t end = end_s.empty() ? cached.size() - 1 : std::stoul(end_s);
                    if (start < cached.size() && end < cached.size() && start <= end) {
                        std::string slice = cached.substr(start, end - start + 1);
                        std::ostringstream extra;
                        extra << cache_hdr << "Content-Range: bytes " << start << "-" << end << "/" << cached.size() << "\r\n";
                        return http_raw("206 Partial Content", "video/mp4", slice, extra.str());
                    }
                } catch (...) { /* fall through */ }
            }
        }
        return http_raw("200 OK", "video/mp4", cached, cache_hdr);
    }
    if(method=="GET"&&path=="/assets/og-image-v4.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v4.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // v3, kept so an old cached share still resolves. The tags now point at
    // og-image-v4.jpg (page_wrap).
    if(method=="GET"&&path=="/assets/og-image-v3.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v3.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    if(method=="GET"&&path=="/assets/og-image-v2.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v2.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // v1, kept so an old cached share of /assets/og-image.png still resolves.
    // The tags now point at og-image-v4.jpg (page_wrap).
    if(method=="GET"&&path=="/assets/og-image.png") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image.png", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/png", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // Real recorded narration (Helios) for the Chapter 1 Listen control --
    // replaces the earlier browser speechSynthesis approach entirely.
    // Narration for every Laws chapter (Helios, recorded 2026-09-13; Brandon 2026-11-09 "i want the audio"):
    // /assets/chapter<N>.mp3 for N = 1..number of chapters. Chapter 1 used to be the only route (a whole-file copy
    // held in memory for good); all 14 that way would pin ~18 MB on the 512 MB droplet, so each request reads the
    // file from disk. Byte ranges are honoured (iPhone Safari asks for them before playing audio). Cached a day,
    // not a year: a chapter whose text changes gets re-recorded at the same address.
    if(method=="GET"&&path.rfind("/assets/chapter",0)==0&&path.size()>19&&path.compare(path.size()-4,4,".mp3")==0) {
        const std::string num = path.substr(15, path.size()-15-4);
        size_t n = 0;
        if (num.empty() || num.size() > 2 || num[0] == '0' || !std::all_of(num.begin(), num.end(), ::isdigit))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        n = static_cast<size_t>(std::stoi(num));
        if (n < 1 || n > law_chapters().size()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        // the name is built, so lgp1_consistency (literal asset names) cannot see it; narration_files_check.py checks
        // that assets/chapter1..N.mp3 exist for every Laws chapter instead
        const std::string file = "assets/chapter" + num + ".mp3";
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            lglog::emit("narration", "missing assets/chapter" + num + ".mp3");
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        }
        std::ostringstream ss; ss << in.rdbuf();
        const std::string all = ss.str();
        const std::string cache = "Cache-Control: public, max-age=86400\r\nAccept-Ranges: bytes\r\n";
        const std::string range = header_value(req, "Range");
        unsigned long long a = 0, b = 0;
        if (!range.empty() && !all.empty() && std::sscanf(range.c_str(), "bytes=%llu-%llu", &a, &b) >= 1) {
            if (range.find('-') == range.size() - 1 || b >= all.size()) b = all.size() - 1;
            if (a > b || a >= all.size())
                return http_raw("416 Range Not Satisfiable", "text/plain; charset=utf-8", "",
                    "Content-Range: bytes */" + std::to_string(all.size()) + "\r\n");
            return http_raw("206 Partial Content", "audio/mpeg", all.substr(a, b - a + 1), cache +
                "Content-Range: bytes " + std::to_string(a) + "-" + std::to_string(b) + "/" + std::to_string(all.size()) + "\r\n");
        }
        return http_raw("200 OK", "audio/mpeg", all, cache);
    }

    if(method=="GET"&&(path=="/theme/dark"||path=="/theme/light")) {
        const std::string mode = path.substr(7);
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/";
        std::string extra = "Set-Cookie: lg_theme="+mode+"; Path=/; SameSite=Lax; Max-Age=31536000";
        if (g_https) extra += "; Secure";
        extra += "\r\n";
        return http_redir(back, extra);
    }

    // The old signed-in landing page (counts, your rooms), still one tap away from the map (2026-11-07).
    if(method=="GET"&&path=="/home") {
        if(!acc) return need_login();
        return http_ok(wrap("Home", user, unread, home_dashboard(user)+
                "<div class=\"card\"><h2>What this is</h2>"
                "<p class=\"muted\">Genesis, not a government. Three rules, and the rest is up to you.</p>"
                "<p class=\"muted\"><a href=\"/laws\">Read the disclosure</a></p></div>"));
    }
    if(method=="GET"&&path=="/") {
        if(acc) {
            // A returning member's "landing page" is here, not the
            // logged-out Genesis page below -- so that's where the opted-in
            // replay actually has to happen for the Settings toggle to mean
            // anything for someone who can reach Settings at all. Bypasses
            // pg() (which never shows the intro) specifically for this;
            // show_intro is otherwise only ever true for the Genesis path.
            const bool replay_intro = g_store.prefs_of(user).intro_every_visit;
            return http_ok(wrap("MAINstream", user, unread, map_page_body(user, ""), replay_intro));
        }
        std::string extra =
            "<div class=\"card\"><p class=\"row\"><a class=\"btn\" href=\"/signup\">Create an account</a> "
            "<a class=\"btn ghost\" href=\"/login\">Log in</a></p></div>";
        return http_ok(wrap("Home", user, unread, genesis_body()+extra, true));
    }
    if(method=="GET"&&path=="/laws") return pg("Laws", laws_body());
    if(method=="GET"&&path.rfind("/laws/",0)==0) {
        size_t n = 0;
        try { n = std::stoull(path.substr(6)); } catch (...) {}
        const auto& chapters = law_chapters();
        if (n < 1 || n > chapters.size())
            return http_code("404 Not Found", pg("Not found", "<div class=\"card\"><p>No such chapter.</p></div>"));
        return pg("Laws · " + chapters[n-1].title, laws_chapter_body(n));
    }
    if(method=="GET"&&path=="/control") return pg("Who holds control", control_body());
    if(method=="GET"&&path=="/privacy") return pg("Privacy", privacy_body());
    if(method=="GET"&&path=="/terms") return pg("How this association works", terms_body());
    if(method=="GET"&&path=="/about") return http_redir("/control");
    if(method=="GET"&&path=="/profile") return http_redir(user.empty() ? login_url("/me") : "/me");
    if(method=="GET"&&(path=="/install"||path=="/apps")) {
        std::ifstream in("assets/apps.html"); std::ostringstream body; if(in) body<<in.rdbuf();
        return pg("Get the app", body.str());
    }
    // The separate Messages/Social/Civic apps are gone (one app now). Their old
    // install pages send people to the real page those apps opened.
    if(method=="GET"&&path=="/app/mail") return http_redir("/mail");
    if(method=="GET"&&path=="/app/social") return http_redir("/feed");
    if(method=="GET"&&path=="/app/civic") return http_redir("/proposals");
    if(method=="GET"&&path=="/signup") return http_ok(signup_page(user, unread, "", SignupDraft{}));
    if(method=="GET"&&path=="/login") {
        // Deliberately one generic message for bad-username and
        // bad-password alike (and phone-not-verified folded in too) --
        // splitting them would let a caller enumerate which usernames
        // exist. Rate-limit is the one case safe to name specifically:
        // it doesn't depend on whether the account exists.
        const std::string err = flash_error(qs);
        const std::string next = safe_next_path(qs["next"]);
        return pg("Log in", "<div class=\"card\"><h1>Log in</h1>"
            "<p class=\"muted\">Log in with your username and password.</p>"+err+
            google_signin_button_html()+
            "<form method=\"POST\" action=\"/login\"><label>Username</label>"
            "<input name=\"username\" required autocomplete=\"username\" value=\""+html_escape(qs["username"])+"\">"
            "<label>Password</label><input type=\"password\" name=\"password\" required autocomplete=\"current-password\">"
            +(next.empty()?std::string():"<input type=\"hidden\" name=\"next\" value=\""+html_escape(next)+"\">")+
            "<button>Continue</button></form>"
            "<p class=\"muted\" style=\"margin-top:.7rem\">"
            "<a href=\"/forgot\">Forgot password?</a> &middot; "
            "New here? <a href=\"/signup\">Create an account</a>.</p></div>");
    }
    if(method=="GET"&&path=="/forgot") {
        return pg("Forgot password", "<div class=\"card\"><h1>Forgot your password?</h1>"
            "<p class=\"muted\">Used Google? <a href=\"/login\">Sign in with Google</a> instead "
            "&mdash; no reset needed.</p>"
            +flash_error(qs)+
            "<form method=\"POST\" action=\"/forgot\">"
            "<label>Email</label><input type=\"email\" name=\"email\" required autocomplete=\"email\">"
            "<button>Send reset link</button></form>"
            "<p class=\"muted\" style=\"margin-top:.7rem\"><a href=\"/login\">Back to log in</a></p></div>");
    }
    // GET renders the form regardless of whether t looks valid -- POST
    // does the actual checking (see its own comment), so this route
    // can't be used to probe token validity separately from consuming
    // one.
    if(method=="GET"&&path=="/reset") {
        return pg("Reset password", "<div class=\"card\"><h1>Set a new password</h1>"
            +flash_error(qs)+
            "<form method=\"POST\" action=\"/reset\">"
            "<input type=\"hidden\" name=\"t\" value=\""+html_escape(qs["t"])+"\">"
            "<label>New password</label><input type=\"password\" name=\"password\" required "
            "minlength=\"8\" autocomplete=\"new-password\">"
            "<button>Set password</button></form></div>");
    }
    if(method=="GET"&&path=="/logout") {
        logout(g_store, sid);
        return http_redir("/", clear_cookie());
    }

    if(method=="GET"&&path=="/world") {
        // Old Quad bookmarks land on the closed Commons page.
        return http_redir("/commons/");
    }
    if(method=="GET"&&path=="/me/canvas") {
        if(!acc) return need_login();
        return pg("Canvas", canvas_tool_page(user, flash_error(qs), false));
    }
    if(method=="GET"&&path=="/me/portrait") {
        if(!acc) return need_login();
        return pg("Self-portrait", canvas_tool_page(user, flash_error(qs), true));
    }
    if(method=="GET"&&path=="/me/post/canvas") {
        if(!acc) return need_login();
        return pg("Compose a post", canvas_tool_page(user, "", false, "/me/post"));
    }
    if(method=="GET"&&path=="/feed/canvas") {
        if(!acc) return need_login();
        return pg("Compose a post", canvas_tool_page(user, "", false, "/feed"));
    }
    if(method=="GET"&&path=="/listen") {
        // Deliberately not wrap()/pg(): this window IS the player, full-page
        // nav chrome and a second (redundant) floating-widget toggle would
        // just be noise in a small popout.
        if(!acc) return need_login();
        return http_ok(page_wrap("Listen", "",
            "<div style=\"padding:.75rem\">" + jukebox_standalone_page(youtube_api_key_from_env(), user) + "</div>"));
    }
    if(method=="GET"&&path=="/feed") {
        if(!acc) return need_login();
        std::string filter_key = qs.count("filter") ? qs.at("filter") : "all";
        FeedFilter filter = FeedFilter::All;
        if (filter_key == "friends") filter = FeedFilter::Friends;
        else if (filter_key == "committees") filter = FeedFilter::Committees;
        else filter_key = "all"; // anything else, an old ?filter=drawings link included, is All (no Drawings, Brandon 2026-11-10)
        auto tab = [&](const char* key, const char* label) {
            const bool active = filter_key == key;
            return "<a class=\"btn"+std::string(active?"\" aria-current=\"page":" ghost")+"\" href=\"/feed?filter="+key+"\">"+label+"</a>";
        };
        std::string html = "<h1>Feed</h1><p class=\"muted eyebrow\">"+html_escape(fmt_time(now()))+
            " &middot; "+std::to_string(g_store.account_count())+" members</p>"
            "<div class=\"segtabs\">"+
            tab("all","All")+tab("friends","Friends")+tab("committees","Groups")+
            "</div>"+
            flash_error(qs)+
            feed_composer_card(user);
        auto posts = g_store.feed_view(user, 80, filter);
        if (posts.empty()) html += "<div class=\"card\"><p class=\"empty\">Nothing in this view yet.</p>"
            "<p class=\"muted\">Write one above, or <a href=\"/friends\">add a friend</a> so theirs show up.</p></div>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        return pg("Feed", html);
    }

    if(method=="GET"&&path=="/proposals") {
        if(!acc) return need_login();
        // /proposals?id=N: one proposal on its own (the map's right-hand field
        // opens this), with its vote, reactions and comments.
        if (qs.count("id")) {
            std::uint64_t pid = 0;
            try { pid = std::stoull(qs.at("id")); } catch (...) { pid = 0; }
            auto one = pid ? g_store.proposal_by_id(pid) : std::nullopt;
            if (!one) return http_code("404 Not Found", wrap("Not found", user, unread,
                "<div class=\"card\"><p>No such proposal.</p><p><a href=\"/proposals\">Proposals</a></p></div>"));
            // A race is shown and voted on /officials only (Brief 7 item 1: one object, one page).
            if (one->is_office) return http_redir("/officials#proposal-" + std::to_string(one->id));
            const std::string self = "/proposals?id=" + std::to_string(one->id);
            return pg(one->title, "<p class=\"muted\"><a href=\"/proposals\">All proposals</a></p>" + flash_error(qs) +
                      "<div class=\"card\">" + proposal_block(*one, user, self) + "</div>" + card_flash_script(qs));
        }
        std::string filter_key;
        if (qs.count("city")) {
            const std::string raw = qs.at("city");
            const std::string key = city_key(raw);
            if (!raw.empty() && key.empty()) return http_redir("/proposals");
            if (!key.empty() && key != raw) return http_redir("/proposals?city="+url_encode(key));
            filter_key = key;
        }
        std::string city_value;
        if (!filter_key.empty()) {
            std::string name = filter_key;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == filter_key) { name = cu.name; break; }
            city_value = " value=\""+html_escape(name)+"\"";
        }
        const std::string list_next = filter_key.empty() ? std::string("/proposals")
            : "/proposals?city="+url_encode(filter_key);
        std::string tag_fields;
        if (!filter_key.empty()) {
            for (const auto& tag : g_store.city_tags(filter_key)) {
                tag_fields += "<label class=\"row\" style=\"gap:.4rem;align-items:center\">"
                              "<input type=\"checkbox\" name=\"tag\" value=\"" + html_escape(tag) + "\"> "
                              + html_escape(tag) + "</label>";
            }
            if (!tag_fields.empty())
                tag_fields = "<label>Departments and councils</label>" + tag_fields;
        }
        std::string html = "<h1>Proposals</h1>"
            // Brief 7 item 2 (PROPOSALS-AUDIT section 4): how to use the page, in one line. The Phase 2 link to /laws/13 sent
            // members into charter text; it stays in Laws and on /control.
            + how_proposals_work_html();
        html += "<p class=\"muted\"><a href=\"/officials\">Races for office</a> are on their own page.</p>";
        if (!filter_key.empty()) {
            std::string name = filter_key;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == filter_key) { name = cu.name; break; }
            html += "<p class=\"muted\">Showing proposals for <a href=\"/city/"+url_encode(filter_key)+"\">"
                    +html_escape(name)+"</a>. <a href=\"/proposals\">All proposals</a></p>";
        }
        html += flash_error(qs);
        // ?title= prefills the title ("Start a proposal about this" from /problems). Never posts by itself.
        std::string title_prefill = qs.count("title") ? trim_ws(qs.at("title")) : std::string();
        if (title_prefill.size() > 140) title_prefill.resize(140);
        auto all = filter_key.empty() ? g_store.all_proposals() : g_store.proposals_in_city(filter_key);
        std::reverse(all.begin(), all.end());
        // Races for office are listed on /officials only, not twice (Brief 7 item 1); a link below says where they are.
        all.erase(std::remove_if(all.begin(), all.end(), [](const Proposal& x) { return x.is_office; }), all.end());
        // The proposals first, the form behind a "New proposal" button (sweep 2026-11-09: on a phone the page opened
        // on an empty form and the proposals began a full screen down). Open on its own when it is what you came for:
        // a ?title= prefill, an error to fix, nothing to read yet, or a link to #new (the script below).
        // Only a create error opens it; a refused answer or option (err=vote/option/optfull) is about a card below.
        const std::string err_code = qs.count("err") ? qs.at("err") : std::string();
        const bool create_err = err_code == "empty" || err_code == "range" || err_code == "options" ||
                                err_code == "dupe" || err_code == "pcity";
        const bool open_form = !title_prefill.empty() || create_err || all.empty();
        html += std::string("<details class=\"card newprop\" id=\"new\"") + (open_form ? " open" : "") +
                "><summary>New proposal</summary><form method=\"POST\" action=\"/proposals\">"
                "<label>Title</label><input name=\"title\" maxlength=\"140\" required value=\""+html_escape(title_prefill)+"\">"
                "<label>What's being proposed</label>"
                "<textarea name=\"body\" maxlength=\"4000\" required placeholder=\"Plain text.\"></textarea>"
                "<label>How should people answer?</label>"
                "<select name=\"kind\" id=\"bkind\">"
                "<option value=\"0\">Yes, no, or skip</option>"
                "<option value=\"1\">A number</option>"
                "<option value=\"2\">A date</option>"
                "<option value=\"3\">Pick one option</option>"
                "<option value=\"4\">A scale (0–100)</option>"
                "</select>"
                "<div id=\"bnum\" hidden>"
                "<label>Unit (optional)</label><input name=\"unit\" maxlength=\"24\" placeholder=\"days, dollars, people\">"
                "<label>Minimum (optional)</label><input name=\"min\" type=\"number\" step=\"any\">"
                "<label>Maximum (optional)</label><input name=\"max\" type=\"number\" step=\"any\">"
                "</div>"
                "<div id=\"bchoice\" hidden>"
                "<label>Options (one per line)</label>"
                "<textarea name=\"options\" maxlength=\"800\" placeholder=\"One option per line\"></textarea>"
                "<label class=\"row\" style=\"gap:.4rem;align-items:center;margin-top:.4rem\">"
                "<input type=\"checkbox\" name=\"open\" value=\"1\"> "
                "Let people add another option <span class=\"muted\">(one each, besides you)</span></label>"
                "</div>"
                "<div id=\"bscale\" hidden>"
                "<label>Left label</label><input name=\"left\" maxlength=\"40\" placeholder=\"Too little\">"
                "<label>Right label</label><input name=\"right\" maxlength=\"40\" placeholder=\"Too much\">"
                "</div>"
                "<label>City (optional)</label>"
                "<input name=\"city\" list=\"citylist\" maxlength=\"80\" autocomplete=\"off\" placeholder=\"Fresno, CA\""
                + city_value + ">"
                "<label>Who could act on this? <span class=\"muted\">(optional, for example &ldquo;The city council&rdquo;)</span></label>"
                "<input name=\"who\" maxlength=\"80\" placeholder=\"The city council\">"
                + tag_fields +
                "<button>Publish</button></form>"
                + city_datalist_html() +
                "<script>"
                "(function(){var s=document.getElementById('bkind');if(!s)return;"
                "function show(){var v=s.value;"
                "document.getElementById('bnum').hidden=(v!=='1');"
                "document.getElementById('bchoice').hidden=(v!=='3');"
                "document.getElementById('bscale').hidden=(v!=='4');}"
                "s.addEventListener('change',show);show();})();"
                "(function(){var d=document.getElementById('new');if(d&&location.hash==='#new')d.open=true;})();"
                "</script></details>";
        if (all.empty()) html += "<div class=\"card\"><p class=\"empty\">None open.</p>"
            "<p class=\"muted\">The form above starts an unofficial straw poll. Not a binding vote.</p></div>";
        for (const auto& p : all) {
            html += "<div class=\"card\">"+proposal_block(p, user, list_next)+"</div>";
        }
        html += card_flash_script(qs);
        return pg("Proposals", html);
    }

    if(method=="GET"&&path.rfind("/portrait/",0)==0) {
        const std::string who = path.substr(10);
        if (!valid_username(who)) return http_code("404 Not Found", "not found");
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto av = g_store.get(found->id);
        if (!av) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto bytes = g_blobs.get(av->blob_id);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"portrait.png\"\r\n");
    }

    // /canvas-frame/<username>/latest -- the newest saved snapshot, for the
    // commons wall slot (glue.md "Wall-slot bitmap"). A separate path token,
    // registered ahead of the index route below on purpose: that handler
    // std::stoi()s the last segment, so "latest" must never reach it (it
    // would 404 today, and any looser parse would read as frame 0 -- the
    // first stroke, often near-blank). Same checks as the index route
    // (unknown user, the viewer's own block check -- anonymous skips it --
    // no frames, missing blob); every one is a plain 404, which the game
    // reads as "empty wall slot", not a broken image. Read-only.
    if(method=="GET"&&path.rfind("/canvas-frame/",0)==0&&path.size()>21&&
       path.compare(path.size()-7,7,"/latest")==0) {
        const std::string who = path.substr(14, path.size()-14-7);
        std::string body404 = "not found";
        if (!valid_username(who)) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto frames = g_store.canvas_frames(found->id);
        if (frames.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto bytes = g_blobs.get(frames.back());
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"frame.png\"\r\n");
    }

    // /canvas-frame/<username>/<index> -- one saved drawing snapshot.
    // Requested by the profile page's own background-image CSS
    // (server-generated, not user CSS -- see profile_canvas_css), and
    // by /me/canvas itself to reload a saved drawing back onto the
    // editing layer -- public either way, same as /portrait/.
    if(method=="GET"&&path.rfind("/canvas-frame/",0)==0) {
        auto rest = path.substr(14);
        auto slash = rest.find('/');
        std::string body404 = "not found";
        if (slash == std::string::npos) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        const std::string who = rest.substr(0, slash);
        int idx = -1;
        try { idx = std::stoi(rest.substr(slash + 1)); } catch (...) { idx = -1; }
        if (!valid_username(who) || idx < 0) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto frames = g_store.canvas_frames(found->id);
        if (static_cast<std::size_t>(idx) >= frames.size()) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto bytes = g_blobs.get(frames[static_cast<std::size_t>(idx)]);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"frame.png\"\r\n");
    }

    // /post-media/<blob_id> -- an image or video attached to a post.
    // Public and unauthenticated, same posture as /portrait/ and
    // /canvas-frame/: a post's own visibility is already decided by
    // wherever it renders (feed, group, profile), and the blob id itself
    // is a content hash, not a secret. blob_id comes straight from the
    // URL, though, so it's validated as a bare hex sha256 digest before
    // ever reaching BlobStore -- FilesystemBlobStore::path_for()
    // concatenates the id directly into a filesystem path with no
    // sanitization of its own, so anything looser here would be a real
    // path-traversal hole, not just a style nit.
    if(method=="GET"&&path.rfind("/post-media/",0)==0) {
        const std::string blob_id = path.substr(12);
        bool valid_hex = blob_id.size() == 64;
        if (valid_hex) {
            for (unsigned char c : blob_id) {
                if (!std::isxdigit(c)) { valid_hex = false; break; }
            }
        }
        if (!valid_hex) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto meta = g_blobs.meta(blob_id);
        auto bytes = g_blobs.get(blob_id);
        if (!meta || !bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string data(bytes->begin(), bytes->end());
        return http_raw("200 OK", meta->mime_type.c_str(), data,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline\r\n");
    }

    if(method=="GET"&&path=="/me") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html;
        // No Publish/Discard bar any more (Brandon 2026-11-11 4:50 PM, Brief 4 item 11): every form[data-draft] here
        // saves itself, see draft_publish_script.
        html += "<div class=\"card\"><div class=\"who\">"+portrait_tag(user,"")+"<div><h1>"+html_escape(user)+"</h1>"
            "<p class=\"muted\">Platform profile · <a href=\"/u/"+html_escape(user)+"\">public page</a> · member since "+
            html_escape(fmt_time(acc->created_at))+"</p></div></div>"+err+
            "<p class=\"muted\">Self-portrait &mdash; draw one with the same tools as the "
            "<a href=\"/me/portrait\">portrait tool</a>.</p>";
        html += "<p class=\"muted\">A platform account is not a citizen identity.</p></div>";
        {
            auto prof = g_store.profile_of(user);
            auto bio = g_store.blurb(acc->id);
            html += "<div class=\"card\"><h2>About you</h2>"
                "<form method=\"POST\" action=\"/me/profile\" data-draft=\"1\">"
                "<label>Display name</label><input name=\"display_name\" required maxlength=\"80\" autocomplete=\"nickname\" value=\""+html_escape(prof.display_name)+"\">"
                "<label>Hometown</label><input name=\"hometown\" maxlength=\"80\" value=\""+html_escape(prof.hometown)+"\">"
                "<label>Where you live now</label><input name=\"living\" maxlength=\"80\" value=\""+html_escape(prof.living)+"\">"
                "<label>Bio</label><textarea name=\"bio\" maxlength=\"500\">"+html_escape(bio)+"</textarea>"
                "<label>Interests</label><input name=\"interests\" maxlength=\"200\" placeholder=\"comma-separated\" value=\""+html_escape(prof.interests)+"\">"
                "<label>Career</label><input name=\"career\" maxlength=\"80\" value=\""+html_escape(prof.career)+"\">"
                "<label>School</label><input name=\"school\" maxlength=\"80\" value=\""+html_escape(prof.school)+"\">"
                "</form>"
                "<p class=\"muted\">Email stays a fingerprint from signup. It is not shown and cannot be changed here.</p></div>";
        }
        {
            auto n_frames = g_store.canvas_frames(acc->id).size();
            std::string canvas_status = n_frames == 0
                ? "No drawing yet &mdash; your profile uses the default background."
                : (std::to_string(n_frames) + " frame" + (n_frames==1?"":"s") + " saved, "
                   + (g_store.canvas_timelapse(acc->id) ? "looping as your background." : "last one shown as your background."));
            html += "<div class=\"card\"><h2>Canvas</h2>"
                "<p class=\"muted\">"+canvas_status+"</p>"
                "<p class=\"row\"><a class=\"btn\" href=\"/me/canvas\">Open the canvas</a></p></div>";
        }
        {
            auto current_css = g_store.author_css(acc->id);
            std::string css_msg;
            // The page-top banner (see `err` above) already covers ?err=css
            // -- this is just the success case, which has no other home.
            if (qs["ok"]=="css") css_msg = "<p class=\"ok\">Saved. See it at <a href=\"/u/"+html_escape(user)+"\">your public profile</a>.</p>";
            html += "<div class=\"card\"><h2>Profile CSS</h2>"
                "<p class=\"muted\">Restyles your public profile page (/u/"+html_escape(user)+
                "), shown inside a sandboxed frame that can never run scripts. "
                "@import, url(), and anything script-adjacent are rejected outright, not silently stripped. "
                "Limit " + std::to_string(kAuthorCssMaxBytes/1000) + "KB. Leave empty to use the default look.</p>"
                + css_msg +
                "<form method=\"POST\" action=\"/me/css\" data-draft=\"1\">"
                "<textarea name=\"css\" rows=\"8\" spellcheck=\"false\" "
                // 16px, not .85rem: under 16px iOS Safari zooms the page in on focus and leaves it zoomed (2026-11-07)
                "style=\"font-family:ui-monospace,Consolas,monospace;font-size:16px\" "
                "placeholder=\".lg-theme h1{color:#a23b3b}\">"+html_escape(current_css)+"</textarea>"
                "</form></div>";
        }
        html += jukebox_widget(user, true, user, qs);
        {
            // Off by default -- visiting a profile shouldn't ambush anyone
            // with sound. "Play this Juketube instead" is stored as-is;
            // an invalid value (self, a stranger who blocked you, a cycle)
            // is never rejected here, just quietly inert at read time (see
            // jukebox_effective_owner) -- same posture as findability
            // elsewhere: a miss looks like a miss, not an error message.
            std::string jmsg = qs["ok"]=="jplay" ? "<p class=\"ok\">Saved.</p>" : "";
            html += "<div class=\"card\"><h2>Play settings</h2>"+jmsg+
                "<form method=\"POST\" action=\"/me/jukebox/play-settings\" data-draft=\"1\">"
                "<label><input type=\"checkbox\" name=\"autoplay\" value=\"1\""+
                (g_store.jukebox_autoplay(user) ? " checked" : "")+
                "> Play my Juketube when someone opens my profile</label>"
                "<label style=\"display:block;margin-top:.5rem\">Play this Juketube instead</label>"
                "<input name=\"relay\" placeholder=\"another person&rsquo;s Juketube, or empty for your own\" autocomplete=\"off\" "
                "value=\""+html_escape(g_store.jukebox_relay_target(user))+"\">"
                "</form></div>";
        }
        html += draft_publish_script();
        return pg("Profile", html);
    }

    // Checked before the general /u/ handler below, since that one treats
    // everything after "/u/" as a bare username.
    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>7&&path.compare(path.size()-6,6,"/theme")==0) {
        const std::string who = path.substr(3, path.size()-3-6);
        std::string body;
        if (!valid_username(who)) {
            body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                   "<body><p>No such profile.</p></body></html>";
        } else {
            auto found = g_store.find_by_username(who);
            if (!found) {
                body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                       "<body><p>No such profile.</p></body></html>";
            } else {
                auto author_css = g_store.author_css(found->id);
                // Deliberately not wrap()/page_wrap(): no nav, no jukebox
                // widget, no session cookie context, no scripts at all --
                // this document only ever renders inside the sandboxed,
                // scriptless iframe below. See sanitize_author_css's
                // comment for what's actually defended here and why a gap
                // in the text filter still can't reach anything.
                // No canvas background here anymore -- a profile-background
                // drawing renders directly on the real /u/<username> page
                // now (see profile_canvas_css), not through this document.
                // This route/iframe exists purely for a custom .lg-theme
                // author_css, so it's only ever reached when that's set.
                body = "<!doctype html><html><head><meta charset=\"utf-8\">"
                       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                       "<style>body{margin:0;padding:1rem;font:16px/1.5 system-ui,sans-serif;"
                       "background:#fff;color:#1a1814}h1{margin:.2rem 0}"
                       "p{white-space:pre-wrap}</style>"
                       "<style>" + author_css + "</style></head>"
                       "<body><div class=\"lg-theme\"><h1>" + html_escape(found->username) + "</h1>"
                       "</div></body></html>";
            }
        }
        return http_raw("200 OK", "text/html; charset=utf-8", body);
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
    if(method=="GET"&&path.rfind("/u/",0)==0) {
        const std::string who = path.substr(3);
        if (!valid_username(who))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        auto found = g_store.find_by_username(who);
        if (!found)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        if (acc && g_store.is_blocked(user, found->username))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        const bool is_owner = acc && acc->username == found->username;
        const bool is_friend = acc && !is_owner && g_store.are_friends(user, found->username);
        auto target_prefs = g_store.prefs_of(found->username);
        if (!visible_to(target_prefs.profile_visibility, is_owner, acc.has_value(), is_friend)) {
            return http_code("200 OK", wrap(found->username, user, unread,
                "<div class=\"card\"><p>This profile is private.</p></div>"));
        }
        auto committees = g_store.communities_of(found->username);
        std::string committees_html;
        if (!committees.empty()) {
            committees_html = "<p class=\"eyebrow\" style=\"margin-top:1rem\">Groups</p>"
                               "<div style=\"display:flex;flex-wrap:wrap;gap:.4rem\">";
            for (const auto& c : committees) {
                committees_html += "<a class=\"chip\" href=\"/c/"+html_escape(c.slug)+"\" style=\"text-decoration:none\">"+
                                    html_escape(c.name)+"</a>";
            }
            committees_html += "</div>";
        }
        // A real button, centered at the top of the page -- not the old
        // underlined text link buried under the portrait, easy to miss
        // and inconsistent with every other primary action on the site.
        // Owner-only: is_owner is the same check the rest of this page
        // already uses to decide what a stranger versus the account
        // itself gets to see here.
        std::string html = !is_owner ? "" :
            "<p class=\"row\" style=\"justify-content:center;margin:0 0 1rem\">"
            "<a class=\"btn\" href=\"/me\">Edit profile</a></p>";
        // A profile-background drawing is the identity card's own
        // background now, not a separate sandboxed iframe (that iframe
        // used to show a near-empty white box -- just a duplicate
        // username heading plus the drawing bleeding through -- for
        // anyone with frames but no custom theme). #profilecard only
        // gets the id (and the <style> block only gets emitted) when
        // there's actually a drawing to show.
        auto canvas_frame_ids = g_store.canvas_frames(found->id);
        std::string card_css = profile_canvas_css(found->username, canvas_frame_ids.size(),
                                                    g_store.canvas_timelapse(found->id));
        html += card_css.empty() ? "" : "<style>"+card_css+"</style>";
        html += std::string("<div class=\"card\"") + (canvas_frame_ids.empty() ? "" : " id=\"profilecard\"") +
            "><div class=\"who\">"+portrait_tag(found->username,"")+
            "<div><h1>"+html_escape(found->username)+"</h1>";
        {
            auto pr = g_store.profile_of(found->username);
            if (!pr.display_name.empty())
                html += "<p class=\"lede\" style=\"margin:.2rem 0 .4rem\">"+html_escape(pr.display_name)+"</p>";
            auto bio = g_store.blurb(found->id);
            std::string bits;
            if (!pr.living.empty()) bits += html_escape(pr.living);
            if (!pr.hometown.empty()) bits += (bits.empty()?"":" · ")+std::string("from ")+html_escape(pr.hometown);
            if (!pr.career.empty()) bits += (bits.empty()?"":" · ")+html_escape(pr.career);
            if (!pr.school.empty()) bits += (bits.empty()?"":" · ")+html_escape(pr.school);
            if (!bits.empty()) html += "<p class=\"muted\">"+bits+"</p>";
            if (!bio.empty()) html += "<p>"+html_escape(bio)+"</p>";
            if (!pr.interests.empty()) html += "<p class=\"muted\">"+html_escape(pr.interests)+"</p>";
        }
        html +=
            "<table class=\"record\">"
            "<tr><td>Standing</td><td>Associate &middot; good</td></tr>"
            "<tr><td>Enrolled</td><td>"+html_escape(fmt_time(found->created_at))+"</td></tr>"
            "</table></div></div>"+committees_html;
        if (!g_store.author_css(found->id).empty()) {
            // Custom theme only -- a profile-background drawing renders
            // directly on this page now (see profile_canvas_css above),
            // not through this iframe at all.
            // sandbox="allow-same-origin", not sandbox="" (opaque origin):
            // confirmed live that opaque-origin sandboxed iframes get
            // silently dropped by real-world browser privacy/tracking
            // protections (reproduced in two independent browsers, not a
            // one-off) -- the iframe element rendered but its document
            // never loaded. allow-same-origin fixes that. This does NOT
            // reopen the security hole it looks like: allow-scripts is
            // still withheld, so nothing can ever execute inside this
            // frame regardless of origin -- and with zero script
            // execution possible, having the same origin as the rest of
            // the site is not exploitable (there's no JS to act on it).
            // The two real defenses -- sanitize_author_css rejecting
            // script-adjacent CSS, and no script execution ever being
            // possible in this frame -- are both still fully intact.
            html += "<iframe src=\"/u/"+html_escape(found->username)+"/theme\" sandbox=\"allow-same-origin\" "
                    "title=\"Profile theme\" "
                    "style=\"width:100%;min-height:320px;border:1px solid var(--rule);"
                    "border-radius:var(--rsm);background:#fff\"></iframe>";
        }
        if (is_owner) {
            // Edit profile is now the centered button at the top of the
            // page (see above) -- nothing further needed here.
        } else if (acc && g_store.are_friends(user, found->username)) {
            html += "<p class=\"row\"><a class=\"btn\" href=\"/mail?with="+html_escape(found->username)+"\">Message</a>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\" "
                    "onsubmit=\"return confirm('Unfriend "+html_escape(found->username)+"? You will leave every group you share with them. They stay in those groups.');\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(found->username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form></p>";
        } else if (acc) {
            html += "<form method=\"POST\" action=\"/friends\"><input type=\"hidden\" name=\"who\" value=\""+
                    html_escape(found->username)+"\"><button>Send friend request</button></form>";
        }
        html += "</div>";
        html += jukebox_widget(found->username, is_owner, user, qs);
        // "Play my Juketube when someone opens my profile" -- off by
        // default. Signed-in visitors only (the floating widget doesn't
        // exist for a logged-out guest, so there'd be nothing to tune),
        // and never for the owner's own visit to their own page --
        // is_owner is already computed above for exactly this branch.
        if (acc && !is_owner && g_store.jukebox_autoplay(found->username)) {
            // wrap() appends the floating widget's own markup+script right
            // after </main> (see its comment -- keeps the widget alive
            // across soft-nav, which only ever touches <main>'s innerHTML),
            // which means __lgTuneStation isn't defined yet at the point
            // this inline script -- inside <main> -- actually runs.
            // setTimeout(0) pushes the call past the rest of this initial
            // parse, by which point the widget's script further down the
            // document has already executed and defined it.
            html += "<script>(function(){"
                    "setTimeout(function(){"
                    "if(window.__lgTuneStation)window.__lgTuneStation('u:"+found->username+"');"
                    "},0);"
                    "})();</script>";
        }
        {
            // A post's own audience (the /feed "Anybody"/"Friends"/"Groups"
            // chips) gates it here, not the account-wide posts_visibility
            // setting -- that blanket still governs only a GroupsOnly post
            // shown on this list (unchanged; a community's real audience is
            // its membership, this is just "does this profile show its
            // posts at all" for those).
            auto theirPosts = g_store.posts_by(found->username);
            std::reverse(theirPosts.begin(), theirPosts.end());
            std::vector<Post> shown;
            for (const auto& p : theirPosts) {
                bool ok;
                if (is_owner) {
                    ok = true;
                } else if (p.audience == PostAudience::GroupsOnly) {
                    ok = visible_to(target_prefs.posts_visibility, is_owner, acc.has_value(), is_friend);
                } else if (p.audience == PostAudience::Anybody) {
                    ok = std::find(p.exceptions.begin(), p.exceptions.end(), user) == p.exceptions.end();
                } else { // Friends, possibly also posted into a community
                    ok = is_friend && target_prefs.posts_visibility != Visibility::OnlyMe &&
                         (p.friend_allowlist.empty() ||
                          std::find(p.friend_allowlist.begin(), p.friend_allowlist.end(), user) !=
                              p.friend_allowlist.end());
                }
                if (ok) shown.push_back(p);
            }
            if (is_owner || !shown.empty()) {
                html += "<h2>Posts</h2><p class=\"muted eyebrow\">"+std::to_string(shown.size())+
                        (shown.size()==1?" entry":" entries")+"</p>";
                for (const auto& p : shown) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
            }
        }
        return pg(found->username, html);
    }

    if(method=="GET"&&path=="/friends") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html = "<h1>People</h1>"+err+ people_suggestions_html(user, 12, "/friends") +
            "<div class=\"card\"><h2>Find someone</h2>"
            "<form method=\"GET\" action=\"/search\"><label>Type a name</label>"
            "<div class=\"row\"><input name=\"q\" id=\"peopleq\" autocomplete=\"off\" "
            "placeholder=\"Starts matching as you type\"><button>Search</button></div></form>"
            "<div id=\"peoplehits\"></div>"
            "<p class=\"muted\">Friends always appear. Everyone else follows their findability. "
            "Mail is friends-only until they accept.</p></div>"
            + people_search_script() +
            "<div class=\"card\"><h2>Add by exact name</h2>"
            "<form method=\"POST\" action=\"/friends\"><input name=\"who\" placeholder=\"account name\" autocomplete=\"off\" required>"
            "<button>Send request</button></form></div>";
        html += "<div class=\"card\"><h2>Friends</h2>";
        auto pals = g_store.friends_of(user);
        auto inner = g_store.inner_of(user);
        std::set<std::string> inner_set(inner.begin(), inner.end());
        html += "<p class=\"muted eyebrow\">"+std::to_string(pals.size())+
                (pals.size()==1?" friend":" friends")+" &middot; "+std::to_string(inner_set.size())+" inner</p>";
        if(pals.empty()) html += "<p class=\"empty\">No friends yet. Search a username above, or send a request by exact name.</p>";
        for (auto& other : pals) {
            const bool inn = inner_set.count(other);
            html += "<div class=\"person\"><div class=\"who\">"+portrait_tag(other,"sm")+"<div><a href=\"/u/"+html_escape(other)+"\"><strong>"+html_escape(other)+"</strong></a>"
                    +(inn?" <span class=\"muted\">inner circle</span>":"")+
                    "</div></div><div class=\"row\">"
                    "<a class=\"btn ghost\" href=\"/mail?with="+html_escape(other)+"\">Mail</a>";
            if (inn) {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner-remove\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">From inner</button></form>";
            } else {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">Inner circle</button></form>";
            }
            html += "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\" "
                    "onsubmit=\"return confirm('Unfriend this person? You will leave every group you share with them. They stay in those groups.');\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/block\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"danger\" type=\"submit\">Block</button></form></div></div>";
        }
        html += "<p class=\"muted\">Inner circle is up to twelve people. Everyone else you've accepted is still a friend.</p></div>";
        html += "<div class=\"card\"><h2>Incoming</h2>";
        auto inc = g_store.incoming_requests(user);
        if(inc.empty()) html += "<p class=\"empty\">None.</p>";
        // a face and a link on every request, Accept and Decline side by side (Brandon 2026-11-10: friendlier)
        auto who_html = [](const std::string& n) {
            return "<div class=\"who\">" + portrait_tag(n, "sm") + "<a href=\"/u/" + html_escape(n) + "\"><strong>" +
                   html_escape(n) + "</strong></a></div>";
        };
        for (auto& from : inc) {
            html += "<div class=\"person\">" + who_html(from) + "<div class=\"row\">"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button type=\"submit\">Accept</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/decline\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button class=\"ghost\" type=\"submit\">Decline</button></form>"
                    "</div></div>";
        }
        html += "</div><div class=\"card\"><h2>Outgoing</h2>"
                "<p class=\"muted\">If someone does not accept, you will not get a notice. That is intentional.</p>";
        auto outg = g_store.outgoing_requests(user);
        if(outg.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& to : outg) html += "<div class=\"person\">" + who_html(to) + "<span class=\"muted\">waiting</span></div>";
        html += "</div>";
        return pg("People", html);
    }

    if(method=="GET"&&path=="/search") {
        if(!acc) return need_login();
        auto term = trim_ws(qs["q"]);
        if (qs["partial"]=="1")
            return http_raw("200 OK", "text/html; charset=utf-8", people_results_html(user, term),
                "Cache-Control: no-store\r\n");
        std::string html = "<h1>Search</h1><div class=\"card\">"
            "<form method=\"GET\" action=\"/search\"><label>Find a person</label>"
            "<div class=\"row\"><input name=\"q\" id=\"peopleq\" value=\""+html_escape(term)+"\" autocomplete=\"off\">"
            "<button>Search</button></div></form>"
            "<p class=\"muted\">Friends always appear. Everyone else follows their findability. "
            "A miss is a miss &mdash; missing, opted out, and blocked look the same. "
            "Mail is friends-only until they accept.</p>"
            "<div id=\"peoplehits\">"+people_results_html(user, term)+"</div></div>"
            + people_search_script();
        return pg("Search", html);
    }

    // Where nginx sends a /mail/big upload it refused before reading it.
    if(method=="GET"&&path=="/mail/too-large") {
        std::string why = "That file is bigger than you can send.";
        if (!acc || !program_policy_for(user)) why = "Only an admin, or someone an admin allowed, can send programs and large files.";
        if (qs["status"] == "429") why = "Too many uploads at once. Wait a minute, then send it again.";
        else if (!qs["status"].empty() && qs["status"] != "413") why = "It wasn't sent (the site didn't answer). Try again in a moment.";
        return pg("Not sent", "<div class=\"card\"><h1>Not sent</h1><p>" + why + "</p>"
                  "<p><a href=\"/mail\">Back to mail</a></p></div>");
    }

    if(method=="GET"&&path=="/mail") {
        if(!acc) return need_login();
        g_store.mark_notices_read_of_type(user, "message");
        std::string thread = mail_thread_html(user, qs["with"], qs);
        if (!qs["with"].empty() && thread.empty()) {
            thread = "<div class=\"card\"><p class=\"warn\">Mail is friends-only until they accept. "
                     "<a href=\"/search?q="+html_escape(qs["with"])+"\">Find "+html_escape(qs["with"])+"</a></p></div>";
        }
        // Brief 4 item 8 (Brandon 2026-11-11 4:37 PM): the heading matches the nav ("Messages", not "Mail"), and the
        // age-assurance note is one line plus "more" so the conversation starts higher up. .mailpage scopes the layout
        // (list beside the thread on wide screens, one scroll region, composer pinned; see css()).
        std::string html = "<div class=\"mailpage\"><h1>Messages</h1>"
            "<div class=\"muted mailage\">Direct messages are friends-only for now. "
            "<details class=\"mailmore\"><summary>more</summary>"
            "Open messaging waits on age assurance so a stranger cannot reach a minor. We will not treat a checkbox as age."
            "</details></div>";
        if (qs["skipped"].size()) html += "<p class=\"warn\">Couldn’t include: "+html_escape(qs["skipped"])+
            ". Unknown, blocked, and contact-settings conflicts are skipped — start a new thread with just them if you still want to talk.</p>";
        html += "<div class=\"card mailwidget"+std::string(thread.empty() ? "" : " hasthread")+"\">"+thread+mail_inbox_html(user, acc->id)+"</div></div>";
        // No separate "Messages" app any more (one app), so no "add Messages
        // to your Home Screen" nudge here; mail_install_hint() is unused.
        return pg("Messages", html);
    }
    if(method=="GET"&&path=="/mail/panel") {
        if(!acc) return http_code("401 Unauthorized", "");
        g_store.mark_notices_read_of_type(user, "message");
        std::string thread = mail_thread_html(user, qs["with"], qs);
        std::string html = thread+mail_inbox_html(user, acc->id);
        return http_ok(html);
    }

    if(method=="GET"&&path.rfind("/mail/attachment/",0)==0) {
        if(!acc) return need_login();
        const std::string blob_id = path.substr(17);
        auto found = g_store.find_mail_attachment(user, blob_id);
        if (!found) {
            auto gfound = g_store.find_group_attachment(acc->id, blob_id);
            if (!gfound) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
            return mail_attachment_response(blob_id,
                gfound->attachment_mime.empty() ? "application/octet-stream" : gfound->attachment_mime,
                attachment_name_key(*gfound));
        }
        const std::string other = (found->from == user) ? found->to : found->from;
        // Same gate as the thread view: attachments do not outlive the
        // friendship they were sent inside of. Do not open stranger DMs.
        if (!g_store.are_friends(user, other) || g_store.is_blocked(user, other) || g_store.is_blocked(other, user))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        // (the same bytes sent twice under two names: the download is named after the message the store finds)
        return mail_attachment_response(blob_id, found->attachment_mime.value_or("application/octet-stream"),
                                        attachment_name_key(*found));
    }

    if(method=="GET"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return need_login();
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(path.substr(8)); } catch(...) { conv_id = 0; }
        auto conv = conv_id ? g_store.get_conversation(conv_id) : std::nullopt;
        const ConversationMember* self_member = nullptr;
        if (conv) for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        if (!conv || !conv->is_group || !self_member)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        g_store.mark_thread_read(user, "g:" + std::to_string(conv_id));
        const std::string base = "/mail/g/" + std::to_string(conv_id);
        const bool active = self_member->state == MemberState::Active;

        std::string html = "<p><a href=\"/mail\">\xE2\x86\x90 Mail</a></p>"
            "<h1>"+html_escape(group_label(*conv, acc->id))+"</h1>"
            "<p class=\"muted\">Platform group conversation. Not an AVND locality.</p>";
        if (qs["err"]=="invite") html += "<p class=\"warn\">Couldn’t add them — conflicts with someone’s contact settings. "
            "Start a new thread with just them if you still want to talk.</p>";
        if (!qs["skipped"].empty()) html += "<p class=\"warn\">Couldn’t include: "+html_escape(qs["skipped"])+". "
            "Unknown, blocked, and contact-settings conflicts are skipped.</p>";
        if (qs["err"]!="invite") html += mail_attachment_error_html(qs["err"], user);

        if (!active) {
            html += "<div class=\"mailpending\"><p><strong>Waiting for you to join.</strong> "
                    "You can’t send until you accept. Declining is never announced to the room.</p>"
                    "<form method=\"POST\" action=\""+base+"/accept\"><button>Accept</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\"><button class=\"ghost\">Decline</button></form></div>";
        }

        html += "<div class=\"card\">";
        auto msgs = g_store.messages_in(conv_id);
        html += "<div class=\"msglist\" id=\"msglist\" style=\"max-height:none\">";
        bool any = false;
        for (auto& m : msgs) {
            if (m.deletion==DeletionState::DeletedForSender && m.sender==acc->id) continue;
            any = true;
            const bool mine = m.sender==acc->id;
            html += "<div class=\"msg "+std::string(mine?"mine":"theirs")+"\"><div class=\"bubble\">";
            if (!mine) html += "<strong>"+html_escape(display_name_for(m.sender))+"</strong> ";
            if (m.deletion==DeletionState::DeletedForEveryone) {
                html += "<em class=\"muted\">Message deleted.</em>";
            } else {
                if (!m.text.empty()) html += html_escape(m.text);
                if (!m.attachment_blob_id.empty())
                    html += mail_attachment_html(m.attachment_blob_id, m.attachment_mime, attachment_name_key(m),
                                                 display_name_for(m.sender), mine);
                if (m.edited_at>0) html += "<span class=\"muted\"> (edited)</span>";
            }
            html += "<span class=\"bubtime\">"+html_escape(fmt_time(m.sent_at))+"</span></div></div>";
        }
        if (!any) html += "<p class=\"empty\">No messages yet.</p>";
        html += "</div>";
        if (active) {
            html += "<form method=\"POST\" action=\""+base+"/send\" enctype=\"multipart/form-data\" class=\"mailcomposer\" id=\"mailcomposer\""
                    + std::string(kComposerOnSubmit) + ">"
                    "<div class=\"row\" style=\"align-items:flex-end;gap:.4rem;flex-wrap:nowrap\">"
                    "<textarea name=\"text\" id=\"mailtext\" maxlength=\"2000\" rows=\"2\" placeholder=\"Message\" "
                    "style=\"flex:1;min-width:0;min-height:44px;resize:vertical\"></textarea>"
                    "<label class=\"mailattbtn\" title=\"Attach a file\">"
                    + mail_file_input_html(user, "/mail/big/g/" + std::to_string(conv_id) + "/send") +
                    "<span aria-hidden=\"true\">\xF0\x9F\x93\x8E</span></label>"
                    "<button type=\"submit\">Send</button></div>"
                    + std::string(kComposerFileLine) +
                    "<p class=\"muted\" style=\"margin:.3rem 0 0\">Enter sends. Shift+Enter is a new line.</p>"
                    + mail_program_note_html(user) + "</form>";
        }
        html += "</div>";

        html += "<details class=\"card\"><summary>Members</summary><ul class=\"plain\">";
        for (auto& m : conv->members) {
            html += "<li>"+html_escape(display_name_for(m.account))+
                    (m.state==MemberState::Pending?" <span class=\"muted\">invited \xE2\x80\x94 not in yet</span>":"")+"</li>";
        }
        html += "</ul>";
        if (active) {
            html += "<form method=\"POST\" action=\""+base+"/invite\"><label>Invite</label>"
                    "<div class=\"row\"><input name=\"who\" required autocomplete=\"off\"><button>Invite</button></div></form>"
                    "<form method=\"POST\" action=\""+base+"/title\" class=\"mailnewgroup\">"
                    "<label>Group name</label><input name=\"title\" maxlength=\"80\" value=\""+html_escape(conv->title)+"\">"
                    "<button class=\"ghost\">Save name</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\" onsubmit=\"return confirm('Leave this group? You will not be told if others stay.');\">"
                    "<button class=\"ghost\">Leave group</button></form>";
        }
        html += "</details>"
            "<script>(function(){"
            "var list=document.getElementById('msglist');if(list)list.scrollTop=list.scrollHeight;"
            "var form=document.getElementById('mailcomposer'),ta=document.getElementById('mailtext');"
            "if(!form||!ta)return;"
            "ta.addEventListener('keydown',function(e){"
            "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();form.requestSubmit();}"
            "});"
            "})();</script>";
        return pg("Group", html);
    }

    if(method=="GET"&&path=="/communities") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html = "<h1>Groups</h1>"+err+
            "<p class=\"lede\">Platform groups. Not AVND localities, and they do not confer standing.</p>";
        auto all = g_store.all_communities();
        std::vector<Community> mine;
        for (auto& c : all) if (g_store.is_member(c.slug, user)) mine.push_back(c);
        if (!mine.empty()) {
            html += "<div class=\"card\"><h2>Yours</h2><ul class=\"plain\">";
            for (auto& c : mine) {
                html += "<li class=\"who\" style=\"margin:.4rem 0\"><div><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+
                        html_escape(c.name)+"</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span></div></li>";
            }
            html += "</ul></div>";
        }
        auto cities = g_store.cities_in_use();
        if (!cities.empty()) {
            html += "<div class=\"card\"><h2>Cities</h2>"
                    "<p class=\"muted\">Groups that name the same city are listed together, so people there can find each other. "
                    "<a href=\"/map\">See them on the MAINstream map</a>.</p><p>";
            for (const auto& cu : cities) {
                html += "<a class=\"chip\" href=\"/city/"+url_encode(cu.key)+"\" style=\"text-decoration:none\">"+
                        html_escape(cu.name)+" &middot; "+std::to_string(cu.groups)+"</a> ";
            }
            html += "</p></div>";
        }
        html += "<div class=\"card\"><h2>Find a group</h2>"
            "<input id=\"commfilter\" type=\"search\" placeholder=\"Search by name or address\" "
            "oninput=\"var q=this.value.toLowerCase();document.querySelectorAll('.commrow').forEach(function(el){"
            "el.style.display=el.getAttribute('data-q').indexOf(q)>=0?'':'none';});\">";
        if (all.empty()) html += "<p class=\"empty\">None yet. Start one below.</p>";
        else {
            std::map<int, std::map<std::string, std::vector<Community>>> grouped;
            for (auto& c : all) grouped[static_cast<int>(c.category)][c.subcategory].push_back(c);
            for (const auto& ci : kCommunityCategories) {
                auto git = grouped.find(static_cast<int>(ci.value));
                if (git == grouped.end()) continue;
                std::size_t total = 0;
                for (auto& [subcat, list] : git->second) total += list.size();
                html += "<details open><summary>"+std::string(ci.label)+" &middot; "+std::to_string(total)+"</summary>";
                for (auto& [subcat, list] : git->second) {
                    html += "<div style=\"margin:.5rem 0 .5rem 1rem\">"
                            "<p class=\"eyebrow\">"+html_escape(subcat.empty() ? "General" : subcat)+"</p><ul class=\"plain\">";
                    for (auto& c : list) {
                        std::string q = c.name + " " + c.slug + " " + c.about + " " + c.subcategory + " " + c.city;
                        for (char& ch : q) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
                        const std::string chref = city_href(c.city);
                        html += "<li class=\"commrow\" data-q=\""+html_escape(q)+"\"><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+
                                html_escape(c.name)+"</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span>"+
                                (chref.empty() ? std::string() : " <a class=\"muted\" href=\""+chref+"\">"+html_escape(c.city)+"</a>")+
                                "<p class=\"muted\">"+html_escape(c.about)+"</p></li>";
                    }
                    html += "</ul></div>";
                }
                html += "</details>";
            }
        }
        // /communities?city=<text> (the city page's "Start a group here")
        // opens the form with the city filled in.
        auto city_qs = qs.find("city");
        const std::string city_prefill = city_qs == qs.end() ? std::string() : clean_city(city_qs->second);
        html += "</div><div class=\"card\"><details"+std::string(city_prefill.empty() ? "" : " open")+"><summary>Start a group</summary>"
            "<form method=\"POST\" action=\"/communities\" style=\"margin-top:.7rem\">"
            "<label>Name</label><input name=\"name\" required maxlength=\"80\" placeholder=\"Neighborhood watch\">"
            "<label>Link</label><input name=\"slug\" required maxlength=\"32\" pattern=\"[A-Za-z0-9._-]+\" placeholder=\"watch\">"
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Becomes /c/watch &mdash; letters, numbers, dots, dashes.</p>"
            "<label>Category</label><select name=\"category\" id=\"catselect\">";
        for (const auto& ci : kCommunityCategories) {
            html += "<option value=\""+std::to_string(static_cast<int>(ci.value))+"\">"+ci.label+"</option>";
        }
        html += "</select>"
            "<label>Subcategory</label>"
            "<input name=\"subcategory\" id=\"subcatinput\" required maxlength=\"60\" list=\"subcatlist\" "
            "placeholder=\"e.g. Neighborhood Watch\" autocomplete=\"off\">"
            "<datalist id=\"subcatlist\"></datalist>"
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Pick an existing one, or type a new one.</p>"
            "<label>City <span class=\"muted\">(optional)</span></label>"
            "<input name=\"city\" maxlength=\"80\" list=\"citylist\" placeholder=\"e.g. Fresno, CA\" autocomplete=\"off\" value=\""+
            html_escape(city_prefill)+"\">"+city_datalist_html()+
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Groups that name the same city are listed together. "
            "Pick an existing spelling if yours is there. A city here is a label, not a government.</p>"
            "<label>About</label><textarea name=\"about\" maxlength=\"2000\" placeholder=\"What this group is for.\"></textarea>"
            "<button>Create</button></form>"
            "<script>(function(){"
            "var SUB="+community_subcats_js()+";"
            "var sel=document.getElementById('catselect'),dl=document.getElementById('subcatlist');"
            "function refresh(){"
            "dl.innerHTML='';"
            "(SUB[sel.value]||[]).forEach(function(s){"
            "var o=document.createElement('option');o.value=s;dl.appendChild(o);"
            "});"
            "}"
            "sel.addEventListener('change',refresh);refresh();"
            "})();</script>"
            "</details></div>";
        return pg("Groups", html);
    }

    if(method=="GET"&&path.rfind("/c/",0)==0&&path.size()>10&&path.compare(path.size()-7,7,"/canvas")==0) {
        if(!acc) return need_login();
        const std::string slug = path.substr(3, path.size()-3-7);
        if (!g_store.community(slug) || !g_store.is_member(slug, user)) return http_redir("/c/"+slug);
        return pg("Compose a post", canvas_tool_page(user, "", false, "/c/"+slug));
    }

    if(method=="GET"&&path.rfind("/c/",0)==0) {
        if(!acc) return need_login();
        const std::string slug = path.substr(3);
        auto c = g_store.community(slug);
        if (!c)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        const bool member = g_store.is_member(slug, user);
        const bool owner = c->owner == user;
        std::string html = "<h1>"+html_escape(c->name)+"</h1>"
            "<p class=\"muted\">"+html_escape(category_label(c->category))+" &middot; "+
            html_escape(c->subcategory.empty() ? "General" : c->subcategory)+
            " · /"+html_escape(c->slug)+" · started by <a href=\"/u/"+html_escape(c->owner)+"\">"+html_escape(c->owner)+"</a>"+
            (city_href(c->city).empty() ? std::string()
                : " · <a href=\""+city_href(c->city)+"\">"+html_escape(c->city)+"</a>")+"</p>";
        html += flash_error(qs);
        html += "<div class=\"card\"><p>"+html_escape(c->about.empty()?"No description yet.":c->about)+"</p>";
        if (!member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/join\"><button>Join this group</button></form>";
        } else if (!owner) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/leave\"><button class=\"ghost\">Leave</button></form>";
        } else {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/delete\" onsubmit=\"return confirm('Delete this group and its posts?');\">"
                    "<button class=\"ghost\">Delete group</button></form>";
            html += "<details style=\"margin-top:.6rem\"><summary>"+
                    std::string(c->city.empty() ? "Add a city" : "Change city")+"</summary>"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/city\" style=\"margin-top:.5rem\">"
                    "<div class=\"row\"><input name=\"city\" maxlength=\"80\" list=\"citylist\" placeholder=\"e.g. Fresno, CA\" "
                    "autocomplete=\"off\" value=\""+html_escape(c->city)+"\"><button>Save</button></div>"+city_datalist_html()+
                    "<p class=\"muted\">Groups that name the same city are listed together so people there can find each other. "
                    "Leave it empty to remove the city.</p></form></details>";
        }
        html += "</div>";
        if (member) {
            html += "<div class=\"card\">"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"\" enctype=\"multipart/form-data\">"
                    "<textarea name=\"text\" maxlength=\"2000\" placeholder=\"Post in "+html_escape(c->name)+"\" "
                    "oninput=\"document.getElementById('cchars').textContent=(2000-this.value.length)+' left'\"></textarea>"
                    + composer_attach_bar("cchars") +
                    "</form>"
                    "<p class=\"muted\" style=\"margin-top:.5rem\"><a href=\"/c/"+html_escape(slug)+"/canvas\">Draw instead</a></p></div>";
        }
        auto posts = g_store.posts_in(slug, 80);
        if (posts.empty()) html += "<div class=\"card\"><p class=\"empty\">No posts yet.</p>"
            "<p class=\"muted\">"+(member ? std::string("You're in. Write the first one.") : "Join to post.")+"</p></div>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        html += "<div class=\"card\"><details><summary>Members · "+std::to_string(g_store.members_of(slug).size())+"</summary><ul class=\"plain\">";
        // Add friend beside each member you could ask (Brandon 2026-11-10): not you, a friend, a pending request or a block
        std::set<std::string> asked;
        if (!user.empty()) {
            for (const auto& u : g_store.incoming_requests(user)) asked.insert(u);
            for (const auto& u : g_store.outgoing_requests(user)) asked.insert(u);
        }
        for (auto& n : g_store.members_of(slug)) {
            const bool can_add = !user.empty() && n != user && !asked.count(n) && !g_store.are_friends(user, n) &&
                                 !g_store.is_blocked(user, n) && !g_store.is_blocked(n, user);
            html += "<li class=\"who\" style=\"margin:.35rem 0;align-items:center\">"+portrait_tag(n,"sm")+
                    "<div><a href=\"/u/"+html_escape(n)+"\">"+html_escape(n)+"</a>"+
                    (n==c->owner?" <span class=\"muted\">starter</span>":"")+"</div>";
            if (can_add)
                // same gold Add friend as People you may know (one look everywhere), centred on the row
                html += "<form class=\"inline\" style=\"margin:0 0 0 auto;align-self:center\" method=\"POST\" action=\"/friends\" data-add=\""+html_escape(n)+"\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(n)+"\">"
                        "<input type=\"hidden\" name=\"next\" value=\"/c/"+html_escape(slug)+"\">"
                        "<button type=\"submit\">Add friend</button></form>";
            html += "</li>";
        }
        html += "</ul>";
        if (member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/invite\" style=\"margin-top:.6rem\">"
                    "<label>Invite by account name</label><div class=\"row\"><input name=\"who\" required placeholder=\"account name\" autocomplete=\"off\"><button>Invite</button></div></form>";
        }
        html += "</details></div>";
        return pg(c->name, html);
    }

    // /city/<key>: every group that names this city (city proposals build
    // plan, slice 1). A city is a label people chose, not an AVND locality.
    // Map view (slice 6): /map is the MAINstream; /city/<key>/map is one city's branch.
    if(method=="GET"&&path=="/map") {
        if(!acc) return need_login();
        // Interactive map (assets/lgmap.js): click a box and its connections
        // grow out of it; its page opens in the field on the right, which can
        // fill the window; MAINstream shrinks it back. <noscript> keeps the
        // plain drawing for browsers without JavaScript.
        std::string focus;
        if (qs.count("focus") && map_gnode(qs.at("focus"), user)) focus = qs.at("focus");
        return pg("Map", map_page_body(user, focus));
    }
    // Your own wishes and pain points. Owner only; nobody else has a way in.
    if(method=="GET"&&path=="/problems") {
        if(!acc) return need_login();
        const bool on = g_store.map_prefs_of(user).pains;
        std::string html = "<h1>Your problems and solutions</h1>"
            "<p class=\"lede\">Things you said you wish for or are tired of, so you can find what and who could help. "
            "Only you can see this page. Flow puts one kept subject on the map for people who can see you. "
            "Flowing it also shows you who else Flowed that same subject.</p>";
        const bool looked_none = on && qs.count("looked") && qs.at("looked") == "none";
        if (!on) html += "<div class=\"card\"><p>This is turned off, so nothing is being noticed. "
                         "<a href=\"/settings#map-privacy\">Turn it on in Settings</a>.</p></div>";
        else {
            html += "<p class=\"muted\">It reads your new posts, proposals, proposal comments, and messages you sent. "
                    "<a href=\"/settings#map-privacy\">Change this</a></p>";
            html += "<form method=\"POST\" action=\"/problems/look\">"
                    "<button type=\"submit\">Look at what I've written</button></form>";
            html += "<p class=\"muted\">This reads posts, proposals, proposal comments, and messages you already wrote. "
                    "Each one is offered here as Keep or Forget before it is a box. "
                    "Friends posts, group posts, and messages stay on your map only. "
                    "It does not read the other person's messages, or reposts.</p>";
            if (looked_none) html += "<p class=\"muted\">Nothing new in what you've written.</p>";
        }
        auto all = g_store.pains_of(user);
        auto source_link = [](const PainPoint& p) -> std::string {
            auto post_href = [](const std::string& source, std::size_t skip, const char* label) {
                return " <a class=\"muted\" href=\"/post/" + html_escape(source.substr(skip)) + "\">" + label + "</a>";
            };
            if (p.source.rfind("post:", 0) == 0) return post_href(p.source, 5, "from your post");
            if (p.source.rfind("fpost:", 0) == 0) return post_href(p.source, 6, "from a friends post");
            if (p.source.rfind("gpost:", 0) == 0) return post_href(p.source, 6, "from a group post");
            if (p.source.rfind("prop:", 0) == 0) return " <a class=\"muted\" href=\"/proposals?id=" + html_escape(p.source.substr(5)) + "\">from your proposal</a>";
            if (p.source.rfind("mail:", 0) == 0) return " <span class=\"muted\">from a message you sent</span>";
            if (p.source.rfind("gmsg:", 0) == 0) return " <span class=\"muted\">from a group message you sent</span>";
            return " <span class=\"muted\">from your comment</span>";
        };
        auto label = [](const PainPoint& p) { return std::string(p.kind == "wish" ? "Want" : "Problem"); };
        auto button = [](const char* action, std::uint64_t id, const char* text, bool ghost) {
            return std::string("<form method=\"POST\" action=\"/problems/") + action + "\" style=\"display:inline\">"
                   "<input type=\"hidden\" name=\"id\" value=\"" + std::to_string(id) + "\"><button" +
                   (ghost ? " class=\"ghost\"" : "") + ">" + text + "</button></form> ";
        };
        std::string sugg, kept;
        for (const auto& p : all) {
            if (!p.kept) {
                sugg += "<li style=\"margin:.6rem 0\"><span class=\"eyebrow\">" + label(p) + "</span> <strong>" +
                        html_escape(p.phrase) + "</strong>" + source_link(p) + "<div style=\"margin-top:.3rem\">" +
                        button("keep", p.id, "Keep", false) + button("forget", p.id, "Forget", true) + "</div></li>";
                continue;
            }
            const auto m = pain_matches(p);
            std::string body;
            if (!m.depts.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Who could act</p><p>";
                for (const auto& [key, tag] : m.depts)
                    body += "<a class=\"chip\" href=\"/proposals?city=" + url_encode(key) + "\">" + html_escape(tag) + "</a> ";
                body += "</p>";
            }
            if (!m.props.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Proposals about it</p><ul class=\"plain\">";
                for (const auto& pr : m.props)
                    body += "<li><a href=\"/proposals?id=" + std::to_string(pr.id) + "\">" + html_escape(pr.title) + "</a></li>";
                body += "</ul>";
            }
            if (!m.groups.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Groups working on it</p><p>";
                for (const auto& c : m.groups)
                    body += "<a class=\"chip\" href=\"/c/" + url_encode(c.slug) + "\">" + html_escape(c.name) + "</a> ";
                body += "</p>";
            }
            if (body.empty()) body = "<p class=\"muted\">No possible solutions on the site yet. You could start one.</p>";
            else body = "<p class=\"eyebrow\" style=\"margin-top:.6rem\">Possible solutions</p>" + body;
            const std::string start = "/proposals?title=" + url_encode(p.phrase) +
                                      (m.city_key.empty() ? std::string() : "&city=" + url_encode(m.city_key)) + "#new";
            const bool shareable = pain_can_flow(p.source);
            const std::string flow_note = !shareable
                ? std::string("This stays on your map. Other people do not see it.")
                : p.flowed
                    ? std::string("This subject is on the map for people who can see you.")
                    : std::string("Flow puts this subject on the map for people who can see you.");
            const std::string flow_btn = !shareable ? std::string()
                : p.flowed ? button("still", p.id, "Still", true)
                           : button("flow", p.id, "Flow", false);
            const std::string also = also_flowed_html(subject_matches(user, p));
            kept += "<div class=\"card\" id=\"pain" + std::to_string(p.id) + "\"><span class=\"eyebrow\">" + label(p) +
                    "</span><h3 style=\"margin:.1rem 0\">" + html_escape(p.phrase) + "</h3>" + source_link(p) + body +
                    "<p style=\"margin-top:.6rem\"><a href=\"" + html_escape(start) + "\">Start a proposal about this</a> &middot; "
                    "<a href=\"/map?focus=pain:" + std::to_string(p.id) + "\">See it on your map</a></p>" +
                    "<p class=\"muted\">" + flow_note + "</p>" + also +
                    flow_btn + button("forget", p.id, "Forget", true) + "</div>";
        }
        if (!sugg.empty())
            html += "<div class=\"card\"><h2>Noticed</h2><p class=\"muted\">Keep the ones that are real. "
                    "Ones you don't keep disappear after 30 days.</p><ul class=\"plain\">" + sugg + "</ul></div>";
        if (!kept.empty()) html += "<h2>Kept</h2>" + kept;
        if (all.empty() && on && !looked_none)
            html += "<div class=\"card\"><p class=\"empty\">Nothing yet.</p><p class=\"muted\">When you write something like "
                    "&ldquo;I wish the buses ran later&rdquo; in a public post, a proposal or a comment, it shows up here.</p></div>";
        return pg("Problems and solutions", html);
    }
    if(method=="POST"&&(path=="/problems/keep"||path=="/problems/forget"||path=="/problems/flow"||path=="/problems/still")) {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        const std::string raw = f["id"];
        if (!raw.empty() && raw.size() <= 19 && raw.find_first_not_of("0123456789") == std::string::npos)
            try { id = std::stoull(raw); } catch (...) { id = 0; }
        // Owner-only checks live in the store; someone else's id just does nothing.
        if (id && path == "/problems/keep") g_store.keep_pain(user, id);
        else if (id && path == "/problems/forget") g_store.forget_pain(user, id);
        else if (id && path == "/problems/flow") {
            auto owned = g_store.pain_by_id(id);
            if (owned && owned->owner == user && pain_can_flow(owned->source)) g_store.flow_pain(user, id, true);
        }
        else if (id && path == "/problems/still") g_store.flow_pain(user, id, false);
        const bool stay = id && path != "/problems/forget";
        return http_redir(stay ? "/problems#pain" + std::to_string(id) : std::string("/problems"));
    }
    // Older public writing, only when this person asks. Same Keep/Forget
    // suggestions as a new post. A switch that is off reads nothing.
    if(method=="POST"&&path=="/problems/look") {
        if(!acc) return need_login();
        const int added = look_public_words(user);
        if (added < 0 || added > 0) return http_redir("/problems");
        return http_redir("/problems?looked=none");
    }
    // One flowed subject. The owner is sent back to their private list.
    // Anyone else sees the phrase and who flowed it, and only when that
    // person is visible to them. A Still box looks the same as a missing one.
    if(method=="GET"&&path.rfind("/pain/",0)==0&&path.size()>6&&path.find('/',6)==std::string::npos) {
        if(!acc) return need_login();
        std::uint64_t id = 0;
        const std::string digits = path.substr(6);
        if (digits.size() <= 19 && digits.find_first_not_of("0123456789") == std::string::npos)
            try { id = std::stoull(digits); } catch (...) { id = 0; }
        auto p = id ? g_store.pain_by_id(id) : std::nullopt;
        auto missing = [&]() {
            return http_code("404 Not Found", wrap("Not found", user, unread,
                "<div class=\"card\"><p>No such subject.</p></div>"));
        };
        if (!p) return missing();
        if (p->owner == user) return http_redir("/problems#pain" + std::to_string(p->id));
        if (!p->kept || !p->flowed || !map_person_ok(p->owner, user)) return missing();
        const std::string eyebrow = p->kind == "wish" ? std::string("Want") : std::string("Problem");
        std::string html = "<div class=\"card\"><span class=\"eyebrow\">" + eyebrow +
            "</span><h1 style=\"margin:.1rem 0\">" + html_escape(p->phrase) + "</h1>"
            "<p>Flowed by <a href=\"/u/" + html_escape(p->owner) + "\">" + html_escape(p->owner) + "</a>.</p>";
        html += also_flowed_html(subject_matches(user, *p));
        html += "<p class=\"muted\">This is the subject they chose to show.</p>"
            "<p><a href=\"/map?focus=pain:" + std::to_string(p->id) + "\">See it on the map</a></p></div>";
        return pg(p->phrase, html);
    }
    // One box and its connections, as JSON, for assets/lgmap.js.
    if(method=="GET"&&path=="/map/node") {
        if(!acc) return http_raw("401 Unauthorized","application/json; charset=utf-8","{\"error\":\"login\"}");
        const std::string id = qs.count("n") ? qs.at("n") : std::string("root");
        auto n = map_gnode(id, user);
        if (!n) return http_raw("404 Not Found","application/json; charset=utf-8","{\"error\":\"not found\"}");
        const MapCtx ctx = map_ctx(user); // relevance colours, for this viewer only
        map_add_sub(*n, user);
        std::string out = "{\"node\":" + map_node_json(*n, map_relevance(*n, ctx)) + ",\"links\":[";
        bool first = true;
        for (auto l : map_links(id, user)) {
            if (!first) out += ",";
            first = false;
            map_add_sub(l.to, user);
            out += "{\"node\":" + map_node_json(l.to, map_relevance(l.to, ctx)) + ",\"edge\":\"" + json_escape(l.edge) + "\"}";
        }
        out += "]}";
        return http_raw("200 OK","application/json; charset=utf-8", out, "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&(path=="/assets/lgmap.js"||path=="/assets/lgmap.css")) {
        const bool js = path=="/assets/lgmap.js";
        std::ifstream in(js ? "assets/lgmap.js" : "assets/lgmap.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK", js ? "text/javascript; charset=utf-8" : "text/css; charset=utf-8", ss.str(),
                        asset_cache_header(rawpath));
    }
    // One post on its own (the map's right-hand field opens this). Same
    // audience rules as the author's profile, via map_post_ok.
    if(method=="GET"&&path.rfind("/post/",0)==0&&path.size()>6&&path.find('/',6)==std::string::npos) {
        if(!acc) return need_login();
        std::uint64_t pid = 0;
        const std::string digits = path.substr(6);
        if (digits.size() <= 19 && digits.find_first_not_of("0123456789") == std::string::npos)
            try { pid = std::stoull(digits); } catch (...) { pid = 0; }
        auto p = pid ? g_store.post_by_id(pid) : std::nullopt;
        if (!p || !map_post_ok(*p, user))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such post.</p></div>"));
        return pg("Post", "<div class=\"card\">" + post_block(*p, user) + "</div>");
    }
    if(method=="GET"&&path.rfind("/city/",0)==0&&path.size()>10&&path.compare(path.size()-4,4,"/map")==0) {
        if(!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size()-6-4));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/map");
        if (key != raw) return http_redir("/city/"+url_encode(key)+"/map");
        auto groups = g_store.communities_in_city(key);
        if (groups.empty() && g_store.proposals_in_city(key).empty())
            return http_code("404 Not Found", wrap("No map here yet", user, unread,
                "<h1>Nothing names this city yet</h1><div class=\"card\"><p><a href=\"/map\">MAINstream</a></p></div>"));
        std::string name = groups.empty() ? key : groups.front().city;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == key) { name = cu.name; break; }
        std::string html = "<p class=\"muted\"><a href=\"/map\">MAINstream</a> &rsaquo; "+html_escape(name)+"</p>"
            "<h1>"+html_escape(name)+"</h1>"
            "<p class=\"lede\">Point at a box to light up its lines. Click it to open it. "
            "<a href=\"/city/"+url_encode(key)+"\">List view</a></p>"
            + city_map_svg(key, name, user) + kCityMapStyle +
            "<p class=\"cmaplegend\"><span class=\"lg-in\">in this city</span><span class=\"lg-member\">member of</span>"
            "<span class=\"lg-touches\">touches</span><span class=\"lg-wrote\">wrote</span>"
            "<span class=\"lg-commented\">commented</span></p>"
            "<p class=\"muted\">Only people who chose to be listed here appear. A city on LiquidGov is a label, not a government.</p>";
        return pg(name+" map", html);
    }
    if(method=="GET"&&path=="/city") return http_redir("/communities"); // the city list lives on Groups
    if(method=="GET"&&path.rfind("/city/",0)==0) {
        if(!acc) return need_login();
        const std::string raw = url_decode(path.substr(6));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        if (key != raw) return http_redir("/city/"+url_encode(key)); // /city/Fresno,%20CA -> /city/fresno-ca
        auto groups = g_store.communities_in_city(key);
        if (groups.empty()) {
            // Proposals and tags for a city are kept while no group names it
            // (Brandon 2026-11-06), so point to them instead of hiding them.
            const auto kept = g_store.proposals_in_city(key).size();
            std::string kept_html;
            if (kept > 0) {
                kept_html = "<p>"+std::to_string(kept)+(kept==1?" proposal is":" proposals are")+
                    " still about this city. <a href=\"/proposals?city="+url_encode(key)+"\">See "+
                    (kept==1?"it":"them")+"</a>. They show here again once a group names the city.</p>";
            }
            return http_code(kept > 0 ? "200 OK" : "404 Not Found", wrap("No groups here yet", user, unread,
                "<h1>No groups name this city yet</h1><div class=\"card\">"
                "<p>Anyone can start one. Give it a city when you do, and it shows up here.</p>"+kept_html+
                "<p><a href=\"/communities\">Groups</a></p></div>"));
        }
        std::string name = groups.front().city;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == key) { name = cu.name; break; }
        std::set<std::string> people;
        for (const auto& c : groups) for (const auto& m : g_store.members_of(c.slug)) people.insert(m);
        std::string html = "<h1>"+html_escape(name)+"</h1>"
            "<p class=\"lede\">People here started these groups. A city on LiquidGov is a label, not a government.</p>"
            "<p><a href=\"/city/"+url_encode(key)+"/map\">Map view</a> &middot; <a href=\"/map\">MAINstream</a></p>"
            + flash_error(qs) +
            "<p class=\"muted\">"+std::to_string(groups.size())+(groups.size()==1?" group":" groups")+" &middot; "+
            std::to_string(people.size())+(people.size()==1?" person":" people")+" in them</p>"
            "<div class=\"card\"><ul class=\"plain\">";
        for (const auto& c : groups) {
            const auto members = g_store.members_of(c.slug).size();
            html += "<li style=\"margin:.6rem 0\"><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+html_escape(c.name)+"</strong></a> "
                    "<span class=\"muted\">/"+html_escape(c.slug)+" &middot; "+html_escape(category_label(c.category))+
                    (c.subcategory.empty() ? std::string() : " &middot; "+html_escape(c.subcategory))+" &middot; "+
                    std::to_string(members)+(members==1?" member":" members")+
                    (g_store.is_member(c.slug, user) ? " &middot; you're in" : "")+"</span>"+
                    (c.about.empty() ? std::string() : "<p class=\"muted\">"+html_escape(c.about)+"</p>")+"</li>";
        }
        html += "</ul></div>";
        {
            const auto tags = g_store.city_tags(key);
            bool viewer_in_city = false;
            for (const auto& c : groups) if (g_store.is_member(c.slug, user)) { viewer_in_city = true; break; }
            html += "<div class=\"card\"><h2>Departments and councils</h2>"
                    "<p class=\"muted\">Named by people in this city's groups. A proposal can say which ones it touches. "
                    "Nothing here is sent to the city.</p>";
            if (viewer_in_city) {
                html += "<form method=\"POST\" action=\"/city/"+url_encode(key)+"/tag\">"
                        "<label>Add one</label><div class=\"row\">"
                        "<input name=\"tag\" maxlength=\"80\" required placeholder=\"Parks, city council\">"
                        "<button>Add</button></div></form>";
            } else {
                html += "<p class=\"muted\">Join a group in this city to add one.</p>";
            }
            if (tags.empty()) html += "<p class=\"empty\">None named yet.</p>";
            else {
                html += "<ul class=\"plain\">";
                for (const auto& tag : tags) html += "<li>"+html_escape(tag)+"</li>";
                html += "</ul>";
            }
            html += "</div>";

            auto city_props = g_store.proposals_in_city(key);
            std::reverse(city_props.begin(), city_props.end());
            const std::string back = "/city/"+url_encode(key);
            html += "<div class=\"card\"><h2>Proposals</h2>"
                    "<p class=\"muted\">Unofficial straw poll. Discussion only. Nothing is sent to the city.</p>";
            if (city_props.empty()) html += "<p class=\"empty\">None for this city yet.</p>";
            else {
                auto carries = [](const Proposal& p, const std::string& tag) {
                    for (const auto& have : p.tags) if (have == tag) return true;
                    return false;
                };
                for (const auto& tag : tags) {
                    std::vector<Proposal> bucket;
                    for (const auto& p : city_props) if (carries(p, tag)) bucket.push_back(p);
                    if (bucket.empty()) continue;
                    html += "<h3>"+html_escape(tag)+"</h3>";
                    for (const auto& p : bucket) html += proposal_block(p, user, back);
                }
                std::vector<Proposal> untagged;
                for (const auto& p : city_props) if (p.tags.empty()) untagged.push_back(p);
                if (!untagged.empty()) {
                    html += "<h3>Not tagged</h3>";
                    for (const auto& p : untagged) html += proposal_block(p, user, back);
                }
            }
            html += "<p><a href=\"/proposals?city="+url_encode(key)+"\">Open a proposal for this city</a></p></div>";
        }
        html += "<div class=\"card\"><h2>People here</h2>"
                "<p class=\"muted\">People in these groups who chose to be listed. Nobody is listed unless they ask.</p>";
        bool viewer_in = false;
        for (const auto& c : groups) if (g_store.is_member(c.slug, user)) { viewer_in = true; break; }
        if (viewer_in) {
            const bool listed = g_store.is_city_listed(key, user);
            html += "<form method=\"POST\" action=\"/city/"+url_encode(key)+"/list\" style=\"margin:.4rem 0\">"
                    "<input type=\"hidden\" name=\"on\" value=\""+std::string(listed ? "0" : "1")+"\">"
                    "<button>"+std::string(listed ? "Stop listing me" : "List me here")+"</button></form>";
        } else {
            html += "<p class=\"muted\">Join a group in this city to be listed here.</p>";
        }
        auto listed = g_store.people_listed_in_city(key);
        if (listed.empty()) html += "<p class=\"empty\">Nobody has chosen to be listed yet.</p>";
        else {
            html += "<ul class=\"plain\">";
            for (const auto& name : listed) {
                if (name != user && (g_store.is_blocked(user, name) || g_store.is_blocked(name, user))) continue;
                html += "<li class=\"who\" style=\"margin:.4rem 0\">"+portrait_tag(name, "sm")+
                        "<div><a href=\"/u/"+html_escape(name)+"\">"+html_escape(name)+"</a>";
                if (name == user) html += " <span class=\"muted\">you</span>";
                else if (g_store.are_friends(user, name)) html += " <span class=\"muted\">Friends</span>";
                else {
                    html += "<form class=\"inline\" method=\"POST\" action=\"/friends\">"
                            "<input type=\"hidden\" name=\"who\" value=\""+html_escape(name)+"\">"
                            "<input type=\"hidden\" name=\"next\" value=\""+html_escape("/city/"+url_encode(key))+"\">"
                            "<button type=\"submit\">Add friend</button></form>";
                }
                html += "</div></li>";
            }
            html += "</ul>";
        }
        html += "</div><div class=\"card\"><p><a href=\"/communities?city="+url_encode(name)+"\">Start a group in "+
                html_escape(name)+"</a></p><p class=\"muted\">Anyone can start one. There's no minimum size.</p></div>";
        return pg(name, html);
    }

    if (method=="POST" && path.rfind("/city/", 0)==0 && path.size() > 11
        && path.compare(path.size()-5, 5, "/list")==0) {
        if (!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size() - 6 - 5));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        const bool on = parse_form(body)["on"] != "0";
        if (!g_store.set_city_listed(key, user, on))
            return http_redir("/city/"+url_encode(key)+"?err=list");
        return http_redir("/city/"+url_encode(key));
    }

    if (method=="POST" && path.rfind("/city/", 0)==0 && path.size() > 10
        && path.compare(path.size()-4, 4, "/tag")==0) {
        if (!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size() - 6 - 4));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        const std::string dest = "/city/"+url_encode(key);
        if (!g_store.in_city(key, user)) return http_redir(dest+"?err=ctag");
        if (!g_store.add_city_tag(key, user, parse_form(body)["tag"]))
            return http_redir(dest+"?err=ctagname");
        return http_redir(dest);
    }

    if(method=="GET"&&path=="/notices") {
        if(!acc) return need_login();
        // Not mark_notices_read: a "message" notice is the Messages icon's
        // badge now, not this page's -- clearing it here would make an
        // unread message vanish from its own badge without the message
        // ever having been opened.
        g_store.mark_notices_read_of_type(user, "friend_accept");
        g_store.mark_notices_read_of_type(user, "group_invite");
        return pg("Notices", "<h1>Notices</h1>"+notices_list_html(user));
    }
    // Fetched by the header's Notifications widget on open -- same content
    // as the full page (same helper), as a bare fragment for innerHTML.
    if(method=="GET"&&path=="/notices/panel") {
        if(!acc) return http_code("401 Unauthorized", "");
        g_store.mark_notices_read_of_type(user, "friend_accept");
        g_store.mark_notices_read_of_type(user, "group_invite");
        return http_ok(notices_list_html(user));
    }

    if(method=="GET"&&path=="/settings/password") {
        if(!acc) return need_login();
        // Only this page has password fields (CODE-SCAN-UX B1). The read-only account field gives the password manager
        // the username half of the pair, so it saves and fills here and stops looking elsewhere.
        std::string html = "<h1>Change password</h1>"
            "<p class=\"muted\"><a href=\"/settings\">&lsaquo; Settings</a></p>" + flash_error(qs) +
            "<div class=\"card\"><form method=\"POST\" action=\"/settings/password\">"
            "<label>Account</label><input type=\"text\" name=\"username\" autocomplete=\"username\" value=\""+html_escape(user)+"\" readonly>"
            "<label>Current password</label><input type=\"password\" name=\"current\" required autocomplete=\"current-password\">"
            "<label>New password</label><input type=\"password\" name=\"next\" required autocomplete=\"new-password\">"
            "<button>Change password</button></form></div>";
        return pg("Change password", html);
    }
    if(method=="GET"&&path=="/settings") {
        if(!acc) return need_login();
        auto p = g_store.prefs_of(user);
        auto find_sel = [&](Findability f){ return p.findability==f?" checked":""; };
        std::string html = "<h1>Settings</h1>"
            "<p class=\"muted\">Account, privacy, data, and notifications.</p>"
            // jump links (sweep 2026-11-09): nine sections were one long scroll on a phone
            "<div class=\"setidx\" role=\"navigation\" aria-label=\"Settings sections\"><a href=\"#security\">Security</a><a href=\"#sessions\">Sessions</a><a href=\"#map\">Map and connections</a><a href=\"#privacy\">Privacy</a><a href=\"#blocks\">Block list</a><a href=\"#jukebox\">Jukebox</a><a href=\"#identity\">Identity</a><a href=\"#data\">Data</a><a href=\"#notifications\">Notifications</a></div>";  // a div: the phone CSS hides every <nav> (nav{display:none} is the mobile menu)
        if (qs["ok"]=="password") html += "<p class=\"ok\">Password changed.</p>";
        html += flash_error(qs);

        // Change password is its own page (CODE-SCAN-UX B1, Brief 4 item 9): a password field anywhere on this long
        // page made iOS/Chrome treat all of it as a login form and fill saved credentials into the block, delete and
        // other boxes further down.
        html += "<div class=\"card\"><h2 id=\"security\">Security</h2>"
            "<p><a class=\"btn ghost\" href=\"/settings/password\">Change password</a></p><h2 id=\"sessions\">Sessions</h2>";
        auto all = g_store.sessions_for(acc->id);
        int shown = 0;
        for (const auto& s : all) {
            if (s.revoked) continue;
            if (s.expires_at <= now()) continue;
            ++shown;
            const bool current = s.id == sid;
            html += "<div class=\"sess\"><div><strong>"+html_escape(s.device_label.empty()?"web":s.device_label)+"</strong>"
                    +(current?" <span class=\"muted\">this device</span>":"")+
                    "<div class=\"muted\">Started "+html_escape(fmt_time(s.created_at))+
                    " · last seen "+html_escape(fmt_time(s.last_seen))+"</div></div>";
            if (!current) {
                html += "<form method=\"POST\" action=\"/settings/revoke\">"
                        "<input type=\"hidden\" name=\"sid\" value=\""+html_escape(s.id)+"\">"
                        "<button class=\"danger\" type=\"submit\">Revoke</button></form>";
            }
            html += "</div>";
        }
        if (!shown) html += "<p class=\"muted\">None.</p>";
        html += "<form method=\"POST\" action=\"/settings/logout-all\" style=\"margin-top:1rem\">"
                "<button class=\"danger\" type=\"submit\">Log out other devices</button></form></div>";

        {
            // Map and connections (slice 6): what /map may show about you.
            const auto mp = g_store.map_prefs_of(user);
            auto box = [](const char* name, bool on, const char* label, const char* help) {
                return std::string("<label class=\"row\" style=\"gap:.5rem;align-items:flex-start;margin:.45rem 0\">"
                       "<input type=\"checkbox\" name=\"") + name + "\" value=\"1\"" + (on ? " checked" : "") + "> <span><strong>" +
                       label + "</strong><br><span class=\"muted\">" + help + "</span></span></label>";
            };
            html += "<div class=\"card\" id=\"map-privacy\"><h2 id=\"map\">Map and connections</h2>"
                    "<p class=\"muted\">The <a href=\"/map\">Map</a> connects people, groups, cities and proposals so you can find "
                    "the people worth talking to. Choose what it shows about you. You always see your own.</p>"
                    "<form method=\"POST\" action=\"/settings/map\">"
                    + box("show", mp.show, "Show me on the map", "Off hides you everywhere on the map, even in groups and cities.")
                    + box("groups", mp.groups, "Show the groups I'm in", "Lines from you to your groups.")
                    + box("posts", mp.posts, "Show my posts", "Posts others can already see, as boxes on the map.")
                    + box("comments", mp.comments, "Show what I commented on", "Lines from you to proposals you commented on.")
                    + box("friends", mp.friends, "Show my friends", "Off by default. A friendship shows to others only when both of you turn this on.")
                    + box("pains", mp.pains, "Notice my problems and wants",
                          "Reads your new posts, proposals, proposal comments, and messages you sent for phrases like "
                          "&ldquo;I wish&hellip;&rdquo; or &ldquo;I hate when&hellip;&rdquo;, and suggests them to you. "
                          "Friends posts, group posts, and messages stay on your map only. The other person's messages are not read. "
                          "On your problems page, Look at what I've written reads older writing of those same kinds when you ask. "
                          "Only you see the private ones. Turning this off deletes everything it found. "
                          "Note: the site's operator can read what is stored on the server.")
                    + "<p class=\"muted\"><a href=\"/problems\">See your problems and solutions</a></p>"
                    + "<button>Save map settings</button></form></div>";
        }
        html += std::string("<div class=\"card\"><h2 id=\"privacy\">Privacy</h2><form method=\"POST\" action=\"/settings/privacy\">")
            + "<label>Who can find you</label>"
            + "<p class=\"muted\">One axis, three states. Each level includes the one above it.</p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"0\"" + find_sel(Findability::FriendsOnly) + "> Friends only</label>"
            + "<br><span class=\"muted\">Only people who are already your friends can find you. Strangers can't search you up at all.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"1\"" + find_sel(Findability::ExactMatch) + "> Exact match</label>"
            + "<br><span class=\"muted\">Anyone who types your exact username can find you, but you won't show up as they type.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"2\"" + find_sel(Findability::Typeahead) + "> Typeahead</label>"
            + "<br><span class=\"muted\">Anyone can find you, including as a partial match while they're still typing.</span></p>"
            + "<p><label><input type=\"checkbox\" name=\"group_invite_confirm\" value=\"1\"" + (p.group_invite_confirm?" checked":"") + "> "
            + "Don't add me to groups without asking</label></p>"
            + "<p class=\"muted\">Direct messages are friends-only until we can tell who is a minor without a checkbox. "
            + "Unfriending or blocking leaves every group conversation you share with that person. "
            + "Messages on this copy are stored so the operator can read them.</p>"
            + "<label>Visibility register</label>"
            + "<p class=\"muted\">Each level admits every level after it too: public sees everything members do, "
            + "members see everything friends do, friends see everything only-me hides from everyone else.</p>";
        auto vis_select = [&](const char* field, Visibility current) {
            auto opt = [&](Visibility v, const char* label) {
                return std::string("<option value=\"") + std::to_string(static_cast<int>(v)) + "\""
                    + (current == v ? " selected" : "") + ">" + label + "</option>";
            };
            return std::string("<select name=\"") + field + "\">"
                + opt(Visibility::Public, "Public")
                + opt(Visibility::Members, "Members")
                + opt(Visibility::Friends, "Friends")
                + opt(Visibility::OnlyMe, "Only me")
                + "</select>";
        };
        html += "<table class=\"register\"><tr><th>Surface</th><th>Who can see it</th></tr>"
                "<tr><td>Profile</td><td>" + vis_select("profile_visibility", p.profile_visibility) + "</td></tr>"
                "<tr><td>Posts</td><td>" + vis_select("posts_visibility", p.posts_visibility) + "</td></tr>"
                "<tr><td></td><td><span class=\"muted\">Each post on /feed now picks its own reach "
                "(Anybody/Friends/Groups) -- this row is the fallback for Groups-only posts on your "
                "profile, and Only me still hides everything from everyone but you.</span></td></tr>"
                "</table>"
                "<button>Save privacy</button></form><h2 id=\"blocks\">Block list</h2>";
        auto bl = g_store.block_entries_of(user);
        html += "<p class=\"muted eyebrow\">"+std::to_string(bl.size())+
                (bl.size()==1?" member":" members")+" &middot; not visible to them</p>";
        if (bl.empty()) html += "<p class=\"empty\">No one blocked.</p>";
        for (auto& b : bl) {
            html += "<div class=\"person\"><div><span>"+html_escape(b.other)+"</span>"
                    "<div class=\"muted\">Blocked "+html_escape(fmt_time(b.at))+
                    (b.reason.empty() ? "" : " · " + html_escape(b.reason))+"</div></div>"
                    "<form method=\"POST\" action=\"/settings/unblock\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(b.other)+"\">"
                    "<button class=\"ghost\">Unblock</button></form></div>";
        }
        html += "<form method=\"POST\" action=\"/settings/block\" style=\"margin-top:.75rem\">"
                "<div class=\"row\"><input name=\"who\" placeholder=\"account name\" autocomplete=\"off\" required style=\"flex:1\">"
                "<button class=\"danger\" type=\"submit\">Add block</button></div>"
                "<input name=\"reason\" placeholder=\"Reason (optional, only you see it)\" maxlength=\"200\"></form>";
        {
            // Moved off the profile page: who can queue a track on your
            // jukebox is an access-control setting like block list and
            // visibility above, not something that belongs on a page
            // visitors see (Brandon: "its not appropriate" there).
            auto jsel = [&](JukeboxPolicy pol) -> std::string { return p.jukebox_policy==pol?" checked":""; };
            html += "<h2 id=\"jukebox\">Jukebox</h2>"
                    "<form method=\"POST\" action=\"/me/jukebox/policy\">"
                    "<label>Who else can queue a track on your jukebox</label>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"0\""+jsel(JukeboxPolicy::OwnerOnly)+"> Just me</label></p>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"1\""+jsel(JukeboxPolicy::Friends)+"> Friends</label></p>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"2\""+jsel(JukeboxPolicy::Anyone)+"> Anyone signed in</label></p>"
                    "<p class=\"muted\">One request at a time per person; it plays once, then it's gone. "
                    "Anyone you've blocked above can never queue, regardless.</p>"
                    "<button>Save</button></form>";
        }
        html += "</div>";

        html += "<div class=\"card\"><h2 id=\"identity\">Identity</h2>"
            "<p>No civic identity is linked to this account.</p>"
            "<p class=\"muted\">Voting is not live yet. Linking a citizen identity later will not happen from this page until it is.</p></div>";

        html += "<div class=\"card\"><h2 id=\"data\">Data</h2>"
            "<p><a class=\"btn ghost\" href=\"/export\">Download your export</a></p>"
            "<p class=\"muted\">Export includes username, blurb, profile CSS, your friend list, groups you started or joined, "
            "posts you wrote, settings, and messages you sent. It does not include other people’s text or your password hash.</p>"
            "<form method=\"POST\" action=\"/settings/delete\">"
            // the name only in the label, never as a placeholder (CODE-SCAN-UX B1: a keychain-style fill target)
            "<label>Delete account — type your account name, <strong>"+html_escape(user)+"</strong></label>"
            "<input name=\"confirm\" autocomplete=\"off\">"
            "<button class=\"danger\">Delete account</button></form>"
            "<p class=\"muted\">Sessions end immediately. Mail you sent is shown as [deleted account]. "
            "A fingerprint of the phone number is held ninety days so a deleted account cannot immediately reopen under the same number.</p></div>";

        html += std::string("<div class=\"card\"><h2 id=\"notifications\">Notifications</h2><form method=\"POST\" action=\"/settings/notify\">")
            + "<p><label><input type=\"checkbox\" name=\"notify_friend\" value=\"1\"" + (p.notify_friend?" checked":"") + "> Friend requests and accepts</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_message\" value=\"1\"" + (p.notify_message?" checked":"") + "> New mail</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_group\" value=\"1\"" + (p.notify_group?" checked":"") + "> Group invites</label></p>"
            + "<p class=\"muted\">Login alerts stay on.</p>"
            + "<p><label><input type=\"checkbox\" name=\"intro_every_visit\" value=\"1\"" + (p.intro_every_visit?" checked":"") + "> Play the landing intro every visit</label></p>"
            + "<p class=\"muted\">Off plays it just the first time you land on the site, on this browser.</p>"
            + "<button>Save notifications</button></form></div>";
        return pg("Settings", html);
    }

    if(method=="GET"&&path=="/export") {
        if(!acc) return need_login();
        std::ostringstream o;
        o << "LiquidGov.US platform export\n";
        o << "This is platform data. It is not the governance kernel.\n\n";
        o << "username: " << user << "\n";
        o << "created_at: " << fmt_time(acc->created_at) << "\n";
        o << "blurb: " << g_store.blurb(acc->id) << "\n";
        o << "profile css:\n" << g_store.author_css(acc->id) << "\n\n";
        o << "friends:\n";
        for (auto& n : g_store.friends_of(user)) o << "  " << n << "\n";
        o << "\ncommunities:\n";
        for (auto& c : g_store.communities_of(user)) o << "  " << c.slug << " " << c.name << "\n";
        o << "\nposts you wrote:\n";
        for (auto& p : g_store.posts_by(user)) {
            o << "  [" << fmt_time(p.ts) << "] ";
            if (!p.community.empty()) o << "/" << p.community << " ";
            o << p.text << "\n";
        }
        o << "\nmessages you sent:\n";
        for (auto& m : g_store.mail_sent_by(user)) {
            o << "  to " << m.to << " [" << fmt_time(m.ts) << "] " << m.text << "\n";
        }
        auto pr = g_store.prefs_of(user);
        o << "\nsettings:\n";
        o << "  findability: " << static_cast<int>(pr.findability) << "\n";
        o << "  group_invite_confirm: " << (pr.group_invite_confirm?"yes":"no") << "\n";
        std::string extra = "Content-Disposition: attachment; filename=\"liquidgov-export.txt\"\r\n";
        extra += "Cache-Control: no-store\r\n";
        return http_raw("200 OK", "text/plain; charset=utf-8", o.str(), extra);
    }

    if(method=="POST"&&path=="/signup") {
        auto f=parse_form(body);
        SignupDraft d;
        d.username=trim_ws(f["username"]);
        d.phone=trim_ws(f["phone"]);
        d.email=trim_ws(f["email"]);
        d.display_name=trim_ws(f["display_name"]);
        d.hometown=trim_ws(f["hometown"]);
        d.living=trim_ws(f["living"]);
        d.bio=trim_ws(f["bio"]);
        d.interests=trim_ws(f["interests"]);
        d.career=trim_ws(f["career"]);
        d.school=trim_ws(f["school"]);
        auto pass=f["password"];
        auto fail=[&](const std::string& err){ return http_ok(signup_page(user, unread, err, d)); };
        if (!valid_username(d.username) || d.username.size() < 3) {
            std::string err = d.username.empty()
                ? "Need a username: 3–32 letters/numbers (dots, underscores, hyphens ok). No spaces, no @."
                : d.username.size() < 3
                    ? "Username needs at least three characters."
                    : ("“"+d.username+"” isn’t a valid username. Letters, numbers, dots, underscores, hyphens. No spaces.");
            return fail(err);
        }
        if (d.display_name.empty() || d.display_name.size() > 80)
            return fail("Need a display name (up to 80 characters).");
        if (pass.size() < 8)
            return fail("Password needs at least eight characters.");
        if (!looks_like_email(d.email))
            return fail("Need a real email address, kept only so you can reset your password if you forget it.");
        if (count_digits(d.phone) < 10)
            return fail("Need a real phone number (at least ten digits). It is a signup binding, not stored as the number itself, and not used to log in.");
        if (pass.empty() || d.phone.empty())
            return fail("Need username, password, email, phone, and a display name.");
        if (g_store.phone_quarantined(fingerprint_phone(d.phone)))
            return fail("Could not create that account.");
        auto r=create_account(g_store,g_breach,d.username,pass,d.phone);
        if(r.error==AuthError::None) {
            if (auto created = g_store.find_by_username(d.username)) {
                g_store.set_phone_fp(created->id, fingerprint_phone(d.phone));
                if (!d.bio.empty()) g_store.set_blurb(created->id, d.bio.substr(0, 500));
                g_store.set_email(d.username, lowercase_ascii(d.email));
                Profile p;
                p.display_name = d.display_name.substr(0, 80);
                p.email_fp = fingerprint_phone(d.email);
                p.hometown = d.hometown.substr(0, 80);
                p.living = d.living.substr(0, 80);
                p.interests = d.interests.substr(0, 200);
                p.career = d.career.substr(0, 80);
                p.school = d.school.substr(0, 80);
                g_store.set_profile(d.username, p);
                g_store.set_map_prefs(d.username, MapPrefs{}); // today's defaults written down: no "now show" note later
            }
            r=login(g_store,g_limiter,d.username,pass,device_label_from(req,ip),ip);
        }
        if(r.error==AuthError::UsernameTaken)
            return fail("That username is taken. Pick another.");
        if(r.error==AuthError::PasswordTooShort)
            return fail("Password needs at least eight characters.");
        if(r.error!=AuthError::None||!r.session)
            return fail("Could not create that account.");
        return http_redir("/", set_cookie(r.session->id));
    }
    if(method=="POST"&&path=="/login") {
        auto f=parse_form(body);
        auto uname = trim_ws(f["username"]);
        auto r=login(g_store,g_limiter,uname,f["password"],device_label_from(req,ip),ip);
        if(r.error!=AuthError::None||!r.session) {
            const char* code = (r.error==AuthError::RateLimited) ? "ratelimit" : "login";
            std::string loc = "/login?err=" + std::string(code) + "&username=" + url_encode(uname);
            auto next = safe_next_path(trim_ws(f["next"]));
            if (!next.empty()) loc += "&next=" + url_encode(next);
            return http_redir(loc);
        }
        auto dest = safe_next_path(trim_ws(f["next"]));
        if (dest.empty()) dest = "/feed";
        return http_redir(dest, set_cookie(r.session->id));
    }
    // Always the same response whether or not the address is on an
    // account -- checked deliberately before ever looking anything up,
    // not just by coincidence of the code path. Never let mail-delivery
    // status (or even the timing of a real lookup+send vs. a no-op)
    // leak back to the caller; that's exactly how account enumeration
    // via a password-reset form usually happens.
    if(method=="POST"&&path=="/forgot") {
        auto f = parse_form(body);
        std::string email = lowercase_ascii(trim_ws(f["email"]));
        if (looks_like_email(email)) {
            if (auto username = g_store.username_by_email(email)) {
                std::string token = generate_session_id(); // same crypto-random 32-byte-hex primitive as a session id
                g_store.set_reset_token(*username, sha256_hex_of(token), now() + 3600);
                send_password_reset_email(email, "https://liquidgov.us/reset?t=" + token);
            }
        }
        return pg("Forgot password", "<div class=\"card\"><h1>Check your email</h1>"
            "<p class=\"muted\">If that address is on an account, a link is on its way. "
            "It works once, for the next hour.</p>"
            "<p class=\"muted\"><a href=\"/login\">Back to log in</a></p></div>");
    }
    if(method=="POST"&&path=="/reset") {
        auto f = parse_form(body);
        std::string t = trim_ws(f["t"]);
        std::string pass = f["password"];
        if (t.empty()) return http_redir("/reset?err=reset");
        if (pass.size() < 8) return http_redir("/reset?t="+url_encode(t)+"&err=short");
        if (g_breach.is_breached(pass)) return http_redir("/reset?t="+url_encode(t)+"&err=password");
        // consume_reset_token erases it either way (found-and-expired or
        // found-and-valid) -- a token is good for exactly one attempt,
        // successful or not, same as any other one-time credential.
        auto username = g_store.consume_reset_token(sha256_hex_of(t));
        if (!username) return http_redir("/reset?err=reset");
        auto acc2 = g_store.find_by_username(*username);
        if (!acc2) return http_redir("/reset?err=reset");
        g_store.set_password_hash(acc2->id, hash_password(pass));
        logout_all_devices(g_store, acc2->id, "");
        auto session = create_session(g_store, acc2->id, device_label_from(req, ip));
        return http_redir("/", set_cookie(session.id));
    }
    // Google Identity Services posts here directly (data-ux_mode="redirect"
    // in google_signin_button_html()) -- no custom JS callback, just a
    // plain form POST carrying the ID token as "credential". Verified
    // against Google's own tokeninfo endpoint (a plain HTTPS GET, no JWT/
    // JWKS library needed) rather than trusting anything the browser says
    // about itself.
    if(method=="POST"&&path=="/login/google") {
        const char* client_id = google_client_id_from_env();
        if (!client_id[0]) return http_redir("/login?err=google");
        auto f = parse_form(body);
        std::string credential = trim_ws(f["credential"]);
        if (credential.empty()) return http_redir("/login?err=google");

        int curl_exit = 0;
        std::string resp = curl_get(
            "https://oauth2.googleapis.com/tokeninfo?id_token=" + url_encode(credential), 8, &curl_exit);
        // nothing came back (no network, timeout): say Google could not be reached, not that the sign-in was bad
        if (curl_exit != 0 && resp.empty()) return http_redir("/login?err=google-down");
        if (resp.empty() || resp[0] != '{') return http_redir("/login?err=google");

        std::string aud = oembed_json_string_field(resp, "aud");
        std::string iss = oembed_json_string_field(resp, "iss");
        std::string sub = oembed_json_string_field(resp, "sub");
        std::string email = lowercase_ascii(oembed_json_string_field(resp, "email"));
        std::string email_verified = oembed_json_string_field(resp, "email_verified");

        if (aud != client_id || sub.empty()) return http_redir("/login?err=google");
        if (iss != "accounts.google.com" && iss != "https://accounts.google.com")
            return http_redir("/login?err=google");
        if (email_verified != "true") return http_redir("/login?err=google");

        const std::string device_label = device_label_from(req, ip);

        // gid hit: this Google identity is already linked to an account.
        if (auto existing = g_store.username_by_google_sub(sub)) {
            auto acc2 = g_store.find_by_username(*existing);
            if (!acc2) return http_redir("/login?err=google");
            auto session = create_session(g_store, acc2->id, device_label);
            return http_redir("/", set_cookie(session.id));
        }
        // gid miss, ml hit: same email as an existing password account --
        // link it (never auto-link on anything looser than a verified
        // email match).
        if (!email.empty()) {
            if (auto linked = g_store.username_by_email(email)) {
                auto acc2 = g_store.find_by_username(*linked);
                if (acc2) {
                    g_store.set_google_id(*linked, sub);
                    auto session = create_session(g_store, acc2->id, device_label);
                    return http_redir("/", set_cookie(session.id));
                }
            }
        }
        // Neither: brand new account, no phone required on this path
        // (bypasses create_account()'s phone-required contract on
        // purpose -- see username_from_email_local_part's own comment --
        // by calling the raw AccountStore::create directly, same as
        // every other FileStore-only account field this file sets right
        // after signup).
        PlatformAccount account;
        account.username = username_from_email_local_part(email.empty() ? sub : email);
        account.password_hash = dummy_hash();
        account.phone_verified = false;
        account.created_at = now();
        AccountId new_id = g_store.create(account);
        if (!email.empty()) g_store.set_email(account.username, email);
        g_store.set_google_id(account.username, sub);
        g_store.set_map_prefs(account.username, MapPrefs{}); // today's defaults written down: no "now show" note later
        auto session = create_session(g_store, new_id, device_label);
        return http_redir("/", set_cookie(session.id));
    }
    if(method=="POST"&&path=="/me/post") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto c = read_composer_post(hdrs, body);
        if (!c.ok) return http_redir(std::string("/me?err=")+c.err);
        if (!c.text.empty() || !c.attachment_blob_id.empty())
            g_store.add_post(user, c.text, "", "", 0, c.attachment_blob_id, c.attachment_mime);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/repost") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t pid = 0;
        try { pid = std::stoull(f["post_id"]); } catch (...) { return http_redir("/me"); }
        auto target = g_store.post_by_id(pid);
        if (!target) return http_redir("/me");
        // A repost of a repost still credits the true original author, not
        // the intermediate reposter -- see the repost_of_author comment on
        // Post.
        const std::string& origAuthor = target->repost_of_author.empty() ? target->author : target->repost_of_author;
        std::uint64_t origId = target->repost_of_author.empty() ? target->id : target->repost_of_id;
        if (g_store.is_blocked(user, origAuthor)) return http_redir("/me");
        std::string blob_id, mime;
        if (!target->attachment_blob_id.empty()) {
            // put() is content-addressed and idempotent -- re-putting the
            // same bytes lands on the exact same blob id while correctly
            // incrementing its refcount for this new post referencing it.
            auto bytes = g_blobs.get(target->attachment_blob_id);
            if (bytes) {
                blob_id = g_blobs.put(*bytes, target->attachment_mime);
                mime = target->attachment_mime;
            }
        }
        g_store.add_post(user, target->text, "", origAuthor, origId, blob_id, mime);
        return http_redir(back_to_post(header_value(req, "Referer"), pid)); // the page it was tapped on (was /me)
    }
    // Reply and second both need to land back on whatever page the post was
    // actually being viewed on (feed, a profile, a community) -- post_block
    // renders in all of those, so unlike most POST handlers here a fixed
    // redirect target would be wrong more often than not. Referer, same
    // pattern the theme toggle already uses, falling back to /feed.
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>12&&
       path.compare(path.size()-6,6,"/reply")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-6)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        auto text = trim_ws(parse_form(body)["text"]);
        if (!text.empty()) g_store.add_reply(pid, user, text);
        return http_redir(back_to_post(back, pid));
    }
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>13&&
       path.compare(path.size()-7,7,"/second")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-7)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        g_store.toggle_second(pid, user);
        return http_redir(back_to_post(back, pid));
    }
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>13&&
       path.compare(path.size()-7,7,"/delete")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-7)); } catch (...) { return http_redir(back); }
        auto target = g_store.post_by_id(pid);
        if (!target) return http_redir(back); // already gone
        if (target->author != user) return http_code("403 Forbidden", "not your post");
        g_store.delete_post(pid, user);
        return http_redir(back);
    }
    if(method=="POST"&&path=="/me/profile") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto display = trim_ws(f["display_name"]);
        if (display.empty() || display.size() > 80) return http_redir("/me?err=profile");
        Profile p = g_store.profile_of(user);
        p.display_name = display;
        p.hometown = trim_ws(f["hometown"]).substr(0, 80);
        p.living = trim_ws(f["living"]).substr(0, 80);
        p.interests = trim_ws(f["interests"]).substr(0, 200);
        p.career = trim_ws(f["career"]).substr(0, 80);
        p.school = trim_ws(f["school"]).substr(0, 80);
        g_store.set_profile(user, p);
        g_store.set_blurb(acc->id, trim_ws(f["bio"]).substr(0, 500));
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/css") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        const std::string& raw_css = f["css"];
        auto check = sanitize_author_css(raw_css);
        if (!check.ok) return http_redir("/me?err=css&reason=" + check.code);
        g_store.set_author_css(acc->id, raw_css);
        return http_redir("/me?ok=css");
    }
    if(method=="POST"&&path=="/me/avatar") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "photo");
        if (!file || file->empty()) return http_redir("/me?err=image");
        auto r = save_avatar(g_blobs, g_store, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me?err=portraitlarge");
        if (r.error == SaveAvatarError::FailedScan) return http_redir("/me?err=scan");
        if (r.error != SaveAvatarError::None) return http_redir("/me?err=image");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/avatar/clear") {
        if(!acc) return need_login();
        clear_avatar(g_blobs, g_store, acc->id);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/canvas/clear") {
        if(!acc) return need_login();
        g_store.clear_canvas_frames(acc->id);
        return http_redir("/me/canvas");
    }
    if(method=="POST"&&path=="/me/canvas/frame") {
        if(!acc) return need_login();
        // Reuses save_avatar's exact normalize/scan/size-cap pipeline (the
        // security-relevant part) via a throwaway AvatarStore that just
        // captures the resulting blob id instead of writing the account's
        // real single avatar slot -- canvas frames are an ordered list,
        // not a single value, so FileStore's own AvatarStore side can't be
        // reused directly for this.
        struct CaptureBlobId : AvatarStore {
            std::optional<Avatar> get(AccountId) const override { return std::nullopt; }
            void set(const Avatar& a) override { blob_id = a.blob_id; }
            void erase(AccountId) override {}
            std::string blob_id;
        } capture;
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "frame");
        if (!file || file->empty()) return http_redir("/me/canvas?err=frameimage");
        if (g_store.canvas_frames(acc->id).size() >= kCanvasMaxFrames)
            return http_redir("/me/canvas?err=framefull");
        auto r = save_avatar(g_blobs, capture, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me/canvas?err=framelarge");
        if (r.error != SaveAvatarError::None) return http_redir("/me/canvas?err=frameimage");
        g_store.add_canvas_frame(acc->id, capture.blob_id);
        return http_ok("ok");
    }
    if(method=="POST"&&path=="/me/canvas/mode") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        g_store.set_canvas_timelapse(acc->id, f["timelapse"]=="1");
        return http_redir("/me/canvas?ok=1");
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
    if(method=="POST"&&path=="/feed") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto c = read_composer_post(hdrs, body);
        if (!c.ok) return http_redir(std::string("/feed?err=")+c.err);
        auto text = trim_ws(c.text);
        if (!text.empty() || !c.attachment_blob_id.empty()) {
            // anybody/friends/groups and the except/friend/group lists come
            // from feed_composer_card's own hidden fields and checkbox
            // groups -- absent (JS-off, or an older cached page) means the
            // same static defaults that markup ships with: Friends, all
            // friends, no group, exactly today's one and only behavior
            // before this existed.
            std::string ctype = header_value(hdrs, "Content-Type");
            bool anybody, friends_on, groups_on;
            std::vector<std::string> except_raw, friend_raw, group_raw;
            if (ctype.find("multipart/form-data") != std::string::npos) {
                anybody = multipart_field(ctype, body, "anybody") == "1";
                friends_on = multipart_field(ctype, body, "friends") == "1";
                groups_on = multipart_field(ctype, body, "groups") == "1";
                except_raw = multipart_field_all(ctype, body, "except");
                friend_raw = multipart_field_all(ctype, body, "friend");
                group_raw = multipart_field_all(ctype, body, "group");
            } else {
                auto f = parse_form(body);
                anybody = f["anybody"] == "1";
                friends_on = f["friends"] == "1";
                groups_on = f["groups"] == "1";
                except_raw = parse_form_multi(body, "except");
                friend_raw = parse_form_multi(body, "friend");
                group_raw = parse_form_multi(body, "group");
            }
            // Anybody is exclusive; "nothing chosen" falls back to today's
            // only behavior (Friends, all friends) -- never trust the
            // client's own idea of these three past this normalization.
            if (anybody) { friends_on = false; groups_on = false; }
            if (!anybody && !friends_on && !groups_on) friends_on = true;

            const auto my_friends = g_store.friends_of(user);
            auto is_a_friend = [&](const std::string& n) {
                return std::find(my_friends.begin(), my_friends.end(), n) != my_friends.end();
            };
            std::vector<std::string> exceptions, friend_allowlist, groups;
            if (anybody) {
                for (const auto& n : except_raw) if (is_a_friend(trim_ws(n))) exceptions.push_back(trim_ws(n));
            }
            if (friends_on) {
                for (const auto& n : friend_raw) if (is_a_friend(trim_ws(n))) friend_allowlist.push_back(trim_ws(n));
            }
            // A group needs an actual, current membership -- same
            // join-to-post rule as posting from the community's own page
            // (see POST /c/<slug>). A checked slug that isn't (anymore) a
            // real membership is just dropped, not trusted from the client.
            if (groups_on) {
                for (const auto& s : group_raw) {
                    auto slug = trim_ws(s);
                    if (!slug.empty() && g_store.is_member(slug, user)) groups.push_back(slug);
                }
                if (groups.empty()) return http_redir("/feed?err=nogroup");
            }

            std::string community = groups.empty() ? "" : groups.front();
            std::vector<std::string> extra_groups(groups.empty() ? groups : std::vector<std::string>(groups.begin()+1, groups.end()));
            const PostAudience audience = anybody ? PostAudience::Anybody
                : (groups_on && !friends_on) ? PostAudience::GroupsOnly
                : PostAudience::Friends;
            const auto new_post = g_store.add_post(user, text, community, "", 0, c.attachment_blob_id, c.attachment_mime,
                              audience, exceptions, friend_allowlist, extra_groups);
            // Reposts are someone else's words. Anything else this person
            // wrote is noticed: Anybody can be Flowed; the rest stay private.
            if (new_post) {
                Post noted;
                noted.id = new_post;
                noted.audience = audience;
                noted.community = community;
                noted.extra_groups = extra_groups;
                notice_pains(user, text, post_pain_source(noted));
            }
        }
        return http_redir("/feed");
    }
    if(method=="POST"&&path=="/proposals") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto title = trim_ws(f["title"]);
        auto ptext = trim_ws(f["body"]);
        if (title.empty() || ptext.empty()) return http_redir("/proposals?err=empty");
        BallotSpec spec;
        int kind = 0;
        try { kind = std::stoi(f["kind"]); } catch (...) { kind = 0; }
        if (kind < 0 || kind > 4) kind = 0;
        spec.kind = static_cast<BallotKind>(kind);
        if (spec.kind == BallotKind::Number) {
            spec.unit = trim_ws(f["unit"]);
            if (spec.unit.size() > 24) spec.unit.resize(24);
            auto mins = trim_ws(f["min"]);
            auto maxs = trim_ws(f["max"]);
            if (!mins.empty() || !maxs.empty()) {
                try { spec.min_v = std::stod(mins); } catch (...) { return http_redir("/proposals?err=range"); }
                try { spec.max_v = std::stod(maxs); } catch (...) { return http_redir("/proposals?err=range"); }
                if (!(spec.min_v < spec.max_v)) return http_redir("/proposals?err=range");
            }
        } else if (spec.kind == BallotKind::Choice) {
            spec.open_choice = !trim_ws(f["open"]).empty();
            std::istringstream in(f["options"]);
            std::string line;
            while (std::getline(in, line)) {
                auto o = trim_ws(line);
                if (!o.empty()) spec.options.push_back(o);
            }
            const std::size_t mn = spec.open_choice ? 1 : 2;
            if (spec.options.size() < mn || spec.options.size() > 12) return http_redir("/proposals?err=options");
            if (!ballot_options_unique(spec.options)) return http_redir("/proposals?err=dupe");
        } else if (spec.kind == BallotKind::Scale) {
            auto left = trim_ws(f["left"]);
            auto right = trim_ws(f["right"]);
            spec.options = {left, right};
        }
        const std::string city_text = trim_ws(f["city"]);
        const std::string who_text = trim_ws(f["who"]);
        std::string city_key_chosen;
        if (!city_text.empty()) {
            city_key_chosen = city_key(clean_city(city_text));
            if (city_key_chosen.empty() || g_store.communities_in_city(city_key_chosen).empty())
                return http_redir("/proposals?err=pcity");
        }
        auto id = g_store.add_proposal(user, title, ptext, spec);
        if (id == 0) return http_redir("/proposals?err=range");
        notice_pains(user, title + ". " + ptext, "prop:" + std::to_string(id));
        if (!city_key_chosen.empty()) g_store.set_proposal_city(id, city_text);
        if (!who_text.empty()) g_store.set_proposal_actor(id, who_text);
        if (!city_key_chosen.empty()) g_store.set_proposal_tags(id, parse_form_multi(body, "tag"));
        if (!city_key_chosen.empty()) return http_redir("/proposals?city="+url_encode(city_key_chosen));
        return http_redir("/proposals");
    }
    if(method=="POST"&&path=="/proposals/option") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        if (!g_store.add_proposal_option(id, user, trim_ws(f["label"]))) {
            // back at the proposal, with the real reason when the list is full (Brief 7 item 5)
            const bool full = p->ballot.kind == BallotKind::Choice && p->ballot.options.size() >= 48;
            return http_redir(back_to_proposal(trim_ws(f["next"]), *p, full ? "optfull" : "option"));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="GET"&&path=="/officials") {
        std::string cards;
        for (const auto& pr : g_store.all_proposals()) {
            if (!pr.is_office) continue;
            cards += "<div class=\"card\" id=\"proposal-"+std::to_string(pr.id)+"\"><h2>"+html_escape(pr.title)+"</h2>"
                     "<p class=\"muted\">"+html_escape(pr.jurisdiction.empty()?pr.body:pr.jurisdiction)+"</p>";
            const std::string next_in = user.empty() ? std::string()
                : "<input type=\"hidden\" name=\"next\" value=\"/officials\">";
            // The same Choice body as a proposal: index payload, "name · count", your check mark (Brief 7 item 1).
            cards += choice_ballot_body(pr, user, next_in, "candidate");
            cards += proposal_talk(pr, user, next_in);
            cards += "</div>";
        }
        if (cards.empty()) cards = "<p class=\"muted\">No races yet.</p>";
        std::string form;
        if (acc) {
            form = "<div class=\"card\"><h2>Open a race</h2>"
                   "<p class=\"muted\">Anyone with an account can vote. Not a certified election.</p>"
                   "<form method=\"POST\" action=\"/officials\">"
                   "<label>Office</label><input name=\"title\" maxlength=\"80\" required placeholder=\"Mayor\">"
                   "<label>Place</label><input name=\"place\" maxlength=\"80\" required placeholder=\"Fresno, CA\">"
                   "<label>Candidates (one per line)</label>"
                   "<textarea name=\"options\" maxlength=\"800\" placeholder=\"One name per line\"></textarea>"
                   "<label class=\"row\" style=\"gap:.4rem;align-items:center;margin-top:.4rem\">"
                   "<input type=\"checkbox\" name=\"open\" value=\"1\" checked> Allow people to add a candidate</label>"
                   "<button type=\"submit\">Open race</button></form></div>";
        } else {
            form = "<p class=\"muted\"><a href=\"/login?next=/officials\">Log in</a> to vote or open a race.</p>";
        }
        return http_ok(wrap("Officials", user, unread,
            std::string("<h1>Officials</h1>"
            "<p class=\"muted\">Account vote only. Not a certified election and not proof of residency.</p>")
            + how_proposals_work_html() + flash_error(qs)
            + cards + form + card_flash_script(qs)));
    }
    if(method=="POST"&&path=="/officials") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto title = trim_ws(f["title"]);
        auto place = trim_ws(f["place"]);
        if (title.empty() || title.size() > 80 || place.empty() || place.size() > 80)
            return http_redir("/officials");
        BallotSpec spec;
        spec.kind = BallotKind::Choice;
        spec.open_choice = !trim_ws(f["open"]).empty();
        std::istringstream in(f["options"]);
        std::string line;
        while (std::getline(in, line)) {
            auto o = trim_ws(line);
            if (!o.empty()) spec.options.push_back(o);
        }
        const std::size_t mn = spec.open_choice ? 1 : 2;
        if (spec.options.size() < mn || spec.options.size() > 12)
            return http_redir("/officials?err=options");
        if (!ballot_options_unique(spec.options)) return http_redir("/officials?err=dupe");
        auto nid = g_store.add_proposal(user, title, place, spec);
        if (!nid || !g_store.mark_proposal_office(nid, place))
            return http_redir("/officials");
        return http_redir("/officials");
    }
    if(method=="POST"&&path=="/proposals/reply") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        auto text = trim_ws(f["text"]);
        if (!text.empty()) {
            const auto rid = g_store.add_proposal_reply(id, user, text);
            if (rid) notice_pains(user, text, "prply:" + std::to_string(rid));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/proposals/react") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        const std::string kind = trim_ws(f["kind"]);
        auto cur = g_store.proposal_reaction_of(id, user);
        if (kind.empty() || (cur && *cur == kind)) g_store.set_proposal_reaction(id, user, "");
        else g_store.set_proposal_reaction(id, user, kind);
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/proposals/vote") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        // A refused answer says so at the proposal (Brief 7 item 5); a Yes/No choice that is not 0-2 is refused, not
        // turned into Skip.
        if (p->ballot.kind == BallotKind::YesNo) {
            const std::string c = trim_ws(f["choice"]);
            if (c != "0" && c != "1" && c != "2") return http_redir(back_to_proposal(trim_ws(f["next"]), *p, "vote"));
            g_store.cast_proposal_vote(id, user, static_cast<ProposalVote>(c[0] - '0'));
        } else if (!g_store.cast_ballot_value(id, user, trim_ws(f["value"]))) {
            return http_redir(back_to_proposal(trim_ws(f["next"]), *p, "vote"));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/friends") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto who=trim_ws(f["who"]);
        if(valid_username(who) && who!=user && g_store.find_by_username(who)
           && !g_store.is_blocked(user, who) && !g_store.is_blocked(who, user)) {
            g_store.add_friend_request(user, who);
        }
        auto dest = safe_next_path(trim_ws(f["next"]));
        if (dest.empty()) dest = "/friends";
        return http_redir(dest);
    }
    if(method=="POST"&&path=="/friends/accept") {
        if(!acc) return need_login();
        auto who=parse_form(body)["who"];
        g_store.accept_friend_request(who, user);
        if (g_store.incoming_requests(user).empty())
            g_store.mark_notices_read_of_type(user, "friend_request");
        return http_redir("/friends");
    }
    // Decline (2026-11-10): the request just goes away; the sender is not told (same as never answering).
    if(method=="POST"&&path=="/friends/decline") {
        if(!acc) return need_login();
        auto who=trim_ws(parse_form(body)["who"]);
        g_store.decline_friend_request(who, user);
        if (g_store.incoming_requests(user).empty())
            g_store.mark_notices_read_of_type(user, "friend_request");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/unfriend") {
        if(!acc) return need_login();
        g_store.unfriend(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/block") {
        if(!acc) return need_login();
        auto who=parse_form(body)["who"];
        if (valid_username(who) && who!=user) g_store.block(user, who);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner") {
        if(!acc) return need_login();
        if (!g_store.add_inner(user, parse_form(body)["who"]))
            return http_redir("/friends?err=inner");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner-remove") {
        if(!acc) return need_login();
        g_store.remove_inner(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/mail") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        std::string ctype = header_value(hdrs, "Content-Type");
        std::string to, text;
        if (ctype.find("multipart/form-data") != std::string::npos) {
            to = multipart_field(ctype, body, "to");
            text = multipart_field(ctype, body, "text");
        } else {
            auto f = parse_form(body);
            to = f["to"]; text = f["text"];
        }
        const MailUpload up = take_mail_upload(ctype, body); // releases the body
        if(text.size()>2000) text.resize(2000);
        // Do not open stranger DMs: friends-only, both directions unblocked,
        // exactly as before attachments existed.
        if(to.empty() || !g_store.are_friends(user,to) || g_store.is_blocked(user,to) || g_store.is_blocked(to,user))
            return http_redir("/mail?with="+to+"&err=notfriends");

        std::optional<std::string> blob_id, mime;
        std::string download_name;
        if (up.file && !up.file->empty()) {
            const std::string code = ingest_mail_attachment(user, up, blob_id, mime, download_name);
            if (!code.empty()) return http_redir("/mail?with="+to+"&err="+code);
        }
        if (!text.empty() || blob_id) {
            g_store.add_mail(user,to,text,blob_id,mime);
            if (!text.empty()) {
                const auto thread = g_store.mail_with(user, to);
                for (auto it = thread.rbegin(); it != thread.rend(); ++it) {
                    if (it->from == user && it->text == text) {
                        notice_pains(user, text, "mail:" + std::to_string(it->ts) + ":" + to);
                        break;
                    }
                }
            }
            if (!download_name.empty()) { // named once the message exists (review 2026-10-27)
                const auto thread = g_store.mail_with(user, to);
                for (auto it = thread.rbegin(); it != thread.rend(); ++it)
                    if (it->from == user && it->attachment_blob_id == blob_id) {
                        g_store.set_attachment_name(attachment_name_key(*it), download_name);
                        break;
                    }
            }
        }
        return http_redir("/mail?with="+to);
    }

    if(method=="POST"&&path=="/mail/group/new") {
        if(!acc) return need_login();
        auto form = parse_form(body);
        const auto who = form["who"];
        auto title = trim_ws(form["title"]);
        if (title.size() > 80) title.resize(80);
        std::vector<AccountId> invitees;
        std::vector<std::string> skipped;
        std::string cur;
        auto flush_tok = [&]() {
            const auto name = trim_ws(cur);
            cur.clear();
            if (name.empty() || name == user) return;
            auto found = g_store.find_by_username(name);
            if (!found || g_store.is_blocked(user, name) || g_store.is_blocked(name, user)) {
                skipped.push_back(name);
                return;
            }
            for (auto id : invitees) if (id == found->id) return;
            invitees.push_back(found->id);
        };
        for (char c : who) {
            if (c==','||c==' '||c=='\t'||c=='\n'||c=='\r') flush_tok();
            else cur += c;
        }
        flush_tok();
        auto result = create_conversation(g_store, g_social_graph, acc->id, invitees, /*is_group=*/true);
        auto conv = g_store.get_conversation(*result.conversation_id);
        if (conv && !title.empty()) {
            conv->title = title;
            g_store.save_conversation(*conv);
        }
        if (conv) {
            std::set<AccountId> kept;
            for (auto& m : conv->members) kept.insert(m.account);
            for (auto id : invitees) {
                if (!kept.count(id)) {
                    if (auto a = g_store.find_by_id(id)) skipped.push_back(a->username);
                } else if (auto a = g_store.find_by_id(id)) {
                    auto pr = g_store.prefs_of(a->username);
                    if (pr.notify_message) g_store.add_notice(a->username, "mail_group", user, std::to_string(conv->id));
                }
            }
        }
        std::string loc = "/mail/g/"+std::to_string(*result.conversation_id);
        if (!skipped.empty()) {
            loc += "?skipped=";
            for (size_t i=0;i<skipped.size();++i) {
                if (i) loc += ",";
                loc += url_encode(skipped[i]);
            }
        }
        return http_redir(loc);
    }

    if(method=="POST"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return need_login();
        std::string rest = path.substr(8);
        auto slash = rest.find('/');
        const std::string id_str = (slash==std::string::npos) ? rest : rest.substr(0,slash);
        const std::string action = (slash==std::string::npos) ? std::string() : rest.substr(slash+1);
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(id_str); } catch(...) { return http_redir("/mail"); }
        auto conv = g_store.get_conversation(conv_id);
        if (!conv || !conv->is_group) return http_redir("/mail");
        const ConversationMember* self_member = nullptr;
        for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        const std::string base = "/mail/g/"+id_str;

        if (action=="accept") {
            if (!self_member) return http_redir("/mail");
            accept_invite(g_store, acc->id, conv_id);
            return http_redir(base);
        }
        if (action=="leave") {
            if (!self_member) return http_redir("/mail");
            leave_conversation(g_store, acc->id, conv_id); // Pending "decline" and Active "leave" alike; never notified
            return http_redir("/mail");
        }
        // Everything below requires being an Active member.
        if (!self_member || self_member->state != MemberState::Active) return http_redir("/mail");

        if (action=="invite") {
            const auto who = trim_ws(parse_form(body)["who"]);
            auto found = g_store.find_by_username(who);
            bool refuse = !found || g_store.is_blocked(user, who) || g_store.is_blocked(who, user);
            if (!refuse) {
                auto r = add_member(g_store, g_social_graph, acc->id, found->id, conv_id);
                refuse = r.error != MessengerError::None;
                if (!refuse) {
                    auto pr = g_store.prefs_of(found->username);
                    if (pr.notify_message) g_store.add_notice(found->username, "mail_group", user, id_str);
                }
            }
            return http_redir(refuse ? (base+"?err=invite") : base);
        }
        if (action=="title") {
            auto title = trim_ws(parse_form(body)["title"]);
            if (title.size() > 80) title.resize(80);
            conv->title = title;
            g_store.save_conversation(*conv);
            return http_redir(base);
        }
        if (action=="send") {
            auto hdr_end = req.find("\r\n\r\n");
            std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
            std::string ctype = header_value(hdrs, "Content-Type");
            std::string text;
            if (ctype.find("multipart/form-data") != std::string::npos) {
                text = multipart_field(ctype, body, "text");
            } else {
                text = parse_form(body)["text"];
            }
            const MailUpload up = take_mail_upload(ctype, body); // releases the body
            text = trim_ws(text);
            if (text.size()>2000) text.resize(2000);
            std::optional<std::string> blob_id, mime;
            std::string download_name;
            if (up.file && !up.file->empty()) {
                const std::string code = ingest_mail_attachment(user, up, blob_id, mime, download_name);
                if (!code.empty()) return http_redir(base+"?err="+code);
            }
            if (!text.empty() || blob_id) {
                auto sent = send_message(g_store, acc->id, conv_id, text);
                if (sent.message_id && !text.empty())
                    notice_pains(user, text, "gmsg:" + std::to_string(*sent.message_id));
                if (sent.message_id && blob_id) {
                    auto msg = g_store.get_message(*sent.message_id);
                    if (msg) {
                        msg->attachment_blob_id = *blob_id;
                        msg->attachment_mime = mime.value_or("");
                        g_store.update_message(*msg);
                        if (!download_name.empty()) // named once the message exists (review 2026-10-27)
                            g_store.set_attachment_name(attachment_name_key(*msg), download_name);
                    }
                } else if (blob_id) {
                    g_blobs.release(*blob_id); // not sent (left the group, blocked...): don't keep up to 100 MB for nothing
                }
            }
            return http_redir(base);
        }
        if (action.rfind("message/",0)==0) {
            const std::string mrest = action.substr(8);
            const auto mslash = mrest.find('/');
            if (mslash==std::string::npos) return http_redir(base);
            MessageId msg_id = 0;
            try { msg_id = std::stoull(mrest.substr(0,mslash)); } catch(...) { return http_redir(base); }
            const std::string mact = mrest.substr(mslash+1);
            if (mact=="edit") {
                auto text = trim_ws(parse_form(body)["text"]);
                if (!text.empty()) {
                    if (text.size()>2000) text.resize(2000);
                    edit_message(g_store, acc->id, msg_id, text);
                }
            } else if (mact=="delete") {
                delete_message(g_store, acc->id, msg_id, /*for_everyone=*/true);
            }
            return http_redir(base);
        }
        return http_redir(base);
    }
    if(method=="POST"&&path=="/communities") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto slug=trim_ws(f["slug"]);
        auto name=trim_ws(f["name"]);
        auto about=f["about"]; if(about.size()>2000) about.resize(2000);
        int cat = 10;
        try { cat = std::stoi(f["category"]); } catch (...) { cat = 10; }
        if (cat < 0 || cat > 10) cat = 10;
        auto subcategory = trim_ws(f["subcategory"]);
        if (subcategory.size() > 60) subcategory.resize(60);
        if (subcategory.empty()) return http_redir("/communities?err=subcategory");
        Community c;
        c.slug = slug; c.name = name.empty()?slug:name; c.owner = user; c.about = about; c.created_at = now();
        c.category = static_cast<CommunityCategory>(cat);
        c.subcategory = subcategory;
        c.city = f["city"]; // optional; FileStore tidies it (clean_city) and drops text with no letters
        if (!g_store.create_community(c)) return http_redir("/communities?err=address");
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path.rfind("/c/",0)==0) {
        if(!acc) return need_login();
        std::string rest = path.substr(3);
        std::string slug, action;
        auto slash = rest.find('/');
        if (slash==std::string::npos) { slug = rest; action = "post"; }
        else { slug = rest.substr(0,slash); action = rest.substr(slash+1); }
        if (!g_store.community(slug)) return http_redir("/communities");
        if (action=="join") { g_store.join_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="city") {
            // Owner only (set_community_city checks); empty clears it.
            auto f=parse_form(body);
            if (!g_store.set_community_city(slug, user, f["city"])) return http_redir("/c/"+slug+"?err=city");
            return http_redir("/c/"+slug);
        }
        if (action=="leave") { g_store.leave_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="delete") { g_store.delete_community(slug, user); return http_redir("/communities"); }
        if (action=="invite") {
            auto who=trim_ws(parse_form(body)["who"]);
            if (!g_store.invite_community(slug, user, who)) return http_redir("/c/"+slug+"?err=invite");
            return http_redir("/c/"+slug);
        }
        if (action=="post") {
            if (!g_store.is_member(slug, user)) return http_redir("/c/"+slug);
            auto hdr_end = req.find("\r\n\r\n");
            std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
            auto c = read_composer_post(hdrs, body);
            if (!c.ok) return http_redir("/c/"+slug+"?err="+c.err);
            auto text = trim_ws(c.text);
            if (!text.empty() || !c.attachment_blob_id.empty()) {
                const auto gid = g_store.add_post(user, text, slug, "", 0, c.attachment_blob_id, c.attachment_mime);
                if (gid) notice_pains(user, text, "gpost:" + std::to_string(gid));
            }
            return http_redir("/c/"+slug);
        }
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path=="/settings/revoke") {
        if(!acc) return need_login();
        auto target = parse_form(body)["sid"];
        bool current = target == sid;
        bool owned = false;
        for (const auto& s : g_store.sessions_for(acc->id)) {
            if (s.id == target) { owned = true; break; }
        }
        if (owned) logout(g_store, target);
        if (current) return http_redir("/login", clear_cookie());
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/logout-all") {
        if(!acc) return need_login();
        logout_all_devices(g_store, acc->id, sid);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/password") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        // A Google-only account has no real password yet -- its hash is
        // the same dummy_hash() placeholder login() uses for a timing-
        // safe "unknown username" rejection, which by design never
        // verifies against anything. Skip the current-password check
        // only for that exact placeholder; a real account still has to
        // prove its current password like always.
        const bool has_real_password = acc->password_hash != dummy_hash();
        if ((has_real_password && !verify_password(f["current"], acc->password_hash)) || f["next"].empty())
            return http_redir("/settings/password?err=password");
        if (f["next"].size() < 8)
            return http_redir("/settings/password?err=short");
        if (g_breach.is_breached(f["next"]))
            return http_redir("/settings/password?err=password");
        g_store.set_password_hash(acc->id, hash_password(f["next"]));
        return http_redir("/settings?ok=password");
    }
    if(method=="POST"&&path=="/settings/map") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        MapPrefs mp;
        mp.show = f["show"] == "1";
        mp.groups = f["groups"] == "1";
        mp.posts = f["posts"] == "1";
        mp.comments = f["comments"] == "1";
        mp.friends = f["friends"] == "1";
        mp.pains = f["pains"] == "1";
        g_store.set_map_prefs(user, mp); // pains off deletes what it found
        return http_redir("/settings#map-privacy");
    }
    if(method=="POST"&&path=="/settings/privacy") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        int fv = 1;
        try { fv = std::stoi(f["findability"]); } catch (...) { fv = 1; }
        if (fv < 0 || fv > 2) fv = 1;
        p.findability = static_cast<Findability>(fv);
        p.group_invite_confirm = f["group_invite_confirm"] == "1";
        p.presence_hide = f["presence_hide"] == "1";
        auto vis_of = [&](const std::string& field, Visibility fallback) {
            int v = static_cast<int>(fallback);
            try { v = std::stoi(f[field]); } catch (...) { v = static_cast<int>(fallback); }
            if (v < 0 || v > 3) v = static_cast<int>(fallback);
            return static_cast<Visibility>(v);
        };
        p.profile_visibility = vis_of("profile_visibility", p.profile_visibility);
        p.posts_visibility = vis_of("posts_visibility", p.posts_visibility);
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/notify") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        p.notify_friend = f["notify_friend"]=="1";
        p.notify_message = f["notify_message"]=="1";
        p.notify_group = f["notify_group"]=="1";
        p.intro_every_visit = f["intro_every_visit"]=="1";
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/unblock") {
        if(!acc) return need_login();
        g_store.unblock(user, parse_form(body)["who"]);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/block") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto who = trim_ws(f["who"]);
        if (valid_username(who) && who != user) g_store.block(user, who, trim_ws(f["reason"]));
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/delete") {
        if(!acc) return need_login();
        auto confirm = trim_ws(parse_form(body)["confirm"]);
        if (confirm != user) return http_redir("/settings?err=delete");
        clear_avatar(g_blobs, g_store, acc->id);
        g_store.delete_account(acc->id);
        return http_redir("/", clear_cookie());
    }

    if(method=="GET"&&path=="/admin") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_overview_page());
    }
    if(method=="GET"&&path=="/admin/accounts") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_accounts_page(user, trim_ws(qs["q"])));
    }
    if(method=="GET"&&path=="/admin/posts") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_posts_page(qs.count("q") ? trim_ws(qs.at("q")) : ""));
    }
    if(method=="GET"&&path=="/admin/communities") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_communities_page());
    }
    if(method=="GET"&&path=="/admin/proposals") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string msg = qs.count("done") ? (qs.at("done") == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : qs.at("done") == "edited" ? "Saved." : qs.at("done") == "archive-failed"
                              ? "Not deleted: the moderation archive could not be written (see the server log)." : "Nothing changed.") : "";
        return pg("Operator tools", admin_proposals_page(msg));
    }
    if(method=="POST"&&path=="/admin/proposals/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        std::uint64_t pid = 0;
        try { pid = std::stoull(parse_form(body)["id"]); } catch (...) { return http_redir("/admin/proposals?done=none"); }
        auto target = g_store.proposal_by_id(pid);
        if (!target) return http_redir("/admin/proposals?done=none");
        if (!archive_deleted_proposal(*target, user)) return http_redir("/admin/proposals?done=archive-failed");
        g_store.delete_proposal(pid);
        g_store.add_audit_log(user, "delete_proposal", std::to_string(pid) + " by " + target->author + ": " + target->title.substr(0, 60));
        return http_redir("/admin/proposals?done=deleted");
    }
    if(method=="GET"&&path=="/admin/comments") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string done = qs.count("done") ? qs.at("done") : "";
        const std::string msg = done == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : done == "archive-failed" ? "Not deleted: the moderation archive could not be written (see the server log)."
                              : done.empty() ? "" : "Nothing changed.";
        return pg("Operator tools", admin_comments_page(qs.count("q") ? trim_ws(qs.at("q")) : "", msg));
    }
    if(method=="POST"&&path=="/admin/comments/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        std::uint64_t cid = 0;
        try { cid = std::stoull(f["id"]); } catch (...) { return http_redir("/admin/comments?done=none"); }
        if (f["on"] == "post") {
            auto r = g_store.post_reply_by_id(cid);
            if (!r) return http_redir("/admin/comments?done=none");
            if (!archive_deleted_comment("post", r->id, r->post_id, r->author, r->ts, r->text, user))
                return http_redir("/admin/comments?done=archive-failed");
            g_store.delete_post_reply(cid);
            g_store.add_audit_log(user, "delete_comment", "post comment " + std::to_string(cid) + " by " + r->author);
        } else if (f["on"] == "proposal") {
            auto r = g_store.proposal_reply_by_id(cid);
            if (!r) return http_redir("/admin/comments?done=none");
            if (!archive_deleted_comment("proposal", r->id, r->proposal_id, r->author, r->ts, r->text, user))
                return http_redir("/admin/comments?done=archive-failed");
            g_store.delete_proposal_reply(cid);
            g_store.add_audit_log(user, "delete_comment", "proposal comment " + std::to_string(cid) + " by " + r->author);
        } else {
            return http_redir("/admin/comments?done=none");
        }
        return http_redir("/admin/comments?done=deleted");
    }
    if(method=="GET"&&path=="/admin/departments") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string done = qs.count("done") ? qs.at("done") : "";
        const std::string msg = done == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : done == "archive-failed" ? "Not deleted: the moderation archive could not be written (see the server log)."
                              : done.empty() ? "" : "Nothing changed.";
        return pg("Operator tools", admin_departments_page(qs.count("q") ? trim_ws(qs.at("q")) : "", msg));
    }
    if(method=="POST"&&path=="/admin/departments/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        const std::string key = f["city"], tag = f["tag"];
        bool known = false;
        for (const auto& t : g_store.city_tags(key)) if (t == tag) { known = true; break; }
        if (key.empty() || !known) return http_redir("/admin/departments?done=none");
        const auto props = g_store.proposals_with_city_tag(key, tag);
        if (!archive_deleted_city_tag(key, tag, props, user)) return http_redir("/admin/departments?done=archive-failed");
        if (!g_store.delete_city_tag(key, tag)) return http_redir("/admin/departments?done=none");
        g_store.add_audit_log(user, "delete_city_tag", key + ": " + tag.substr(0, 80) + " (" + std::to_string(props.size()) + " proposal(s))");
        return http_redir("/admin/departments?done=deleted");
    }
    if(method=="POST"&&path=="/admin/proposals/edit") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        std::uint64_t pid = 0;
        try { pid = std::stoull(f["id"]); } catch (...) { return http_redir("/admin/proposals?done=none"); }
        const std::string title = trim_ws(f["title"]).substr(0, 200);
        const std::string text = f["body"].substr(0, 10000);
        if (!g_store.edit_proposal(pid, title, text)) return http_redir("/admin/proposals?done=none");
        g_store.add_audit_log(user, "edit_proposal", std::to_string(pid));
        return http_redir("/admin/proposals?done=edited");
    }
    if(method=="GET"&&path=="/admin/log") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_log_page());
    }
    if(method=="POST"&&path=="/admin/accounts/lock") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && target != user && g_store.find_by_username(target)) {
            g_store.set_locked(target, true);
            g_store.add_audit_log(user, "lock", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/programs") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto form = parse_form(body);
        auto target = trim_ws(form["username"]);
        if (!target.empty() && g_store.find_by_username(target) && !is_operator(target)) {
            ProgramGrant g;
            g.username = target;
            for (const auto& k : program_kinds())
                if (!form[std::string("k_") + k.slug].empty()) g.kinds |= static_cast<std::uint32_t>(k.kind);
            std::uint64_t mb = 0;
            try { mb = std::stoull(trim_ws(form["max_mb"])); } catch (...) { mb = 0; }
            const std::uint64_t ceiling_mb = program_ceiling_bytes() / (1024 * 1024);
            mb = std::max<std::uint64_t>(1, std::min<std::uint64_t>(mb, ceiling_mb));
            g.max_bytes = mb * 1024 * 1024;
            g.granted_by = user;
            g.granted_at = now();
            const bool had = g_store.program_grant(target).has_value();
            g_store.set_program_grant(g);
            if (g.kinds) g_store.add_audit_log(user, "programs_grant", target);
            else if (had) g_store.add_audit_log(user, "programs_revoke", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/unlock") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && g_store.find_by_username(target)) {
            g_store.set_locked(target, false);
            g_store.add_audit_log(user, "unlock", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/logout-all") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && target != user) {
            if (auto ta = g_store.find_by_username(target)) {
                logout_all_devices(g_store, ta->id, "");
                g_store.add_audit_log(user, "logout_all", target);
            }
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        auto target = trim_ws(f["username"]);
        auto confirm = trim_ws(f["confirm"]);
        if (!target.empty() && target != user && confirm_says_delete(confirm)) {
            if (auto ta = g_store.find_by_username(target)) {
                clear_avatar(g_blobs, g_store, ta->id);
                g_store.delete_account(ta->id);
                g_store.add_audit_log(user, "delete_account", target);
            }
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/posts/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        std::uint64_t pid = 0;
        try { pid = std::stoull(parse_form(body)["id"]); } catch (...) { return http_redir("/admin/posts"); }
        if (auto target = g_store.post_by_id(pid)) {
            g_store.delete_post(pid, target->author);
            g_store.add_audit_log(user, "delete_post", std::to_string(pid));
        }
        return http_redir("/admin/posts");
    }
    if(method=="POST"&&path=="/admin/communities/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        auto slug = trim_ws(f["slug"]);
        auto confirm = trim_ws(f["confirm"]);
        if (!slug.empty() && confirm_says_delete(confirm)) {
            if (auto c = g_store.community(slug)) {
                g_store.delete_community(slug, c->owner);
                g_store.add_audit_log(user, "delete_community", slug);
            }
        }
        return http_redir("/admin/communities");
    }

    return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><h1>Not found</h1><p>That page is not here.</p></div>"));
}

}  // namespace liquidgov::site
