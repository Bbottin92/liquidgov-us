#include "site/router.hpp"
#include "site/core/route.hpp"
#include "site/features/system/system_routes.hpp"
#include "site/features/assets/assets_routes.hpp"
#include "site/features/jukebox/jukebox_routes.hpp"
#include "site/features/mail/mail_routes.hpp"
#include "site/features/push/push_routes.hpp"
#include "site/features/pages/pages_routes.hpp"
#include "site/features/auth/auth_routes.hpp"
#include "site/features/live/live_routes.hpp"
#include "site/features/canvas/canvas_routes.hpp"
#include "site/features/feed/feed_routes.hpp"
#include "site/features/proposals/proposals_routes.hpp"
#include "site/features/profile/profile_routes.hpp"
#include "site/features/people/people_routes.hpp"
#include "site/features/communities/communities_routes.hpp"
#include "site/features/map/map_routes.hpp"
#include "site/features/notices/notices_routes.hpp"
#include "site/features/settings/settings_routes.hpp"
#include "site/features/admin/admin_routes.hpp"
#include "site/features/styles/styles_routes.hpp"
#include "site/core/state.hpp"
#include "site/core/adapters_store.hpp"
#include "site/ui/favicon_cookies.hpp"
#include "site/core/http.hpp"
#include "site/ui/wrap.hpp"

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
    if (path.size() > 1 && path.back() == '/') {
        std::string dest = path.substr(0, path.size() - 1);
        auto q = rawpath.find('?');
        if (q != std::string::npos) dest += rawpath.substr(q);
        return http_redir(dest);
    }

    Route route{req, peer_ip, body, method, rawpath, path, qs, ip, acc, user, sid, unread, pg, need_login, admin_404};
    // Each feature's routes, in the order they first appeared in handle(); the first that answers wins.
    if (auto out = system_routes(route)) return *out;
    if (auto out = assets_routes(route)) return *out;
    if (auto out = jukebox_routes(route)) return *out;
    if (auto out = mail_routes(route)) return *out;
    if (auto out = push_routes(route)) return *out;
    if (auto out = pages_routes(route)) return *out;
    if (auto out = auth_routes(route)) return *out;
    if (auto out = live_routes(route)) return *out;
    if (auto out = canvas_routes(route)) return *out;
    if (auto out = feed_routes(route)) return *out;
    if (auto out = proposals_routes(route)) return *out;
    if (auto out = profile_routes(route)) return *out;
    if (auto out = styles_routes(route)) return *out;
    if (auto out = people_routes(route)) return *out;
    if (auto out = communities_routes(route)) return *out;
    if (auto out = map_routes(route)) return *out;
    if (auto out = notices_routes(route)) return *out;
    if (auto out = settings_routes(route)) return *out;
    if (auto out = admin_routes(route)) return *out;


    return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><h1>Not found</h1><p>That page is not here.</p></div>"));
}

}  // namespace liquidgov::site
