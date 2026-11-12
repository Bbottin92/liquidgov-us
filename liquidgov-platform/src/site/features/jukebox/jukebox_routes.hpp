#pragma once
// jukebox_routes.hpp: the jukebox feature's HTTP routes (split out of handle(); see site/router.cpp).
#include "site/core/route.hpp"

namespace liquidgov::site {

// The response for a jukebox route, or nullopt when the request is not one of this feature's routes.
std::optional<std::string> jukebox_routes(Route& r);

}  // namespace liquidgov::site
