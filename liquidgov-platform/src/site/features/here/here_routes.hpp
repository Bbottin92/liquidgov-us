#pragma once
// here_routes.hpp: the dots feature's HTTP routes (POST /here/choice; see site/router.cpp). The live room itself is a
// WebSocket, handled by here_ws_upgrade from main()'s accept loop like the other sockets.
#include "site/core/route.hpp"

namespace liquidgov::site {

std::optional<std::string> here_routes(Route& r);

}  // namespace liquidgov::site
