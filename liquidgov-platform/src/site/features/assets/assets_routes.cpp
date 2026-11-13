#include "site/features/assets/assets_routes.hpp"
#include "site/core/http.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/favicon_cookies.hpp"

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

    if(method=="GET"&&(path=="/apple-touch-icon.png"||path=="/apple-touch-icon-precomposed.png"||path=="/icon-192.png"||path=="/icon-512.png"||path=="/assets/icon-192.png"||path=="/assets/icon-512.png"||path=="/assets/apple-touch-icon.png")) {
        std::string file="assets/apple-touch-icon.png";
        if(path.find("192")!=std::string::npos) file="assets/icon-192.png";
        else if(path.find("512")!=std::string::npos) file="assets/icon-512.png";
        std::ifstream in(file, std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","image/png", ss.str(), "Cache-Control: public, max-age=86400\r\n");
    }

    if(method=="GET"&&(path=="/icon-mail.png"||path=="/icon-social.png"||path=="/icon-civic.png"
        ||path=="/icon-mail-192.png"||path=="/icon-social-192.png"||path=="/icon-civic-192.png"
        ||path=="/icon-mail-512.png"||path=="/icon-social-512.png"||path=="/icon-civic-512.png")) {
        std::ifstream in(("assets"+path), std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","image/png", ss.str(), "Cache-Control: public, max-age=86400\r\n");
    }
    if(method=="GET"&&path=="/assets/setup.js") {
        std::ifstream in("assets/setup.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    // The site stylesheet (was css(), inlined into every page; a real file since 2026-11-12 so browsers cache it).
    if(method=="GET"&&path=="/assets/site.css") {
        std::ifstream in("assets/site.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/setup.css") {
        std::ifstream in("assets/setup.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    // Page scripts that were inlined into pages until 2026-11-12 (same code, now cached): one route for all of them.
    if(method=="GET"&&(path=="/assets/mail_widget.js"||path=="/assets/draft_publish.js"||path=="/assets/people_search.js"||
                       path=="/assets/jukebox.js"||path=="/assets/here.js"||path=="/assets/canvas.js")) {
        std::ifstream in(path.substr(1), std::ios::binary);   // "assets/<name>.js"
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/juketube.js") {
        std::ifstream in("assets/juketube.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/juketube.css") {
        std::ifstream in("assets/juketube.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/intro.mp4") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/intro.mp4", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        const std::string cache_hdr = "Cache-Control: public, max-age=31536000, immutable\r\nAccept-Ranges: bytes\r\n";
        // Real browsers request video with a Range header even on first
        // play; answering 200-whole-body every time (no 206 support) is
        // a genuine small smoothness cost, independent of anything else.
        // Falls straight through to the full 200 response on anything
        // malformed or out of bounds -- never worth 500ing a video load.
        const std::string range = header_value(req, "Range");
        if (range.rfind("bytes=", 0) == 0) {
            auto dash = range.find('-', 6);
            if (dash != std::string::npos) {
                try {
                    std::string start_s = range.substr(6, dash - 6);
                    std::string end_s = range.substr(dash + 1);
                    std::size_t start = start_s.empty() ? 0 : std::stoul(start_s);
                    std::size_t end = end_s.empty() ? cached.size() - 1 : std::stoul(end_s);
                    if (start < cached.size() && end < cached.size() && start <= end) {
                        std::string slice = cached.substr(start, end - start + 1);
                        std::ostringstream extra;
                        extra << cache_hdr << "Content-Range: bytes " << start << "-" << end << "/" << cached.size() << "\r\n";
                        return http_raw("206 Partial Content", "video/mp4", slice, extra.str());
                    }
                } catch (...) { /* fall through */ }
            }
        }
        return http_raw("200 OK", "video/mp4", cached, cache_hdr);
    }
    if(method=="GET"&&path=="/assets/og-image-v4.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v4.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // v3, kept so an old cached share still resolves. The tags now point at
    // og-image-v4.jpg (page_wrap).
    if(method=="GET"&&path=="/assets/og-image-v3.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v3.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    if(method=="GET"&&path=="/assets/og-image-v2.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v2.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // v1, kept so an old cached share of /assets/og-image.png still resolves.
    // The tags now point at og-image-v4.jpg (page_wrap).
    if(method=="GET"&&path=="/assets/og-image.png") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image.png", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/png", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
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
