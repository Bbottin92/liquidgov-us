#pragma once
// auth_routes.hpp: the auth feature's HTTP routes (split out of handle(); see site/router.cpp).
#include "site/core/route.hpp"

namespace liquidgov::site {

// The response for a auth route, or nullopt when the request is not one of this feature's routes.
std::optional<std::string> auth_routes(Route& r);

}  // namespace liquidgov::site
