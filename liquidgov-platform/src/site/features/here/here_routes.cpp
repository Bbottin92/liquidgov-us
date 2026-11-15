#include "site/features/here/here_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/http.hpp"
#include "site/features/here/here.hpp"

namespace liquidgov::site {

namespace {
// Only a path on this site: "/..." but never "//host" or a backslash trick. Anything else goes to Settings.
std::string safe_back(const std::string& b) {
    if (b.empty() || b[0] != '/' || (b.size() > 1 && (b[1] == '/' || b[1] == '\\')) || b.size() > 200) return "/settings#here";
    for (char c : b) if (static_cast<unsigned char>(c) < 0x20 || c == '\\') return "/settings#here";
    return b;
}
}  // namespace

std::optional<std::string> here_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if (method == "POST" && path == "/here/choice") {
        if (!acc) return need_login();
        auto f = parse_form(body);
        const std::string back = safe_back(f["back"]);
        // the profile-page card sends choice=; the Settings radios and the signup form send here=
        const std::string c = f.count("choice") ? f["choice"] : f["here"];
        if (c == "1" || c == "0") g_store.set_here_choice(user, c == "1");
        return http_redir(back);
    }
    // the browser extension's one-time socket ticket (here.hpp here_ticket_for). No CORS headers: only the extension
    // (which has this site's permission) can read it, never another website.
    if (method == "GET" && path == "/here/ticket") {
        if (!acc) return http_raw("401 Unauthorized", "application/json", "{\"error\":\"signed out\"}", "Cache-Control: no-store\r\n");
        const std::string t = here_ticket_for(user);
        if (t.empty()) return http_raw("503 Service Unavailable", "application/json", "{\"error\":\"busy\"}", "Cache-Control: no-store\r\n");
        return http_raw("200 OK", "application/json", "{\"ticket\":\"" + t + "\"}", "Cache-Control: no-store\r\n");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
