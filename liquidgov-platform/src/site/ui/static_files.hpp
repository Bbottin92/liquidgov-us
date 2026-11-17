#pragma once
// static_files.hpp: every static file the site serves, in one table, and the one function that serves them
// (2026-11-17, website cleanup slice 1: this replaces about twenty hand-copied "open the file, read it, label it,
// attach a Cache-Control line" blocks in assets_routes.cpp, map_routes.cpp and push_routes.cpp).
#include "site/common.hpp"

namespace liquidgov::site {

// The answer for a GET of `path` if it is one of the table's static files, else nullopt (the caller carries on to
// its other routes). `rawpath` is the request path with its query, which decides versioned caching (?v=...).
// `range_header` is the request's Range header value ("" when none); only the intro video honours it.
std::optional<std::string> serve_static(const std::string& path, const std::string& rawpath,
                                        const std::string& range_header);

// Contents of a file under the working directory. Kept in memory and re-read only when the file's modification time
// or size changes, so a file copied in by hand without a restart is still served fresh (same rule as asset_url).
// Returns false when the file cannot be read.
bool read_file_cached(const std::string& file, std::string& out);

}  // namespace liquidgov::site
