#include "site/features/preview/preview.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/http.hpp"
#include "site/core/text.hpp"

#include <sys/stat.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>

namespace liquidgov::site {

namespace {

std::string settings_path() { return std::string(store_path_from_env()) + ".preview"; }
std::string image_path() { return std::string(store_path_from_env()) + ".preview-image"; }

// one line per field: name TAB value, with backslash, newline and tab escaped in the value
std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else o += c;
    }
    return o;
}
std::string unesc(const std::string& s) {
    std::string o;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            o += s[i] == 'n' ? '\n' : s[i] == 't' ? '\t' : s[i];
        } else {
            o += s[i];
        }
    }
    return o;
}

// trimmed, no control characters (a newline becomes a space), cut to `max` bytes on a whole character
std::string clean(const std::string& in, std::size_t max) {
    std::string o;
    for (unsigned char c : in) {
        if (c == '\n' || c == '\r' || c == '\t') { if (!o.empty() && o.back() != ' ') o += ' '; continue; }
        if (c < 0x20 || c == 0x7f) continue;
        o += static_cast<char>(c);
    }
    o = trim_ws(o);
    if (o.size() > max) {
        o.resize(max);
        while (!o.empty() && (static_cast<unsigned char>(o.back()) & 0xC0) == 0x80) o.pop_back();   // not mid-character
        if (!o.empty() && (static_cast<unsigned char>(o.back()) & 0x80)) o.pop_back();               // drop a cut lead byte
    }
    return o;
}

bool write_atomic(const std::string& path, const std::string& bytes) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) { std::remove(tmp.c_str()); return false; }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) { std::remove(tmp.c_str()); return false; }
    return true;
}

std::mutex g_mu;
SitePreview g_cached;
std::chrono::steady_clock::time_point g_at{};
bool g_have = false;

struct Raw { std::string title, description, mime; };
Raw read_settings() {
    Raw r;
    std::ifstream in(settings_path(), std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string k = line.substr(0, tab), v = unesc(line.substr(tab + 1));
        if (k == "title") r.title = clean(v, kPreviewTitleMax);
        else if (k == "description") r.description = clean(v, kPreviewDescMax);
        else if (k == "image_mime") r.mime = (v == "image/jpeg" || v == "image/png") ? v : "";
    }
    return r;
}
bool write_settings(const Raw& r) {
    std::ostringstream os;
    os << "title\t" << esc(r.title) << "\n" << "description\t" << esc(r.description) << "\n" << "image_mime\t" << esc(r.mime) << "\n";
    return write_atomic(settings_path(), os.str());
}
void forget() { std::lock_guard<std::mutex> lock(g_mu); g_have = false; }

}  // namespace

std::string preview_builtin_description() {
    // the landing hero plus the limit the page itself states: a voluntary association, not a replacement for the
    // government you already live under (the old Myspace joke stays on the genesis quote card)
    return "This is YOUR government! Represent yourself \xE2\x80\x94 a voluntary association, not a replacement for the one you already live under.";
}
// v4 is a new path because v3 (and v2 before it) is cached immutable for a year on crawlers
std::string preview_builtin_image_url() { return "https://liquidgov.us/assets/og-image-v4.jpg"; }

SitePreview site_preview() {
    std::lock_guard<std::mutex> lock(g_mu);
    const auto now = std::chrono::steady_clock::now();
    if (g_have && now - g_at < std::chrono::seconds(1)) return g_cached;
    SitePreview p;
    const Raw r = read_settings();
    p.title = r.title; p.description = r.description;
    struct stat st{};
    if (!r.mime.empty() && ::stat(image_path().c_str(), &st) == 0 && st.st_size > 0) {
        p.image_mime = r.mime;
        p.image_version = std::to_string(static_cast<long long>(st.st_mtime)) + "-" + std::to_string(static_cast<long long>(st.st_size));
    }
    g_cached = p; g_at = now; g_have = true;
    return p;
}

bool site_preview_save_text(const std::string& title, const std::string& description) {
    Raw r = read_settings();
    r.title = clean(title, kPreviewTitleMax);
    r.description = clean(description, kPreviewDescMax);
    const bool ok = write_settings(r);
    forget();
    return ok;
}

std::string site_preview_set_image(const std::vector<std::uint8_t>& b) {
    if (b.size() > kPreviewImageMax) return "size";
    std::string mime;
    if (b.size() >= 4 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') mime = "image/png";
    else if (b.size() >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) mime = "image/jpeg";
    else return "type";
    if (!write_atomic(image_path(), std::string(b.begin(), b.end()))) return "write";
    Raw r = read_settings();
    r.mime = mime;
    const bool ok = write_settings(r);
    forget();
    return ok ? "" : "write";
}

void site_preview_remove_image() {
    Raw r = read_settings();
    r.mime.clear();
    write_settings(r);
    std::remove(image_path().c_str());
    forget();
}

void site_preview_reset() {
    std::remove(settings_path().c_str());
    std::remove(image_path().c_str());
    forget();
}

std::optional<std::pair<std::string, std::string>> site_preview_image() {
    const SitePreview p = site_preview();
    if (p.image_mime.empty()) return std::nullopt;
    std::ifstream in(image_path(), std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return std::make_pair(ss.str(), p.image_mime);
}

}  // namespace liquidgov::site
