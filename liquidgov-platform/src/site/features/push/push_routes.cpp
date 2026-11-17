#include "site/features/push/push_routes.hpp"
#include "site/core/http.hpp"
#include "site/features/live/live_sessions.hpp"
#include "site/features/push/push_subs.hpp"
#include "site/ui/assets.hpp"

namespace liquidgov::site {

std::optional<std::string> push_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
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
    return std::nullopt;
}

}  // namespace liquidgov::site
