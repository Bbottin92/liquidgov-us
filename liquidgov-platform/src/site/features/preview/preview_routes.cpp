#include "site/features/preview/preview_routes.hpp"
#include "site/core/http.hpp"
#include "site/features/preview/preview.hpp"

namespace liquidgov::site {

std::optional<std::string> preview_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if (method == "GET" && path == "/preview-image") {
        auto img = site_preview_image();
        if (!img) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found\n");
        // the address carries ?v=<version>, so a changed image is a new address: it may be cached for a day
        return http_raw("200 OK", img->second.c_str(), img->first, "Cache-Control: public, max-age=86400\r\n");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
