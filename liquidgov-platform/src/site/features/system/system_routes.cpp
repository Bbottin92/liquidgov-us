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

    // (The four same-box /internal/ routes the Commons daemon used, session-user, friends, blocked and user-exists,
    // were removed 2026-11-15 with the Commons; git tag commons-final / branch archive/commons-final has them.)
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
