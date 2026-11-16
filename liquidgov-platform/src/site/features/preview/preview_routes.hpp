#pragma once
// preview_routes.hpp: GET /preview-image, the picture on the site's link preview (set on /admin/preview; preview.hpp).
// Public on purpose: Facebook, X and the like fetch it without signing in.
#include "site/core/route.hpp"

namespace liquidgov::site {

std::optional<std::string> preview_routes(Route& r);

}  // namespace liquidgov::site
