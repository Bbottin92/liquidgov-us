#include "site/features/assets/assets_routes.hpp"
#include "site/core/http.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/favicon_cookies.hpp"
#include "site/ui/static_files.hpp"

namespace liquidgov::site {

std::optional<std::string> assets_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/favicon.ico")
        return http_redir("/favicon.svg");
    // A day of caching (2026-11-08: it had no Cache-Control, so every page fetched it again; measured in headless Edge).
    // The icon only changes with a deploy, and a day-old tab icon is harmless.
    if(method=="GET"&&path=="/favicon.svg")
        return http_raw("200 OK","image/svg+xml", favicon_svg(), "Cache-Control: public, max-age=86400\r\n");

    if(method=="GET"&&path=="/manifest.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"LiquidGov.US\","
            "\"short_name\":\"LiquidGov\","
            "\"description\":\"A voluntary natural democracy -- civic association and working software.\","
            "\"start_url\":\"/\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#8a6620\","
            // an installed app is offered in the phone's Share menu (feed_routes.cpp /share fills the post box)
            "\"share_target\":{\"action\":\"/share\",\"method\":\"GET\",\"params\":{\"title\":\"title\",\"text\":\"text\",\"url\":\"url\"}},"
            "\"icons\":[{\"src\":\"/icon-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"},{\"src\":\"/favicon.svg\",\"sizes\":\"any\",\"type\":\"image/svg+xml\",\"purpose\":\"any\"}]"
            "}");
    }

    // Linked only from a page rendered with title "Messages" (currently
    // just /mail -- see page_wrap's manifest href branch). start_url is
    // /mail, not /, so installing *from that page* on a manifest-reading
    // platform (Chrome/Edge/Android; iOS Safari ignores this file
    // entirely and has no install banner of its own) opens straight back
    // into the inbox instead of the Genesis landing page.
    if(method=="GET"&&path=="/manifest-mail.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Messages\","
            "\"short_name\":\"Messages\","
            "\"description\":\"LiquidGov.US mail and conversations.\","
            "\"start_url\":\"/mail\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#d1a85c\","
            "\"icons\":[{\"src\":\"/icon-mail-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-mail-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }

    // Same pattern as /manifest-mail.webmanifest, linked only from a page rendered with title "Social" or "Civic"
    // (the /app/social and /app/civic install shells, which /apps sends people to). start_url is the real app page,
    // exactly where each shell already sends a standalone launch (see assets/app-social.html / app-civic.html), so an
    // installed Social opens the feed and an installed Civic opens proposals instead of the Genesis landing page.
    // Icons and theme colours are the ones page_wrap already uses for those titles. (iOS ignores manifests.)
    if(method=="GET"&&path=="/manifest-social.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Social\","
            "\"short_name\":\"Social\","
            "\"description\":\"LiquidGov.US feed, people, and groups.\","
            "\"start_url\":\"/feed\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#c17a4a\","
            "\"icons\":[{\"src\":\"/icon-social-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-social-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }
    if(method=="GET"&&path=="/manifest-civic.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Civic\","
            "\"short_name\":\"Civic\","
            "\"description\":\"LiquidGov.US proposals, laws, and who holds control.\","
            "\"start_url\":\"/proposals\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#6b7d5a\","
            "\"icons\":[{\"src\":\"/icon-civic-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-civic-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }

    // Every static file (scripts, stylesheets, icons, share images, the intro video, the service worker) comes from the
    // one table in site/ui/static_files.cpp; tests/e2e/asset_routes.sh is its spec.
    if(method=="GET") {
        if(auto s = serve_static(path, rawpath, header_value(req, "Range"))) return s;
    }
    if(method=="GET"&&(path=="/theme/dark"||path=="/theme/light")) {
        const std::string mode = path.substr(7);
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/";
        std::string extra = "Set-Cookie: lg_theme="+mode+"; Path=/; SameSite=Lax; Max-Age=31536000";
        if (g_https) extra += "; Secure";
        extra += "\r\n";
        return http_redir(back, extra);
    }
    // Settings > Your droplet: hide or show your droplet in this browser (a cookie, like the theme), then back.
    if(method=="GET"&&(path=="/drop/off"||path=="/drop/on")) {
        // back to the page you came from, but only a page on this site (any other Referer: Settings)
        std::string back = header_value(req, "Referer"), host = header_value(req, "Host");
        if (host.empty() || (back.rfind("https://" + host + "/", 0) != 0 && back.rfind("http://" + host + "/", 0) != 0))
            back = "/settings#drop";
        std::string extra = path=="/drop/off" ? "Set-Cookie: lg_drop=off; Path=/; SameSite=Lax; Max-Age=31536000"
                                              : "Set-Cookie: lg_drop=; Path=/; SameSite=Lax; Max-Age=0";
        if (g_https) extra += "; Secure";
        extra += "\r\n";
        return http_redir(back, extra);
    }
    // Settings > Adult videos: a shared adult video plays without its "tap to show" cover in this browser, or with it.
    if(method=="GET"&&(path=="/nsfw/on"||path=="/nsfw/off")) {
        std::string back = header_value(req, "Referer"), host = header_value(req, "Host");
        if (host.empty() || (back.rfind("https://" + host + "/", 0) != 0 && back.rfind("http://" + host + "/", 0) != 0))
            back = "/settings#nsfw";
        std::string extra = path=="/nsfw/on" ? "Set-Cookie: lg_nsfw=on; Path=/; SameSite=Lax; Max-Age=31536000"
                                             : "Set-Cookie: lg_nsfw=; Path=/; SameSite=Lax; Max-Age=0";
        if (g_https) extra += "; Secure";
        extra += "\r\n";
        return http_redir(back, extra);
    }
    if(method=="GET"&&(path=="/install"||path=="/apps")) {
        std::ifstream in("assets/apps.html"); std::ostringstream body; if(in) body<<in.rdbuf();
        return pg("Get the app", body.str());
    }
    // The separate Messages/Social/Civic apps are gone (one app now). Their old
    // install pages send people to the real page those apps opened.
    if(method=="GET"&&path=="/app/mail") return http_redir("/mail");
    if(method=="GET"&&path=="/app/social") return http_redir("/feed");
    if(method=="GET"&&path=="/app/civic") return http_redir("/proposals");
    return std::nullopt;
}

}  // namespace liquidgov::site
