#include "site/features/live/live_routes.hpp"
#include "site/core/http.hpp"

namespace liquidgov::site {

std::optional<std::string> live_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    // (/world, the old Quad bookmark, redirected to the Commons; it went with the Commons, 2026-11-13.)
    return std::nullopt;
}

}  // namespace liquidgov::site
