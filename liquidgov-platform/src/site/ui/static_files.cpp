#include "site/ui/static_files.hpp"
#include "site/core/http.hpp"
#include "site/ui/assets.hpp"

#include <sys/stat.h>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

namespace liquidgov::site {
namespace {

// How a file may be cached by the browser.
enum class Cache {
    Versioned,   // "...?v=<hash>" -> a year, immutable; the bare URL -> no-store (asset_cache_header decides)
    Day,         // icons: one day
    Year,        // share images, the intro video: a year, immutable (their URLs carry their own version)
    NoStore,     // the service worker: never cached
};

struct StaticFile {
    const char* url;      // the request path
    const char* file;     // the file under the working directory
    const char* ctype;    // Content-Type
    Cache cache;
    bool ranges = false;  // answers Range with 206 (the intro video)
    const char* extra = "";  // extra header lines, each ending \r\n, put before the Cache-Control line
};

constexpr const char* JS  = "text/javascript; charset=utf-8";
constexpr const char* CSS = "text/css; charset=utf-8";
constexpr const char* PNG = "image/png";
constexpr const char* JPG = "image/jpeg";

// THE table. Add a static file here and nowhere else. (tests/e2e/asset_routes.sh is the spec for every row.)
const StaticFile kFiles[] = {
    // scripts and stylesheets: versioned
    {"/assets/setup.js",        "assets/setup.js",        JS,  Cache::Versioned},
    {"/assets/mail_widget.js",  "assets/mail_widget.js",  JS,  Cache::Versioned},
    {"/assets/draft_publish.js","assets/draft_publish.js",JS,  Cache::Versioned},
    {"/assets/people_search.js","assets/people_search.js",JS,  Cache::Versioned},
    {"/assets/jukebox.js",      "assets/jukebox.js",      JS,  Cache::Versioned},
    {"/assets/here.js",         "assets/here.js",         JS,  Cache::Versioned},
    {"/assets/canvas.js",       "assets/canvas.js",       JS,  Cache::Versioned},
    {"/assets/walker.js",       "assets/walker.js",       JS,  Cache::Versioned},
    {"/assets/here_media.js",   "assets/here_media.js",   JS,  Cache::Versioned},
    {"/assets/lgmap_walk.js",   "assets/lgmap_walk.js",   JS,  Cache::Versioned},
    {"/assets/tip.js",          "assets/tip.js",          JS,  Cache::Versioned},
    {"/assets/home_drop.js",    "assets/home_drop.js",    JS,  Cache::Versioned},
    {"/assets/intro.js",        "assets/intro.js",        JS,  Cache::Versioned},
    {"/assets/video_links.js",  "assets/video_links.js",  JS,  Cache::Versioned},
    {"/assets/hints.js",        "assets/hints.js",        JS,  Cache::Versioned},
    {"/assets/juketube.js",     "assets/juketube.js",     JS,  Cache::Versioned},
    {"/assets/lgmap.js",        "assets/lgmap.js",        JS,  Cache::Versioned},
    {"/assets/push.js",         "assets/push.js",         JS,  Cache::Versioned},
    {"/assets/site.css",        "assets/site.css",        CSS, Cache::Versioned},
    {"/assets/setup.css",       "assets/setup.css",       CSS, Cache::Versioned},
    {"/assets/juketube.css",    "assets/juketube.css",    CSS, Cache::Versioned},
    {"/assets/lgmap.css",       "assets/lgmap.css",       CSS, Cache::Versioned},
    // the service worker: never cached; allowed to control the whole site
    {"/sw.js", "assets/sw.js", JS, Cache::NoStore, false, "Service-Worker-Allowed: /\r\n"},
    // icons: one day
    {"/apple-touch-icon.png",             "assets/apple-touch-icon.png", PNG, Cache::Day},
    {"/apple-touch-icon-precomposed.png", "assets/apple-touch-icon.png", PNG, Cache::Day},
    {"/assets/apple-touch-icon.png",      "assets/apple-touch-icon.png", PNG, Cache::Day},
    {"/icon-192.png",         "assets/icon-192.png",        PNG, Cache::Day},
    {"/assets/icon-192.png",  "assets/icon-192.png",        PNG, Cache::Day},
    {"/icon-512.png",         "assets/icon-512.png",        PNG, Cache::Day},
    {"/assets/icon-512.png",  "assets/icon-512.png",        PNG, Cache::Day},
    {"/icon-mail.png",        "assets/icon-mail.png",       PNG, Cache::Day},
    {"/icon-mail-192.png",    "assets/icon-mail-192.png",   PNG, Cache::Day},
    {"/icon-mail-512.png",    "assets/icon-mail-512.png",   PNG, Cache::Day},
    {"/icon-social.png",      "assets/icon-social.png",     PNG, Cache::Day},
    {"/icon-social-192.png",  "assets/icon-social-192.png", PNG, Cache::Day},
    {"/icon-social-512.png",  "assets/icon-social-512.png", PNG, Cache::Day},
    {"/icon-civic.png",       "assets/icon-civic.png",      PNG, Cache::Day},
    {"/icon-civic-192.png",   "assets/icon-civic-192.png",  PNG, Cache::Day},
    {"/icon-civic-512.png",   "assets/icon-civic-512.png",  PNG, Cache::Day},
    // share images (v1..v4, old shares still resolve) and the intro video: a year, immutable
    {"/assets/og-image-v4.jpg", "assets/og-image-v4.jpg", JPG, Cache::Year},
    {"/assets/og-image-v3.jpg", "assets/og-image-v3.jpg", JPG, Cache::Year},
    {"/assets/og-image-v2.jpg", "assets/og-image-v2.jpg", JPG, Cache::Year},
    {"/assets/og-image.png",    "assets/og-image.png",    PNG, Cache::Year},
    {"/assets/intro.mp4",       "assets/intro.mp4",       "video/mp4", Cache::Year, true},
};

std::string cache_line(Cache c, const std::string& rawpath) {
    switch (c) {
        case Cache::Versioned: return asset_cache_header(rawpath);
        case Cache::Day:       return "Cache-Control: public, max-age=86400\r\n";
        case Cache::Year:      return "Cache-Control: public, max-age=31536000, immutable\r\n";
        case Cache::NoStore:   return "Cache-Control: no-store\r\n";
    }
    return "Cache-Control: no-store\r\n";
}

// "bytes=<start>-<end>" against a body of `size` bytes. True only for a well-formed range inside the body;
// anything else (malformed, out of bounds) is false and the caller sends the whole file, never an error.
bool parse_range(const std::string& range, std::size_t size, std::size_t& start, std::size_t& end) {
    if (range.rfind("bytes=", 0) != 0) return false;
    const auto dash = range.find('-', 6);
    if (dash == std::string::npos) return false;
    try {
        const std::string s = range.substr(6, dash - 6), e = range.substr(dash + 1);
        start = s.empty() ? 0 : std::stoul(s);
        end = e.empty() ? size - 1 : std::stoul(e);
    } catch (...) { return false; }
    return start < size && end < size && start <= end;
}

}  // namespace

bool read_file_cached(const std::string& file, std::string& out) {
    struct Entry { long long mtime_ns; long long size; std::string data; };
    static std::mutex mu;
    static std::map<std::string, Entry> files;
    std::lock_guard<std::mutex> lock(mu);
    struct stat st{};
    if (::stat(file.c_str(), &st) != 0) return false;
    const long long mtime = static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec;
    auto it = files.find(file);
    if (it != files.end() && it->second.mtime_ns == mtime && it->second.size == static_cast<long long>(st.st_size)) {
        out = it->second.data;
        return true;
    }
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf();
    files[file] = Entry{mtime, static_cast<long long>(st.st_size), ss.str()};
    out = files[file].data;
    return true;
}

std::optional<std::string> serve_static(const std::string& path, const std::string& rawpath,
                                        const std::string& range_header) {
    for (const StaticFile& f : kFiles) {
        if (path != f.url) continue;
        std::string data;
        if (!read_file_cached(f.file, data)) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string headers = std::string(f.extra) + cache_line(f.cache, rawpath);
        if (f.ranges) {
            headers += "Accept-Ranges: bytes\r\n";
            std::size_t start = 0, end = 0;
            if (parse_range(range_header, data.size(), start, end)) {
                std::ostringstream h;
                h << headers << "Content-Range: bytes " << start << "-" << end << "/" << data.size() << "\r\n";
                return http_raw("206 Partial Content", f.ctype, data.substr(start, end - start + 1), h.str());
            }
        }
        return http_raw("200 OK", f.ctype, data, headers);
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
