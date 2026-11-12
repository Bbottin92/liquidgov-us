#include "site/features/notices/notices_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/http.hpp"
#include "site/features/notices/notices.hpp"

namespace liquidgov::site {

std::optional<std::string> notices_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
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
    return std::nullopt;
}

}  // namespace liquidgov::site
