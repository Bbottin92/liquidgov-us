#include "site/features/live/live_routes.hpp"
#include "site/core/http.hpp"

namespace liquidgov::site {

std::optional<std::string> live_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/world") {
        // Old Quad bookmarks land on the closed Commons page.
        return http_redir("/commons/");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
