#pragma once
// system_routes.hpp: the system feature's HTTP routes (split out of handle(); see site/router.cpp).
#include "site/core/route.hpp"

namespace liquidgov::site {

// The response for a system route, or nullopt when the request is not one of this feature's routes.
std::optional<std::string> system_routes(Route& r);

}  // namespace liquidgov::site
