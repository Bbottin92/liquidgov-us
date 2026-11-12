#include "site/features/system/system_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/http.hpp"
#include "site/core/text.hpp"
#include "site/features/mail/mail_attachments.hpp"

namespace liquidgov::site {

std::optional<std::string> system_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    // nginx's auth_request for the /mail/big upload door (nginx blocks
    // /internal/ from outside).
    if (path == "/internal/upload-ok") return upload_ok_response(req);    if(method=="GET"&&path=="/robots.txt")
        return http_raw("200 OK", "text/plain; charset=utf-8",
            "User-agent: *\nAllow: /\nDisallow: /settings\nDisallow: /mail\nDisallow: /export\nDisallow: /internal\n");

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
    return std::nullopt;
}

}  // namespace liquidgov::site
