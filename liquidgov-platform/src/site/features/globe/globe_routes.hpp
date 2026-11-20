#pragma once
// globe_routes.hpp: the map globes' pages, /globe/<kind> (map globes G1, Brandon 2026-11-20; see site/router.cpp).
#include "site/core/route.hpp"

namespace liquidgov::site {

// The response for a /globe/ route, or nullopt when the request is not one of this feature's routes.
std::optional<std::string> globe_routes(Route& r);

}  // namespace liquidgov::site
