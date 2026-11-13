#pragma once
// styles_routes.hpp: the profile style library's HTTP routes (/styles...; see site/router.cpp).
#include "site/core/route.hpp"

namespace liquidgov::site {

// The response for a style library route, or nullopt when the request is not one of this feature's routes.
std::optional<std::string> styles_routes(Route& r);

}  // namespace liquidgov::site
