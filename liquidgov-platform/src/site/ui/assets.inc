// Asset URLs with a content version (2026-11-07 iPhone sweep): every script and stylesheet was served no-store and
// re-downloaded on every page view, and the hand-typed ?v=N had drifted (one place ?v=14, another none). asset_url
// returns "/assets/<name>?v=<hash of the file>". The hash is redone whenever the file's modification time changes, not
// only at startup: a file copied in by hand without a restart still gets a new URL, so no phone can stay on an old copy
// for the year it may cache it (side-agent catch). A request that carries ?v= may be cached for a year (asset_cache_header).
std::string asset_url(const std::string& name) {
    static std::mutex mu;
    static std::map<std::string, std::pair<long long, std::string>> done;   // name -> (mtime ns, url)
    std::lock_guard<std::mutex> lock(mu);
    struct stat st{};
    const long long mtime = ::stat(("assets/" + name).c_str(), &st) == 0
        ? static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec : -1;
    auto it = done.find(name);
    if (it != done.end() && it->second.first == mtime) return it->second.second;
    std::ifstream in("assets/" + name, std::ios::binary);
    std::string url = "/assets/" + name;
    if (in) {
        std::ostringstream ss; ss << in.rdbuf();
        const std::string data = ss.str();
        unsigned long long h = 1469598103934665603ULL;            // FNV-1a 64
        for (unsigned char c : data) { h ^= c; h *= 1099511628211ULL; }
        char hex[17];
        std::snprintf(hex, sizeof hex, "%016llx", h);
        url += "?v=" + std::string(hex, 10);
    } else {
        // missing file: no version, so it is never cached for a year; the route itself answers 404
        std::cerr << "asset_url: assets/" << name << " not readable; serving it unversioned\n";
    }
    done[name] = {mtime, url};
    return url;
}

// Cache-Control for an asset request: a versioned URL (?v=...) never changes, so a year and immutable; a bare one
// (an old page, a bookmark) keeps today's no-store so it can never go stale.
std::string asset_cache_header(const std::string& rawpath) {
    return rawpath.find("?v=") != std::string::npos || rawpath.find("&v=") != std::string::npos
        ? "Cache-Control: public, max-age=31536000, immutable\r\n"
        : "Cache-Control: no-store\r\n";
}

