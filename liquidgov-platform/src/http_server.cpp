// LiquidGov.US site — C++ HTTP + platform auth.
// Persistence: FileStore (LGP1 snapshot). Not liquidgov-core's ledger.
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/avatar.hpp"
#include "liquidgov/platform/blob.hpp"
#include "liquidgov/platform/clock.hpp"
#include "liquidgov/platform/file_store.hpp"
#include "liquidgov/platform/file_transfer.hpp"
#include "liquidgov/platform/file_type.hpp"
#include "liquidgov/platform/messenger.hpp"
#include "liquidgov/platform/websocket.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
using namespace liquidgov::platform;
namespace {
constexpr int kPort = 8080;
// Mail attachments run through file_transfer's default caps (image 10 MiB,
// document/PDF/OOXML 25 MiB — see SizeCaps in file_transfer.hpp). The read
// loop's ceiling has to cover the largest of those plus multipart overhead.
constexpr std::size_t kMaxBody = 25ull * 1024 * 1024 + 64 * 1024;
std::mutex g_mu;

// --- World: one shared room, connected users see each other move. ---
// Deliberately separate from g_mu / g_store: this is ephemeral in-memory
// position state, never persisted, touched only by world session threads
// (one per connected /world/ws client) — the rest of the server stays on
// its existing single-threaded accept loop, untouched by this feature.
struct WorldPlayer {
    double x = 128, y = 128;
    std::string username;
    std::string color;
};
std::mutex g_world_mu;
std::unordered_map<int, WorldPlayer> g_world_players; // keyed by client socket fd

std::string world_color_for(const std::string& username) {
    static const char* kPalette[] = {"#a23b3b", "#3b6ea2", "#3ba25e", "#a2823b",
                                      "#6e3ba2", "#3ba297", "#a23b8f", "#5f7a2e"};
    std::size_t h = std::hash<std::string>{}(username);
    return kPalette[h % 8];
}

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};
class AllowLimiter : public RateLimiter {
public:
    bool allow(const std::string&, const std::string&) override { return true; }
    void record_attempt(const std::string&, const std::string&) override {}
};
// Not a malware engine. Normalized avatars are re-encoded PNG; this
// rejects anything that is not a PNG signature. Always-true would be a lie.
class PngMagicScanner : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>& bytes) const override {
        static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        if (bytes.size() < 8) return false;
        return std::memcmp(bytes.data(), sig, 8) == 0;
    }
};
// Same category of placeholder as PngMagicScanner, generalized to every
// type ingest_file allows. Not a malware engine — a real deployment plugs
// one in here. This re-checks that the bytes actually being stored still
// match a known-good signature; always-true would be a lie.
class MailAttachmentScanner : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>& bytes) const override {
        return detect_type(bytes) != DetectedType::Unknown;
    }
};

// Adapter over FileStore's username-keyed friend graph, mapping through
// AccountId as messenger.hpp requires. is_minor has no AgeAssurance
// backing yet — returns false for everyone rather than fail-safe-true,
// per the closed pin: treating everyone as a minor would brick the site.
// The compensating control is that 1:1 mail stays friends-only regardless
// of this predicate (unchanged, enforced separately in the /mail routes
// below) — this predicate only ever matters for the group minor
// invariant, which is consequently unpaid until AgeAssurance lands.
class SiteSocialGraph : public SocialGraph {
public:
    bool are_friends(AccountId a, AccountId b) const override;
    bool is_minor(AccountId) const override { return false; }
};

const char* store_path_from_env() {
    const char* p = std::getenv("LIQUIDGOV_STORE");
    return p && p[0] ? p : "liquidgov_accounts.lgp";
}
const char* blob_path_from_env() {
    const char* p = std::getenv("LIQUIDGOV_BLOBS");
    return p && p[0] ? p : "liquidgov_blobs";
}
// Client-side YouTube search (Data API v3 search.list) needs a key. Read-only
// key, meant to be HTTP-referrer-restricted in Google Cloud Console to this
// site's origin — same trust level as any client-embedded map/analytics key.
// Empty means "not configured"; the search box degrades to a message instead
// of failing silently.
const char* youtube_api_key_from_env() {
    const char* p = std::getenv("YOUTUBE_API_KEY");
    return p && p[0] ? p : "";
}

FileStore g_store{store_path_from_env()};
FilesystemBlobStore g_blobs{blob_path_from_env()};
PngMagicScanner g_scanner;
MailAttachmentScanner g_mail_scanner;
SizeCaps g_mail_caps; // defaults from file_transfer.hpp
NeverBreached g_breach;
AllowLimiter g_limiter;
SiteSocialGraph g_social_graph;

bool SiteSocialGraph::are_friends(AccountId a, AccountId b) const {
    auto ua = g_store.find_by_id(a);
    auto ub = g_store.find_by_id(b);
    if (!ua || !ub) return false;
    return g_store.are_friends(ua->username, ub->username);
}

std::string html_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': o += '&'; o += "amp;"; break;
            case '<': o += '&'; o += "lt;"; break;
            case '>': o += '&'; o += "gt;"; break;
            case '"': o += '&'; o += "quot;"; break;
            default: o += c; break;
        }
    }
    return o;
}

// Quotes a server-controlled (not user-controlled) string as a single-quoted
// JS string literal embedded in generated HTML, e.g. the YouTube API key.
std::string js_string_literal(const std::string& s) {
    std::string o = "'";
    for (char c : s) {
        switch (c) {
            case '\'': o += "\\'"; break;
            case '\\': o += "\\\\"; break;
            case '<': o += "\\x3c"; break;
            default: o += c; break;
        }
    }
    o += "'";
    return o;
}

std::string css() {
    return
        ":root{--paper:#f7f1e4;--parchment:#efe8d8;--panel:#f3ecdd;--ink:#1a1814;"
        "--navy:#1e2d42;--stone:#6b6458;--rule:rgba(26,24,20,.14);--danger:#6b2424;"
        "--warn:#7a4a12;--ok:#1e3d32;--rsm:8px;--rmd:14px;--rlg:22px;"
        "--display:\"Iowan Old Style\",Palatino,\"Palatino Linotype\",Georgia,serif;"
        "--body:\"Segoe UI\",system-ui,-apple-system,sans-serif}"
        "*{box-sizing:border-box}html{-webkit-font-smoothing:antialiased}"
        "body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.5 var(--body)}"
        "a{color:var(--navy)}a:hover{color:var(--ink)}"
        "h1,h2,h3{font-family:var(--display);font-weight:600;letter-spacing:-.02em;"
        "text-wrap:balance;line-height:1.2}h1{font-size:clamp(1.6rem,1.2rem + 1.6vw,2.2rem);margin:.2rem 0 .8rem}"
        "h2{font-size:1.15rem;margin:1.4rem 0 .6rem}p{text-wrap:pretty}"
        ".wrap{max-width:44rem;margin:0 auto;padding:0 1.1rem 3.5rem}"
        "header.top{border-bottom:1px solid var(--rule);background:var(--parchment);"
        "position:sticky;top:0;z-index:5}"
        ".topinner{max-width:44rem;margin:0 auto;padding:.7rem 1.1rem;display:flex;"
        "align-items:center;flex-wrap:wrap;gap:.45rem 1rem}"
        ".brand{display:flex;align-items:center;gap:.65rem;text-decoration:none;color:var(--ink);margin-right:auto}"
        ".mark{width:28px;height:28px;border:1.5px solid var(--navy);display:grid;"
        "grid-template-rows:1fr 1fr 1fr;padding:6px 5px;gap:3px;flex-shrink:0}"
        ".mark i{display:block;height:1.5px;background:var(--ink)}"
        ".brand strong{font-family:var(--display);font-size:1.15rem;letter-spacing:-.03em}"
        ".brand span{color:var(--stone);font-size:.72rem;letter-spacing:.12em;text-transform:uppercase;margin-left:.15rem}"
        "#navc{position:absolute;left:-999px}"
        ".navbtn{display:none;min-height:44px;min-width:44px;align-items:center;justify-content:center;"
        "border:1px solid var(--rule);background:var(--paper);border-radius:var(--rsm);color:var(--ink);"
        "font:600 .85rem var(--body);cursor:pointer}"
        "nav{display:flex;flex-wrap:wrap;gap:.1rem .85rem;align-items:center;flex:1 1 16rem}"
        "nav a{color:var(--navy);text-decoration:none;font-size:.92rem;padding:.35rem 0;min-height:44px;"
        "display:inline-flex;align-items:center}nav a:hover{text-decoration:underline}"
        ".badge{display:inline-flex;align-items:center;justify-content:center;min-width:1.3rem;height:1.3rem;"
        "padding:0 .35rem;border-radius:999px;background:var(--navy);color:var(--paper);font-size:.7rem;"
        "margin-left:.25rem;font-variant-numeric:tabular-nums}"
        ".card{background:var(--panel);border:1px solid var(--rule);border-radius:var(--rlg);padding:1.15rem 1.25rem;margin:1rem 0}"
        ".card .card{border-radius:var(--rmd)}"
        ".lede{font-family:var(--display);font-size:1.15rem;line-height:1.4;color:var(--navy)}"
        ".muted{color:var(--stone);font-size:.9rem}.warn{color:var(--warn)}.ok{color:var(--ok)}"
        ".laws{display:grid;gap:.7rem;margin:1rem 0}"
        "@media(min-width:640px){.laws{grid-template-columns:1fr 1fr 1fr}}"
        ".law{background:var(--panel);border:1px solid var(--rule);border-radius:var(--rlg);padding:1rem 1.05rem}"
        ".law strong{display:block;font-family:var(--display);font-size:1.05rem;margin-bottom:.3rem}"
        "label{display:block;font-size:.82rem;color:var(--stone);margin:.4rem 0 .15rem;font-weight:500}"
        "input,textarea,select{width:100%;padding:.65rem .7rem;border-radius:var(--rsm);border:1px solid var(--rule);"
        "background:var(--paper);color:var(--ink);font:1rem var(--body);min-height:44px}"
        "textarea{min-height:6rem;resize:vertical}button,.btn{display:inline-flex;align-items:center;justify-content:center;"
        "gap:.35rem;background:var(--navy);color:var(--paper);border:0;padding:.65rem 1.05rem;border-radius:var(--rsm);"
        "cursor:pointer;font:600 .95rem var(--body);min-height:44px;text-decoration:none}"
        "button:hover,.btn:hover{filter:brightness(1.08)}button:active,.btn:active{transform:scale(.98)}"
        "button.ghost,.btn.ghost{background:transparent;color:var(--navy);border:1px solid var(--rule)}"
        "button.danger,.btn.danger{background:var(--danger);color:#f7f1e4}"
        ".row{display:flex;gap:.5rem;flex-wrap:wrap;align-items:center}"
        ".person,.sess,.msg,.post{border-top:1px solid var(--rule);padding:.7rem 0;display:flex;"
        "justify-content:space-between;gap:.8rem;flex-wrap:wrap;align-items:flex-start}"
        ".person:first-of-type,.sess:first-of-type,.msg:first-of-type,.post:first-of-type{border-top:0}"
        ".portrait{width:96px;height:96px;border-radius:50%;object-fit:cover;border:1px solid var(--rule);"
        "background:var(--parchment);flex-shrink:0;display:inline-block;vertical-align:middle}"
        ".portrait.sm{width:36px;height:36px}"
        ".portrait.ph{box-sizing:border-box}"
        ".who{display:flex;gap:.75rem;align-items:flex-start}"
        ".post p{margin:.25rem 0 0;white-space:pre-wrap}"
        "ul.plain{list-style:none;padding:0;margin:0}ul.plain li{border-top:1px solid var(--rule);padding:.65rem 0}"
        "footer.site{max-width:44rem;margin:0 auto;padding:1.2rem 1.1rem 2.5rem;color:var(--stone);font-size:.82rem;"
        "border-top:1px solid var(--rule)}"
        "footer.site a{color:var(--stone)}"
        ".empty{color:var(--stone);padding:.4rem 0}"
        "form.inline{display:inline}form.stack{margin:0}"
        ".clocks{display:grid;gap:.7rem}@media(min-width:640px){.clocks{grid-template-columns:1fr 1fr}}"
        ".jukebox iframe{width:100%;max-width:480px;aspect-ratio:16/9;border:0;border-radius:var(--rsm);display:block}"
        ".jukebox .row{margin-top:.5rem}"
        "@media(max-width:720px){.navbtn{display:inline-flex}nav{display:none;width:100%;flex-direction:column;"
        "align-items:stretch}nav a{min-height:44px}#navc:checked~nav{display:flex}}"
        "@media(prefers-reduced-motion:reduce){*{transition:none!important;animation:none!important}}";
}

std::string favicon_svg() {
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 32 32\">"
           "<rect width=\"32\" height=\"32\" fill=\"#efe8d8\"/>"
           "<rect x=\"6\" y=\"6\" width=\"20\" height=\"20\" fill=\"none\" stroke=\"#1e2d42\" stroke-width=\"1.5\"/>"
           "<path d=\"M10 13h12M10 16h12M10 19h12\" stroke=\"#1a1814\" stroke-width=\"1.3\"/></svg>";
}

std::string page_wrap(const std::string& title, const std::string& navhtml, const std::string& body) {
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>"+html_escape(title)+" — LiquidGov.US</title>"
           "<link rel=\"icon\" href=\"/favicon.svg\" type=\"image/svg+xml\">"
           "<style>"+css()+"</style></head><body>"
           "<header class=\"top\"><div class=\"topinner\">"
           "<a class=\"brand\" href=\"/\"><span class=\"mark\" aria-hidden=\"true\"><i></i><i></i><i></i></span>"
           "<div><strong>LiquidGov</strong><span>.US</span></div></a>"
           +navhtml+"</div></header>"
           "<main class=\"wrap\">"+body+"</main>"
           "<footer class=\"site\"><a href=\"/\">Genesis</a> · <a href=\"/laws\">Laws</a> · "
           "<a href=\"/control\">Who holds control</a><br>"
           "Not a government. A platform account is not a CitizenId. "
           "This copy is working software, not liquidgov.us in production.</footer></body></html>";
}

std::string nav(const std::string& user, int unread) {
    std::string burger = "<label class=\"navbtn\" for=\"navc\">Menu</label><input id=\"navc\" type=\"checkbox\">";
    if (user.empty()) {
        return burger+"<nav><a href=\"/\">Genesis</a><a href=\"/laws\">Laws</a>"
               "<a href=\"/control\">Who holds control</a>"
               "<a href=\"/signup\">Sign up</a><a href=\"/login\">Log in</a></nav>";
    }
    std::string n = burger+"<nav><a href=\"/feed\">Feed</a>"
                    "<a href=\"/friends\">People</a><a href=\"/mail\">Mail</a>"
                    "<a href=\"/communities\">Groups</a>"
                    "<a href=\"/world\">World</a>"
                    "<a href=\"/notices\">Notices";
    if (unread > 0) n += "<span class=\"badge\">"+std::to_string(unread)+"</span>";
    n += "</a><a href=\"/me\">"+html_escape(user)+"</a>"
         "<a href=\"/settings\">Settings</a><a href=\"/logout\">Log out</a></nav>";
    return n;
}

std::string url_decode(const std::string& s) {
    std::string o;
    for (size_t i=0;i<s.size();++i) {
        if (s[i]=='+') o+=' ';
        else if (s[i]=='%' && i+2<s.size()) { int v=0; std::stringstream ss; ss<<std::hex<<s.substr(i+1,2); ss>>v; o+=char(v); i+=2; }
        else o+=s[i];
    }
    return o;
}
std::map<std::string,std::string> parse_form(const std::string& body) {
    std::map<std::string,std::string> m; std::stringstream ss(body); std::string part;
    while (std::getline(ss,part,'&')) { auto eq=part.find('='); if(eq==std::string::npos) continue;
        m[url_decode(part.substr(0,eq))]=url_decode(part.substr(eq+1)); }
    return m;
}
std::map<std::string,std::string> parse_qs(const std::string& path) {
    std::map<std::string,std::string> m;
    auto q = path.find('?');
    if (q==std::string::npos) return m;
    return parse_form(path.substr(q+1));
}
std::string cookie_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return {};
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_session="); if(k==std::string::npos) return {};
    k+=11; auto sc=line.find(';',k); return line.substr(k, sc==std::string::npos?std::string::npos:sc-k);
}
std::string header_value(const std::string& headers, const std::string& name) {
    std::string key = name + ":";
    auto pos = headers.find(key);
    if (pos == std::string::npos) {
        key = name;
        for (char& c : key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        pos = headers.find(key + ":");
        if (pos == std::string::npos) return {};
        key += ":";
    }
    pos += key.size();
    while (pos < headers.size() && headers[pos]==' ') ++pos;
    auto end = headers.find("\r\n", pos);
    if (end == std::string::npos) return headers.substr(pos);
    return headers.substr(pos, end-pos);
}
std::string device_label_from(const std::string& headers, const std::string& ip) {
    auto ua = header_value(headers, "User-Agent");
    std::string label;
    if (ua.find("Firefox") != std::string::npos) label = "Firefox";
    else if (ua.find("Edg/") != std::string::npos) label = "Edge";
    else if (ua.find("Chrome") != std::string::npos) label = "Chrome";
    else if (ua.find("Safari") != std::string::npos) label = "Safari";
    else if (ua.find("curl") != std::string::npos) label = "curl";
    else if (!ua.empty()) label = "web";
    else label = "web";
    if (!ip.empty()) label += " · " + ip;
    if (label.size() > 80) label.resize(80);
    return label;
}
std::string fmt_time(std::int64_t t) {
    if (t <= 0) return "";
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &tm);
    return buf;
}

std::optional<PlatformAccount> account_from_req(const std::string& headers) {
    auto sid = cookie_of(headers);
    auto sess=g_store.get_session(sid);
    if(!sess) return std::nullopt;
    g_store.touch_session(sid);
    return g_store.find_by_id(sess->account);
}
std::string http_ok(const std::string& body, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_raw(const char* status, const char* ctype, const std::string& body, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 "<<status<<"\r\nContent-Type: "<<ctype<<"\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_code(const char* status, const std::string& body) {
    return http_raw(status, "text/html; charset=utf-8", body);
}
std::string http_redir(const std::string& loc, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 303 See Other\r\nLocation: "<<loc<<"\r\nContent-Length: 0\r\nConnection: close\r\n"<<extra<<"\r\n";
    return o.str();
}
std::string set_cookie(const std::string& sid) {
    return "Set-Cookie: lg_session="+sid+"; HttpOnly; Path=/; SameSite=Lax; Max-Age=2592000\r\n";
}
std::string clear_cookie() {
    return "Set-Cookie: lg_session=; HttpOnly; Path=/; SameSite=Lax; Max-Age=0\r\n";
}

std::string trim_ws(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\r'||s[a]=='\n')) ++a;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'||s[b-1]=='\n')) --b;
    return s.substr(a, b-a);
}

std::string header_line_value(const std::string& headers, const char* name) {
    const std::size_t nlen = std::strlen(name);
    std::size_t i = 0;
    while (i < headers.size()) {
        std::size_t e = headers.find('\n', i);
        if (e == std::string::npos) e = headers.size();
        if (e - i >= nlen) {
            bool match = true;
            for (std::size_t k = 0; k < nlen; ++k) {
                char a = headers[i+k], b = name[k];
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b) { match = false; break; }
            }
            if (match && i + nlen < headers.size() && headers[i+nlen] == ':') {
                std::size_t v = i + nlen + 1;
                while (v < e && (headers[v]==' '||headers[v]=='\t')) ++v;
                std::size_t end = e;
                if (end > v && headers[end-1]=='\r') --end;
                return headers.substr(v, end-v);
            }
        }
        i = e + 1;
    }
    return {};
}

bool chunked_complete(const std::string& body) {
    std::size_t i = 0;
    while (i < body.size()) {
        auto nl = body.find("\r\n", i);
        if (nl == std::string::npos) return false;
        std::string sz = body.substr(i, nl-i);
        auto semi = sz.find(';');
        if (semi != std::string::npos) sz = sz.substr(0, semi);
        std::size_t n = 0;
        try { n = std::stoul(sz, nullptr, 16); } catch (...) { return false; }
        i = nl + 2;
        if (n == 0) return true;
        if (i + n + 2 > body.size()) return false;
        i += n + 2;
    }
    return false;
}

std::string decode_chunked(const std::string& body) {
    std::string out;
    std::size_t i = 0;
    while (i < body.size()) {
        auto nl = body.find("\r\n", i);
        if (nl == std::string::npos) break;
        std::string sz = body.substr(i, nl-i);
        auto semi = sz.find(';');
        if (semi != std::string::npos) sz = sz.substr(0, semi);
        std::size_t n = 0;
        try { n = std::stoul(sz, nullptr, 16); } catch (...) { break; }
        i = nl + 2;
        if (n == 0) break;
        if (i + n > body.size()) break;
        out.append(body, i, n);
        i += n;
        if (i + 2 <= body.size()) i += 2;
    }
    return out;
}

bool read_http_request(int fd, std::string& req) {
    req.clear();
    char buf[8192];
    auto more = [&]() -> ssize_t {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) req.append(buf, static_cast<std::size_t>(n));
        return n;
    };
    while (req.find("\r\n\r\n") == std::string::npos && req.find("\n\n") == std::string::npos) {
        if (more() <= 0) return !req.empty();
        if (req.size() > 1024 * 1024) return false;
    }
    if (req.find("\r\n\r\n") == std::string::npos) {
        std::string n;
        n.reserve(req.size() + 16);
        for (std::size_t i = 0; i < req.size(); ++i) {
            if (req[i] == '\n' && (i == 0 || req[i-1] != '\r')) n += "\r\n";
            else n += req[i];
        }
        req.swap(n);
    }
    auto sep = req.find("\r\n\r\n");
    if (sep == std::string::npos) return false;
    std::string headers = req.substr(0, sep);
    std::string body = req.substr(sep + 4);

    std::string expect = header_line_value(headers, "Expect");
    std::string te = header_line_value(headers, "Transfer-Encoding");
    std::string cl = header_line_value(headers, "Content-Length");
    for (char& c : te) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    bool chunked = te.find("chunked") != std::string::npos;

    if (!expect.empty() && body.empty()) {
        std::string low = expect;
        for (char& c : low) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (low.find("100-continue") != std::string::npos) {
            const char msg[] = "HTTP/1.1 100 Continue\r\n\r\n";
            send(fd, msg, sizeof(msg) - 1, 0);
        }
    }

    if (chunked) {
        while (!chunked_complete(body)) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
            if (body.size() > kMaxBody) return false;
        }
        body = decode_chunked(body);
        if (body.size() > kMaxBody) return false;
    } else if (!cl.empty()) {
        std::size_t need = 0;
        try { need = static_cast<std::size_t>(std::stoull(cl)); } catch (...) { need = 0; }
        if (need > kMaxBody) return false;
        while (body.size() < need) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
            if (body.size() > kMaxBody) return false;
        }
        if (body.size() > need) body.resize(need);
    }
    req = headers + "\r\n\r\n" + body;
    return true;
}

std::string wrap(const std::string& title, const std::string& user, int unread, const std::string& body) {
    return page_wrap(title, nav(user, unread), body);
}

std::string trim_copy(std::string s) {
    while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='"'||s.back()=='\'')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='"'||s[i]=='\'')) ++i;
    return s.substr(i);
}

std::optional<std::vector<std::uint8_t>> multipart_file(const std::string& content_type,
                                                        const std::string& body,
                                                        const char* field) {
    auto bpos = content_type.find("boundary=");
    if (bpos == std::string::npos) return std::nullopt;
    std::string boundary = trim_copy(content_type.substr(bpos + 9));
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary.resize(semi);
    boundary = trim_copy(boundary);
    if (boundary.empty()) return std::nullopt;
    const std::string delim = "--" + boundary;
    const std::string needle = std::string("name=\"") + field + "\"";
    std::size_t i = 0;
    while (i < body.size()) {
        auto start = body.find(delim, i);
        if (start == std::string::npos) break;
        start += delim.size();
        if (start + 1 < body.size() && body[start] == '-' && body[start + 1] == '-') break;
        if (start + 1 < body.size() && body[start] == '\r' && body[start + 1] == '\n') start += 2;
        else if (start < body.size() && body[start] == '\n') start += 1;
        auto next = body.find(delim, start);
        if (next == std::string::npos) break;
        std::string part = body.substr(start, next - start);
        while (part.size() >= 2 && part[part.size() - 2] == '\r' && part.back() == '\n')
            part.resize(part.size() - 2);
        auto hdr_end = part.find("\r\n\r\n");
        std::size_t skip = 4;
        if (hdr_end == std::string::npos) { hdr_end = part.find("\n\n"); skip = 2; }
        if (hdr_end == std::string::npos) { i = next; continue; }
        std::string hdrs = part.substr(0, hdr_end);
        if (hdrs.find(needle) == std::string::npos) { i = next; continue; }
        std::string file = part.substr(hdr_end + skip);
        return std::vector<std::uint8_t>(file.begin(), file.end());
    }
    return std::nullopt;
}

// A multipart text field (not a file) is the same wire shape as a file
// part — a header block, blank line, then the value — so multipart_file
// already extracts it correctly; this just gives that a string-typed name
// at call sites that want a form field, not bytes.
std::string multipart_field(const std::string& content_type, const std::string& body,
                             const char* field) {
    auto v = multipart_file(content_type, body, field);
    if (!v) return {};
    return std::string(v->begin(), v->end());
}

// Group rosters and messages key on AccountId, which can outlive the
// account itself (delete_account() does not prune conversations_/
// messages_ — see file_store.cpp). Same "relabel, don't erase" choice
// already used for mail_/posts_.
std::string display_name_for(AccountId id) {
    auto acc = g_store.find_by_id(id);
    if (!acc) return "[deleted account]";
    return acc->username;
}

// No conversation title in the messenger.hpp/conversation.hpp spec — the
// list label is everyone else in the room, so the thread is identifiable
// without inventing a naming feature that isn't there.
std::string group_label(const Conversation& conv, AccountId viewer) {
    std::string label;
    for (const auto& m : conv.members) {
        if (m.account == viewer) continue;
        if (!label.empty()) label += ", ";
        label += display_name_for(m.account);
    }
    return label.empty() ? "(just you)" : label;
}

// Suggested download name only (the browser lets the user rename); no
// original filename is stored, so this is derived from the mime that
// ingest_file actually assigned after content sniffing.
std::string attachment_filename_for_mime(const std::string& mime) {
    if (mime == "image/png") return "attachment.png";
    if (mime == "application/pdf") return "attachment.pdf";
    if (mime.rfind("text/plain", 0) == 0) return "attachment.txt";
    return "attachment";
}

bool has_portrait(AccountId id) {
    auto av = g_store.get(id);
    if (!av) return false;
    return g_blobs.meta(av->blob_id).has_value();
}

std::string portrait_tag(const std::string& username, const char* cls) {
    auto acc = g_store.find_by_username(username);
    if (!acc || !has_portrait(acc->id))
        return std::string("<span class=\"portrait ")+cls+" ph\" aria-hidden=\"true\"></span>";
    return std::string("<img class=\"portrait ")+cls+"\" src=\"/portrait/"+html_escape(username)+
           "\" alt=\"\" width=\"96\" height=\"96\">";
}

// World client page: a <canvas>, WASD/arrow movement, one WebSocket to
// /world/ws. Sends {"x":n,"y":n} on movement; server broadcasts the full
// player snapshot back on an interval regardless of whether this client
// moved, so remote players animate smoothly too. Matches the documented
// MVP: "One shared room, two people see each other move."
std::string world_page(const std::string& username) {
    std::string html =
        "<div class=\"card\"><h1>World</h1>"
        "<p class=\"muted\">One shared room. Arrow keys or WASD to move. Closing this page removes you.</p>"
        "<canvas id=\"wcanvas\" width=\"800\" height=\"500\" "
        "style=\"border:1px solid var(--rule);border-radius:var(--rsm);background:#eee7d6;"
        "max-width:100%;touch-action:none\"></canvas>"
        "<p class=\"muted\" id=\"wstatus\">Connecting\\u2026</p></div>"
        "<script>(function(){"
        "var me="+js_string_literal(username)+";"
        "var cv=document.getElementById('wcanvas'),ctx=cv.getContext('2d');"
        "var st=document.getElementById('wstatus');"
        "var players={};"
        "var proto=location.protocol==='https:'?'wss:':'ws:';"
        "var ws=new WebSocket(proto+'//'+location.host+'/world/ws');"
        "var keys={};"
        "ws.onopen=function(){st.textContent='Connected.';};"
        "ws.onclose=function(){st.textContent='Disconnected. Reload to rejoin.';};"
        "ws.onerror=function(){st.textContent='Connection error.';};"
        "ws.onmessage=function(ev){"
        "try{var data=JSON.parse(ev.data);players=data.players||{};}catch(e){}"
        "draw();"
        "};"
        "function draw(){"
        "ctx.clearRect(0,0,cv.width,cv.height);"
        "Object.keys(players).forEach(function(id){"
        "var p=players[id];"
        "ctx.beginPath();ctx.arc(p.x,p.y,14,0,Math.PI*2);"
        "ctx.fillStyle=p.color;ctx.fill();"
        "if(p.username===me){ctx.lineWidth=2;ctx.strokeStyle='#1a1814';ctx.stroke();}"
        "ctx.fillStyle='#1a1814';ctx.font='12px sans-serif';ctx.textAlign='center';"
        "ctx.fillText(p.username,p.x,p.y-20);"
        "});"
        "}"
        "document.addEventListener('keydown',function(e){keys[e.key.toLowerCase()]=true;});"
        "document.addEventListener('keyup',function(e){keys[e.key.toLowerCase()]=false;});"
        "var x=cv.width/2,y=cv.height/2;"
        "setInterval(function(){"
        "if(ws.readyState!==1)return;"
        "var dx=0,dy=0,speed=5;"
        "if(keys['arrowleft']||keys['a'])dx-=speed;"
        "if(keys['arrowright']||keys['d'])dx+=speed;"
        "if(keys['arrowup']||keys['w'])dy-=speed;"
        "if(keys['arrowdown']||keys['s'])dy+=speed;"
        "if(dx||dy){"
        "x=Math.max(14,Math.min(cv.width-14,x+dx));"
        "y=Math.max(14,Math.min(cv.height-14,y+dy));"
        "ws.send(JSON.stringify({x:x,y:y}));"
        "}"
        "},50);"
        "})();</script>";
    return html;
}

// One thread per connected /world/ws client. Runs the RFC 6455 handshake
// response, then loops: try to read this client's new position (short
// timeout so the loop can broadcast even when nobody moved), update the
// shared registry, and push everyone's current positions back to this
// client. Removes the client from the registry on disconnect.
void run_world_session(int fd, std::string username) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 100000; // 100ms
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        WorldPlayer p;
        p.username = username;
        p.color = world_color_for(username);
        g_world_players[fd] = p;
    }

    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                // Minimal ad-hoc parse of {"x":<num>,"y":<num>} — the only
                // shape this endpoint ever receives, so a general JSON
                // parser would be pure overhead here.
                const std::string& s = result.frame.payload;
                auto extract = [&](const char* key) -> std::optional<double> {
                    auto p = s.find(key);
                    if (p == std::string::npos) return std::nullopt;
                    p = s.find(':', p);
                    if (p == std::string::npos) return std::nullopt;
                    try { return std::stod(s.substr(p + 1)); } catch (...) { return std::nullopt; }
                };
                auto nx = extract("\"x\"");
                auto ny = extract("\"y\"");
                if (nx && ny) {
                    std::lock_guard<std::mutex> lock(g_world_mu);
                    auto it = g_world_players.find(fd);
                    if (it != g_world_players.end()) { it->second.x = *nx; it->second.y = *ny; }
                }
            }
        }
        // status == Timeout falls through here too — always re-broadcast.

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_world_mu);
            if (g_world_players.find(fd) == g_world_players.end()) break;
            std::ostringstream os;
            os << "{\"players\":{";
            bool first = true;
            for (auto& [id, p] : g_world_players) {
                if (!first) os << ",";
                first = false;
                os << "\"" << id << "\":{\"x\":" << p.x << ",\"y\":" << p.y
                   << ",\"username\":\"" << p.username << "\",\"color\":\"" << p.color << "\"}";
            }
            os << "}}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        g_world_players.erase(fd);
    }
    close(fd);
}

// In-browser drawing tool for the self-portrait. Draws to a <canvas>, exports
// via canvas.toBlob('image/png') and POSTs it to the *same* /me/avatar route
// as a file upload — the server can't tell a drawn PNG from an uploaded one,
// so save_avatar's normalize/scan/blob pipeline is untouched by this feature.
std::string portrait_draw_widget() {
    return
        "<div class=\"card\"><h2>Or draw one</h2>"
        "<canvas id=\"pcanvas\" width=\"256\" height=\"256\" "
        "style=\"border:1px solid var(--rule);border-radius:var(--rsm);background:#fff;"
        "touch-action:none;cursor:crosshair;max-width:100%\"></canvas>"
        "<div class=\"row\" style=\"margin-top:.5rem\">"
        "<input type=\"color\" id=\"pcolor\" value=\"#1a1814\" aria-label=\"Color\">"
        "<input type=\"range\" id=\"psize\" min=\"1\" max=\"24\" value=\"4\" aria-label=\"Brush size\">"
        "<button type=\"button\" class=\"ghost\" id=\"pclear\">Clear</button>"
        "<button type=\"button\" id=\"psave\">Use this drawing</button>"
        "</div><p class=\"muted\" id=\"pstatus\"></p></div>"
        "<script>(function(){"
        "var cv=document.getElementById('pcanvas'),ctx=cv.getContext('2d');"
        "ctx.fillStyle='#fff';ctx.fillRect(0,0,cv.width,cv.height);"
        "var drawing=false,last=null;"
        "function pos(e){var r=cv.getBoundingClientRect();var p=e.touches?e.touches[0]:e;"
        "return{x:(p.clientX-r.left)*cv.width/r.width,y:(p.clientY-r.top)*cv.height/r.height};}"
        "function start(e){drawing=true;last=pos(e);e.preventDefault();}"
        "function move(e){if(!drawing)return;var p=pos(e);"
        "ctx.strokeStyle=document.getElementById('pcolor').value;"
        "ctx.lineWidth=document.getElementById('psize').value;"
        "ctx.lineCap='round';ctx.lineJoin='round';"
        "ctx.beginPath();ctx.moveTo(last.x,last.y);ctx.lineTo(p.x,p.y);ctx.stroke();"
        "last=p;e.preventDefault();}"
        "function end(){drawing=false;}"
        "cv.addEventListener('mousedown',start);cv.addEventListener('mousemove',move);"
        "window.addEventListener('mouseup',end);"
        "cv.addEventListener('touchstart',start,{passive:false});"
        "cv.addEventListener('touchmove',move,{passive:false});"
        "cv.addEventListener('touchend',end);"
        "document.getElementById('pclear').addEventListener('click',function(){"
        "ctx.fillStyle='#fff';ctx.fillRect(0,0,cv.width,cv.height);});"
        "document.getElementById('psave').addEventListener('click',function(){"
        "var st=document.getElementById('pstatus');st.textContent='Saving\\u2026';"
        "cv.toBlob(function(blob){"
        "var fd=new FormData();fd.append('photo',blob,'portrait.png');"
        "fetch('/me/avatar',{method:'POST',body:fd}).then(function(r){"
        "window.location=r.url||'/me';"
        "}).catch(function(){st.textContent='Could not save. Try again.';});"
        "},'image/png');"
        "});"
        "})();</script>";
}

// Page-local jukebox widget (navigation option A: MPA, own instance per
// page, stops on navigate away — no site-wide SPA, no WebSocket sync).
// Owner-curated: add/remove only render when owner_view is true; visitors
// get Prev/Next over the same queue via ordinary links, nothing more.
std::string jukebox_widget(const std::string& owner, bool owner_view, const std::map<std::string,std::string>& qs) {
    auto tracks = g_store.jukebox_of(owner);
    std::string html = "<div class=\"card jukebox\"><h2>Jukebox</h2>";
    if (qs.count("jerr")) {
        const std::string& e = qs.at("jerr");
        if (e=="url") html += "<p class=\"warn\">That link wasn't recognized as a YouTube video.</p>";
        else if (e=="full") html += "<p class=\"warn\">Queue already holds "+std::to_string(kJukeboxQueueCap)+" tracks. Remove one first.</p>";
    }
    if (tracks.empty()) {
        html += "<p class=\"empty\">No tracks queued.</p>";
    } else {
        int idx = 0;
        auto it = qs.find("t");
        if (it != qs.end()) { try { idx = std::stoi(it->second); } catch (...) { idx = 0; } }
        if (idx < 0) idx = 0;
        if (idx >= static_cast<int>(tracks.size())) idx = static_cast<int>(tracks.size()) - 1;
        const auto& t = tracks[static_cast<std::size_t>(idx)];
        html += "<iframe src=\"https://www.youtube-nocookie.com/embed/"+html_escape(t.video_id)+"?rel=0\" "
                "title=\"Queued track\" allow=\"encrypted-media; picture-in-picture\" "
                "referrerpolicy=\"strict-origin-when-cross-origin\" allowfullscreen></iframe>"
                "<p class=\"muted\">Track "+std::to_string(idx+1)+" of "+std::to_string(tracks.size())+"</p>"
                "<div class=\"row\">";
        if (idx > 0) html += "<a class=\"btn ghost\" href=\"?t="+std::to_string(idx-1)+"\">Prev</a>";
        if (idx+1 < static_cast<int>(tracks.size())) html += "<a class=\"btn ghost\" href=\"?t="+std::to_string(idx+1)+"\">Next</a>";
        if (owner_view) {
            html += "<form class=\"inline\" method=\"POST\" action=\"/me/jukebox/remove\">"
                    "<input type=\"hidden\" name=\"video_id\" value=\""+html_escape(t.video_id)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Remove this track</button></form>";
        }
        html += "</div>";
    }
    if (owner_view) {
        const char* yt_key = youtube_api_key_from_env();
        if (yt_key[0]) {
            html += "<div><label>Search YouTube</label>"
                    "<div class=\"row\"><input id=\"jsq\" placeholder=\"Search for a video\">"
                    "<button type=\"button\" id=\"jsbtn\">Search</button></div>"
                    "<div id=\"jsresults\" class=\"row\" style=\"margin-top:.5rem;flex-direction:column;align-items:stretch;gap:.4rem\"></div></div>"
                    "<script>(function(){"
                    "var key="+js_string_literal(yt_key)+";"
                    "var q=document.getElementById('jsq'),btn=document.getElementById('jsbtn'),out=document.getElementById('jsresults');"
                    "function esc(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML;}"
                    "function run(){"
                    "var term=q.value.trim();if(!term)return;"
                    "out.innerHTML='<p class=\"muted\">Searching\\u2026</p>';"
                    "fetch('https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&maxResults=8&q='"
                    "+encodeURIComponent(term)+'&key='+key)"
                    ".then(function(r){return r.json();}).then(function(data){"
                    "if(!data.items||!data.items.length){out.innerHTML='<p class=\"muted\">No results.</p>';return;}"
                    "out.innerHTML='';"
                    "data.items.forEach(function(it){"
                    "var id=it.id.videoId,title=it.snippet.title,thumb=it.snippet.thumbnails.default.url;"
                    "var row=document.createElement('div');row.className='row';row.style.alignItems='center';"
                    "row.innerHTML='<img src=\"'+thumb+'\" width=\"60\" height=\"45\" alt=\"\" style=\"border-radius:4px\">"
                    "<span style=\"flex:1\">'+esc(title)+'</span>"
                    "<button type=\"button\" class=\"ghost\" data-id=\"'+id+'\">Queue</button>';"
                    "row.querySelector('button').addEventListener('click',function(){"
                    "var fd=new FormData();fd.append('url','https://www.youtube.com/watch?v='+id);"
                    "fetch('/me/jukebox/add',{method:'POST',body:fd}).then(function(r){window.location=r.url||location.href;});"
                    "});"
                    "out.appendChild(row);"
                    "});"
                    "}).catch(function(){out.innerHTML='<p class=\"warn\">Search failed.</p>';});"
                    "}"
                    "btn.addEventListener('click',run);"
                    "q.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();run();}});"
                    "})();</script>";
        } else {
            html += "<p class=\"muted\">Search is off (site operator hasn't set YOUTUBE_API_KEY). Paste a link instead:</p>";
        }
        html += "<form method=\"POST\" action=\"/me/jukebox/add\"><label>Add a YouTube link</label>"
                "<div class=\"row\"><input name=\"url\" placeholder=\"https://www.youtube.com/watch?v=...\" required>"
                "<button>Queue</button></div></form>"
                "<p class=\"muted\">Owner-curated. Visitors listen; navigating away stops playback.</p>";
    }
    html += "</div>";
    return html;
}

std::string genesis_body() {
    return "<h1>This is genesis, not a government.</h1>"
           "<p class=\"lede\">LiquidGov.US is a voluntary association and working software for "
           "liquid, natural-law democracy. Anyone here is a guest or a member of that association.</p>"
           "<div class=\"card\"><p>Nobody on this site is your city, state, or federal government. "
           "No office seated from these pages has legal authority over anyone who did not choose to associate.</p>"
           "<p>Website control is temporary. When a working election exists among enrolled citizens, "
           "that control is meant to end. The internal shape of that election is itself a citizen vote. "
           "Real governmental authority is a later, explicit landslide — Phase 2 — not this page and not "
           "the first people who show up.</p>"
           "<p class=\"warn\">Until then, genesis operators run the machines and say so in public. "
           "That is a disclosure, not a claim of sovereignty.</p></div>"
           "<div class=\"laws\">"
           "<div class=\"law\"><strong>Do not steal</strong><p class=\"muted\">Property stays with the person who holds it. Taking it by motion is the forbidden act.</p></div>"
           "<div class=\"law\"><strong>Do not damage</strong><p class=\"muted\">Harm to person or goods is outside the association’s license.</p></div>"
           "<div class=\"law\"><strong>Do not deprive of liberty</strong><p class=\"muted\">No one here may cage another’s lawful movement or speech by force of this software.</p></div>"
           "</div>"
           "<div class=\"clocks\">"
           "<div class=\"card\"><h2>Two clocks</h2><p><strong>Site control</strong> ends when a working election exists among enrolled citizens.</p>"
           "<p><strong>Governmental authority</strong> stays a later landslide — more than half of the eligible citizen population. However long that takes.</p></div>"
           "<div class=\"card\"><h2>This website is the door</h2><p>Feed, friends, mail, and communities live here. "
           "Voting weight, offices, land, and judgments live in a separate governance kernel. "
           "A platform account is not a CitizenId. Combined verification for civic acts is not live.</p></div>"
           "</div>";
}

std::string laws_body() {
    return "<h1>Three laws</h1>"
           "<p class=\"lede\">The association is built around three prohibitions. Everything else is mechanism, and most of that is still a citizen vote.</p>"
           "<div class=\"card\"><p>Do not steal. Do not damage. Do not deprive of liberty. "
           "Those are the laws. They are not slogans on a landing page — they are the only hard limits this project will encode as law.</p>"
           "<p>Direct voting is the posture. Delegation exists as a capability, not a default. "
           "There is no sortition in the governance kernel. Seats, case thresholds, land-pool sizes, and the shape of the executive are not numbers we invent here.</p></div>"
           "<div class=\"card\"><h2>What this site is not</h2>"
           "<p>It is not a government. It is not a seated legislature. It does not issue CitizenIds. "
           "Unverified visitors can look around. One combined verification is meant to unlock civic acts when that path exists. "
           "We will not treat a checkbox as age. Minors, when age can be assured, are friends-and-family only.</p></div>"
           "<div class=\"card\"><h2>Kernel, not this process</h2>"
           "<p>Identity, citizenship, land, business, judgment, and genesis authority live in <code>liquidgov-core</code>. "
           "This website does not write those records. Linking a platform account to a kernel identity is a later, explicit step — and it is not citizenship.</p>"
           "<p class=\"muted\"><a href=\"/control\">Who holds control of this site</a> · <a href=\"/\">Genesis disclosure</a></p></div>";
}

std::string control_body() {
    return "<h1>Who holds control</h1>"
           "<p class=\"lede\">Genesis operators run the machines and say so in public.</p>"
           "<div class=\"card\"><p><strong>Operator of this copy:</strong> Brandon Bottin.</p>"
           "<p><strong>As of:</strong> September 2026.</p>"
           "<p>That name is a disclosure of who can change this software and this store today. "
           "It is not an office. It is not a claim that the association already governs anyone.</p>"
           "<p class=\"warn\">Control of the website is meant to end when a working election exists among enrolled citizens. "
           "Governmental authority is a later landslide, not this page.</p>"
           "<p class=\"muted\">This preview is a working copy of the association’s door. It is not liquidgov.us in production.</p></div>";
}

std::string signup_page(const std::string& navuser, int unread, const std::string& err,
                        const std::string& username, const std::string& phone) {
    std::string e;
    if (!err.empty()) e = "<p class=\"warn\">"+html_escape(err)+"</p>";
    return wrap("Sign up", navuser, unread,
        "<div class=\"card\"><h1>Create an account</h1>"
        "<p class=\"muted\">Username is one word: letters, numbers, dots, underscores, hyphens. "
        "No spaces, no @. Phone is a signup binding, not the daily login. "
        "This is a platform account, not a CitizenId.</p>"+e+
        "<form method=\"POST\" action=\"/signup\">"
        "<label>Username</label><input name=\"username\" required maxlength=\"32\" "
        "pattern=\"[A-Za-z0-9._-]+\" title=\"Letters, numbers, dots, underscores, hyphens. No spaces.\" "
        "autocomplete=\"username\" placeholder=\"alice\" value=\""+html_escape(username)+"\">"
        "<label>Password</label><input type=\"password\" name=\"password\" required autocomplete=\"new-password\">"
        "<label>Phone</label><input name=\"phone\" type=\"tel\" required autocomplete=\"tel\" value=\""+html_escape(phone)+"\">"
        "<button>Continue</button></form></div>");
}

std::string post_block(const Post& p) {
    std::string where;
    if (!p.community.empty())
        where = " in <a href=\"/c/"+html_escape(p.community)+"\">"+html_escape(p.community)+"</a>";
    std::string when = fmt_time(p.ts);
    return "<article class=\"post\"><div class=\"who\">"+portrait_tag(p.author,"sm")+"<div><a href=\"/u/"+html_escape(p.author)+"\"><strong>"+
           html_escape(p.author)+"</strong></a>"+where+
           (when.empty()?"":" <span class=\"muted\">"+html_escape(when)+"</span>")+
           "<p>"+html_escape(p.text)+"</p></div></div></article>";
}

std::string handle(const std::string& req, const std::string& peer_ip) {
    auto line_end=req.find("\r\n");
    if(line_end==std::string::npos) line_end=req.find('\n');
    if(line_end==std::string::npos) return http_ok(wrap("Error","",0,"<p>bad request</p>"));
    std::string method, rawpath; { std::istringstream is(req.substr(0,line_end)); is>>method>>rawpath; }
    std::string path = rawpath;
    auto qpos=path.find('?'); if(qpos!=std::string::npos) path=path.substr(0,qpos);
    auto qs = parse_qs(rawpath);
    std::string body;
    auto sep=req.find("\r\n\r\n");
    if(sep!=std::string::npos) body=req.substr(sep+4);
    else { sep=req.find("\n\n"); if(sep!=std::string::npos) body=req.substr(sep+2); }
    std::lock_guard<std::mutex> lock(g_mu);
    auto acc = account_from_req(req);
    std::string user = acc ? acc->username : "";
    const std::string sid = cookie_of(req);
    const int unread = acc ? g_store.unread_count(user) : 0;
    auto pg = [&](const std::string& title, const std::string& b) {
        return http_ok(wrap(title, user, unread, b));
    };

    if(method=="GET"&&path=="/favicon.svg")
        return http_raw("200 OK","image/svg+xml", favicon_svg());

    if(method=="GET"&&path=="/") {
        std::string extra;
        if(acc)
            extra = "<div class=\"card\"><p>Signed in as <strong>"+html_escape(user)+
                "</strong>. <a href=\"/feed\">Open the feed</a>.</p>"
                "<p class=\"muted\">Session from platform login(). Not seating. Not a CitizenId.</p></div>";
        else
            extra = "<div class=\"card\"><p class=\"row\"><a class=\"btn\" href=\"/signup\">Create an account</a> "
                    "<a class=\"btn ghost\" href=\"/login\">Log in</a></p></div>";
        return pg("Genesis", genesis_body()+extra);
    }
    if(method=="GET"&&path=="/laws") return pg("Laws", laws_body());
    if(method=="GET"&&path=="/control") return pg("Who holds control", control_body());
    if(method=="GET"&&path=="/signup") return http_ok(signup_page(user, unread, "", "", ""));
    if(method=="GET"&&path=="/login")
        return pg("Log in", "<div class=\"card\"><h1>Log in</h1>"
            "<p class=\"muted\">Username and password. Phone is not the daily credential.</p>"
            "<form method=\"POST\" action=\"/login\"><label>Username</label><input name=\"username\" required autocomplete=\"username\">"
            "<label>Password</label><input type=\"password\" name=\"password\" required autocomplete=\"current-password\">"
            "<button>Continue</button></form></div>");
    if(method=="GET"&&path=="/logout") {
        logout(g_store, sid);
        return http_redir("/", clear_cookie());
    }

    if(method=="GET"&&path=="/world") {
        if(!acc) return http_redir("/login");
        return pg("World", world_page(user));
    }
    if(method=="GET"&&path=="/feed") {
        if(!acc) return http_redir("/login");
        std::string html = "<h1>Feed</h1>"
            "<div class=\"card\"><form method=\"POST\" action=\"/feed\">"
            "<label>Write to people who know you</label>"
            "<textarea name=\"text\" maxlength=\"2000\" required placeholder=\"Plain text. Friends see this.\"></textarea>"
            "<button>Post</button></form>"
            "<p class=\"muted\">Personal posts reach you and your friends. Communities have their own pages. "
            "This is not a civic act.</p></div>";
        auto posts = g_store.feed_for(user, 80);
        if (posts.empty()) html += "<p class=\"empty\">Nothing yet. Write something, or add a friend.</p>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p)+"</div>";
        return pg("Feed", html);
    }

    if(method=="GET"&&path.rfind("/portrait/",0)==0) {
        const std::string who = path.substr(10);
        if (!valid_username(who)) return http_code("404 Not Found", "not found");
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto av = g_store.get(found->id);
        if (!av) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto bytes = g_blobs.get(av->blob_id);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"portrait.png\"\r\n");
    }

    if(method=="GET"&&path=="/me") {
        if(!acc) return http_redir("/login");
        auto b = g_store.blurb(acc->id);
        std::string err;
        if (qs["err"]=="image") err = "<p class=\"warn\">That file was not a usable image (PNG/JPEG/GIF, 2&nbsp;MiB, 1024px). SVG is not accepted.</p>";
        else if (qs["err"]=="large") err = "<p class=\"warn\">That file is too large. Cap is 2&nbsp;MiB and 1024 pixels on the long edge.</p>";
        else if (qs["err"]=="scan") err = "<p class=\"warn\">That file could not be stored.</p>";
        std::string html = "<div class=\"card\"><div class=\"who\">"+portrait_tag(user,"")+"<div><h1>"+html_escape(user)+"</h1>"
            "<p class=\"muted\">Platform profile · <a href=\"/u/"+html_escape(user)+"\">public page</a> · member since "+
            html_escape(fmt_time(acc->created_at))+"</p></div></div>"+err+
            "<form method=\"POST\" action=\"/me/avatar\" enctype=\"multipart/form-data\">"
            "<label>Self-portrait</label>"
            "<input type=\"file\" name=\"photo\" accept=\"image/png,image/jpeg,image/gif\" required>"
            "<p class=\"muted\">Decoded and re-encoded as PNG. No SVG. No author CSS. Not a CitizenId.</p>"
            "<div class=\"row\"><button>Upload</button></div></form>"
            + portrait_draw_widget();
        if (has_portrait(acc->id))
            html += "<form method=\"POST\" action=\"/me/avatar/clear\"><button class=\"ghost\" type=\"submit\">Remove portrait</button></form>";
        html += "<p>"+html_escape(b.empty()?"No blurb yet.":b)+"</p>"
            "<form method=\"POST\" action=\"/me\"><label>Blurb</label><textarea name=\"blurb\" rows=\"4\" maxlength=\"2000\">"+html_escape(b)+
            "</textarea><button>Save</button></form>"
            "<p class=\"muted\">Author CSS is not on this route. A platform account is not a CitizenId.</p></div>";
        html += jukebox_widget(user, true, qs);
        return pg("Profile", html);
    }

    if(method=="GET"&&path.rfind("/u/",0)==0) {
        const std::string who = path.substr(3);
        if (!valid_username(who))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        auto found = g_store.find_by_username(who);
        if (!found)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        if (acc && g_store.is_blocked(user, found->username))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        auto b = g_store.blurb(found->id);
        std::string html = "<div class=\"card\"><div class=\"who\">"+portrait_tag(found->username,"")+
            "<div><h1>"+html_escape(found->username)+"</h1>"
            "<p>"+html_escape(b.empty()?"No blurb yet.":b)+"</p>"
            "<p class=\"muted\">Public profile. Not a CitizenId. Not a seated office.</p></div></div>";
        if (acc && acc->username == found->username) {
            html += "<p><a href=\"/me\">Edit profile</a></p>";
        } else if (acc && g_store.are_friends(user, found->username)) {
            html += "<p class=\"row\"><a class=\"btn\" href=\"/mail?with="+html_escape(found->username)+"\">Message</a>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(found->username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form></p>";
        } else if (acc) {
            html += "<form method=\"POST\" action=\"/friends\"><input type=\"hidden\" name=\"who\" value=\""+
                    html_escape(found->username)+"\"><button>Send friend request</button></form>";
        }
        html += "</div>";
        html += jukebox_widget(found->username, acc && acc->username == found->username, qs);
        return pg(found->username, html);
    }

    if(method=="GET"&&path=="/friends") {
        if(!acc) return http_redir("/login");
        std::string err;
        if (qs["err"]=="inner") err = "<p class=\"warn\">Inner circle holds twelve. Take someone out first.</p>";
        std::string html = "<h1>People</h1>"+err+
            "<div class=\"card\"><h2>Find someone</h2>"
            "<form method=\"GET\" action=\"/search\"><label>Exact username, or three letters for typeahead</label>"
            "<div class=\"row\"><input name=\"q\" required><button>Search</button></div></form></div>"
            "<div class=\"card\"><h2>Add by exact name</h2>"
            "<form method=\"POST\" action=\"/friends\"><input name=\"who\" placeholder=\"username\" required>"
            "<button>Send request</button></form></div>";
        html += "<div class=\"card\"><h2>Friends</h2>";
        auto pals = g_store.friends_of(user);
        auto inner = g_store.inner_of(user);
        std::set<std::string> inner_set(inner.begin(), inner.end());
        if(pals.empty()) html += "<p class=\"empty\">None yet.</p>";
        for (auto& other : pals) {
            const bool inn = inner_set.count(other);
            html += "<div class=\"person\"><div class=\"who\">"+portrait_tag(other,"sm")+"<div><a href=\"/u/"+html_escape(other)+"\"><strong>"+html_escape(other)+"</strong></a>"
                    +(inn?" <span class=\"muted\">inner circle</span>":"")+
                    "</div></div><div class=\"row\">"
                    "<a class=\"btn ghost\" href=\"/mail?with="+html_escape(other)+"\">Mail</a>";
            if (inn) {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner-remove\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">From inner</button></form>";
            } else {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">Inner circle</button></form>";
            }
            html += "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/block\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"danger\" type=\"submit\">Block</button></form></div></div>";
        }
        html += "<p class=\"muted\">Inner circle is capped at twelve. Outer circle is everyone else you have accepted — derived, not stored as a second list.</p></div>";
        html += "<div class=\"card\"><h2>Incoming</h2>";
        auto inc = g_store.incoming_requests(user);
        if(inc.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& from : inc) {
            html += "<div class=\"person\"><span>"+html_escape(from)+"</span>"
                    "<form method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button>Accept</button></form></div>";
        }
        html += "</div><div class=\"card\"><h2>Outgoing</h2>";
        auto outg = g_store.outgoing_requests(user);
        if(outg.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& to : outg) html += "<div class=\"person\"><span>"+html_escape(to)+"</span><span class=\"muted\">waiting</span></div>";
        html += "<p class=\"muted\">There is no declined notice. Silence and a declined request look the same.</p></div>";
        return pg("People", html);
    }

    if(method=="GET"&&path=="/search") {
        if(!acc) return http_redir("/login");
        auto term = trim_ws(qs["q"]);
        std::string html = "<h1>Search</h1><div class=\"card\"><form method=\"GET\" action=\"/search\">"
            "<label>Find a person</label><div class=\"row\"><input name=\"q\" value=\""+html_escape(term)+"\">"
            "<button>Search</button></div></form><p class=\"muted\">Friends always appear. "
            "Everyone else follows their findability. A miss is a miss — missing, opted out, and blocked look the same.</p><ul class=\"plain\">";
        std::vector<std::string> hits;
        if (!term.empty()) {
            if (g_store.is_findable_exact(user, term)) {
                if (auto found = g_store.find_by_username(term)) hits.push_back(found->username);
            }
            if (term.size() >= 3) {
                for (auto& n : g_store.typeahead(user, term)) {
                    if (std::find(hits.begin(), hits.end(), n)==hits.end()) hits.push_back(n);
                }
            }
        }
        if (term.empty()) html += "<li class=\"muted\">Type a name.</li>";
        else if (hits.empty()) html += "<li class=\"muted\">No match.</li>";
        for (auto& n : hits) {
            html += "<li class=\"who\">"+portrait_tag(n,"sm")+"<a href=\"/u/"+html_escape(n)+"\">"+html_escape(n)+"</a></li>";
        }
        html += "</ul></div>";
        return pg("Search", html);
    }

    if(method=="GET"&&path=="/mail") {
        if(!acc) return http_redir("/login");
        auto other = qs["with"];
        std::string html = "<h1>Mail</h1><div class=\"card\"><p class=\"muted\">Direct messages are friends-only in this release. "
            "Open messaging waits on age assurance so a stranger cannot reach a minor. We will not treat a checkbox as age.</p><ul class=\"plain\">";
        auto pals = g_store.friends_of(user);
        if (pals.empty()) html += "<li class=\"muted\">Friend someone first.</li>";
        for (auto& n : pals) html += "<li><a href=\"/mail?with="+html_escape(n)+"\">"+html_escape(n)+"</a></li>";
        html += "</ul></div>";
        if (!other.empty() && g_store.are_friends(user, other) && !g_store.is_blocked(user, other)) {
            html += "<div class=\"card\"><h2>"+html_escape(other)+"</h2>";
            if (qs["err"]=="type") html += "<p class=\"warn\">That attachment isn't a usable type. Allowed: PNG/JPEG/GIF, PDF, plain text, macro-free Word/Excel/PowerPoint. No SVG, no archives.</p>";
            else if (qs["err"]=="large") html += "<p class=\"warn\">That attachment is too large.</p>";
            else if (qs["err"]=="active") html += "<p class=\"warn\">That document was refused: it declares a macro, embedded object, or OLE content.</p>";
            else if (qs["err"]=="scan") html += "<p class=\"warn\">That attachment could not be stored.</p>";
            auto thread = g_store.mail_with(user, other);
            if (thread.empty()) html += "<p class=\"empty\">No messages yet.</p>";
            for (auto& m : thread) {
                html += "<div class=\"msg\"><div><strong>"+html_escape(m.from)+"</strong> "
                        "<span class=\"muted\">"+html_escape(fmt_time(m.ts))+"</span>";
                if (!m.text.empty()) html += "<p>"+html_escape(m.text)+"</p>";
                if (m.attachment_blob_id) {
                    html += "<p><a href=\"/mail/attachment/"+html_escape(*m.attachment_blob_id)+"\">Download attachment</a> "
                            "<span class=\"muted\">"+html_escape(m.attachment_mime.value_or(""))+"</span></p>";
                }
                html += "</div></div>";
            }
            html += "<form method=\"POST\" action=\"/mail\" enctype=\"multipart/form-data\">"
                    "<input type=\"hidden\" name=\"to\" value=\""+html_escape(other)+"\">"
                    "<label>Message</label><input name=\"text\" maxlength=\"2000\">"
                    "<label>Attachment (optional)</label>"
                    "<input type=\"file\" name=\"attachment\" "
                    "accept=\"image/png,image/jpeg,image/gif,application/pdf,text/plain,.docx,.xlsx,.pptx\">"
                    "<p class=\"muted\">Scanned, content-sniffed, default-deny. No SVG. No archives.</p>"
                    "<button>Send</button></form></div>";
        }

        html += "<div class=\"card\"><h2>Groups</h2>"
            "<p class=\"muted\">Invite a friend and they join right away. Invite someone you're not "
            "friends with yet and they get a pending invite to accept before they can post. Adding "
            "someone into a room that would put a minor with a stranger is refused with a generic "
            "message — start a new group instead.</p>";
        auto groups = g_store.conversations_for(acc->id);
        std::string list;
        for (auto& conv : groups) {
            if (!conv.is_group) continue;
            bool pending = false;
            for (auto& m : conv.members) if (m.account == acc->id) pending = m.state == MemberState::Pending;
            list += "<li><a href=\"/mail/g/" + std::to_string(conv.id) + "\">" +
                    html_escape(group_label(conv, acc->id)) + "</a>" +
                    (pending ? " <span class=\"muted\">invited</span>" : "") + "</li>";
        }
        html += "<ul class=\"plain\">" + (list.empty() ? "<li class=\"muted\">No groups yet.</li>" : list) +
                "</ul>"
                "<form method=\"POST\" action=\"/mail/group/new\">"
                "<label>Start a group — usernames, space or comma separated</label>"
                "<input name=\"who\" placeholder=\"alice bob carol\">"
                "<button>Create</button></form></div>";
        return pg("Mail", html);
    }

    if(method=="GET"&&path.rfind("/mail/attachment/",0)==0) {
        if(!acc) return http_redir("/login");
        const std::string blob_id = path.substr(17);
        auto found = g_store.find_mail_attachment(user, blob_id);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        const std::string other = (found->from == user) ? found->to : found->from;
        // Same gate as the thread view: attachments do not outlive the
        // friendship they were sent inside of. Do not open stranger DMs.
        if (!g_store.are_friends(user, other) || g_store.is_blocked(user, other) || g_store.is_blocked(other, user))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        const std::string mime = found->attachment_mime.value_or("application/octet-stream");
        auto bytes = g_blobs.get(blob_id);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string data(bytes->begin(), bytes->end());
        std::string extra = "Content-Disposition: attachment; filename=\""+attachment_filename_for_mime(mime)+"\"\r\n"
                             "Cache-Control: private, no-store\r\n";
        return http_raw("200 OK", mime.c_str(), data, extra);
    }

    if(method=="GET"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return http_redir("/login");
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(path.substr(8)); } catch(...) { conv_id = 0; }
        auto conv = conv_id ? g_store.get_conversation(conv_id) : std::nullopt;
        const ConversationMember* self_member = nullptr;
        if (conv) for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        if (!conv || !conv->is_group || !self_member)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        const std::string base = "/mail/g/" + std::to_string(conv_id);
        const bool active = self_member->state == MemberState::Active;

        std::string html = "<h1>"+html_escape(group_label(*conv, acc->id))+"</h1>"
            "<p class=\"muted\">Platform group conversation. Not an AVND locality.</p>";
        if (qs["err"]=="invite") html += "<p class=\"warn\">Conflicts with another member’s contact settings.</p>";
        html += "<div class=\"card\"><h2>Members</h2><ul class=\"plain\">";
        for (auto& m : conv->members) {
            html += "<li>"+html_escape(display_name_for(m.account))+
                    (m.state==MemberState::Pending?" <span class=\"muted\">invited</span>":"")+"</li>";
        }
        html += "</ul>";
        if (!active) {
            html += "<p class=\"muted\">You’re invited to this group. Accept to join, or decline — declining is never announced to the room.</p>"
                    "<form method=\"POST\" action=\""+base+"/accept\"><button>Accept</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\"><button class=\"ghost\">Decline</button></form>";
        } else {
            html += "<form method=\"POST\" action=\""+base+"/invite\"><label>Invite</label>"
                    "<div class=\"row\"><input name=\"who\" required><button>Invite</button></div></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\"><button class=\"ghost\">Leave group</button></form>";
        }
        html += "</div>";

        if (active) {
            html += "<div class=\"card\">";
            auto msgs = g_store.messages_in(conv_id);
            bool any = false;
            for (auto& m : msgs) {
                if (m.deletion==DeletionState::DeletedForSender && m.sender==acc->id) continue;
                any = true;
                html += "<div class=\"msg\"><div><strong>"+html_escape(display_name_for(m.sender))+"</strong> "
                        "<span class=\"muted\">"+html_escape(fmt_time(m.sent_at))+"</span>";
                if (m.deletion==DeletionState::DeletedForEveryone) {
                    html += "<p class=\"muted\"><em>Message deleted.</em></p>";
                } else {
                    html += "<p>"+html_escape(m.text)+"</p>";
                    if (m.edited_at>0) html += "<p class=\"muted\">(edited)</p>";
                    if (m.sender==acc->id) {
                        const std::string mbase = base+"/message/"+std::to_string(m.id);
                        html += "<form method=\"POST\" action=\""+mbase+"/edit\" class=\"row\">"
                                "<input name=\"text\" value=\""+html_escape(m.text)+"\" maxlength=\"2000\">"
                                "<button>Save edit</button></form>"
                                "<form method=\"POST\" action=\""+mbase+"/delete\">"
                                "<button class=\"ghost\">Delete</button></form>";
                    }
                }
                html += "</div></div>";
            }
            if (!any) html += "<p class=\"empty\">No messages yet.</p>";
            html += "<form method=\"POST\" action=\""+base+"/send\">"
                    "<label>Message</label><input name=\"text\" maxlength=\"2000\">"
                    "<button>Send</button></form></div>";
        }
        return pg("Group", html);
    }

    if(method=="GET"&&path=="/communities") {
        if(!acc) return http_redir("/login");
        std::string err;
        if (qs["err"]=="slug") err = "<p class=\"warn\">Need a free slug: letters, numbers, dots, underscores, hyphens.</p>";
        std::string html = "<h1>Communities</h1>"+err+
            "<p class=\"lede\">These are platform groups. They are not AVND localities and they do not confer standing.</p>"
            "<div class=\"card\"><h2>Start one</h2><form method=\"POST\" action=\"/communities\">"
            "<label>Slug</label><input name=\"slug\" required maxlength=\"32\" pattern=\"[A-Za-z0-9._-]+\" placeholder=\"town-hall\">"
            "<label>Name</label><input name=\"name\" required maxlength=\"80\">"
            "<label>About</label><textarea name=\"about\" maxlength=\"2000\"></textarea>"
            "<button>Create</button></form></div><div class=\"card\"><h2>All communities</h2>";
        auto all = g_store.all_communities();
        if (all.empty()) html += "<p class=\"empty\">None yet.</p>";
        for (auto& c : all) {
            html += "<div class=\"person\"><div><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+html_escape(c.name)+
                    "</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span>"
                    "<p class=\"muted\">"+html_escape(c.about)+"</p></div></div>";
        }
        html += "</div>";
        return pg("Communities", html);
    }

    if(method=="GET"&&path.rfind("/c/",0)==0) {
        if(!acc) return http_redir("/login");
        const std::string slug = path.substr(3);
        auto c = g_store.community(slug);
        if (!c)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such community.</p></div>"));
        const bool member = g_store.is_member(slug, user);
        const bool owner = c->owner == user;
        std::string html = "<h1>"+html_escape(c->name)+"</h1>"
            "<p class=\"muted\">/"+html_escape(c->slug)+" · started by "+html_escape(c->owner)+
            " · a platform community, not a locality</p>";
        if (qs["err"]=="invite") html += "<p class=\"warn\">Conflicts with another member’s contact settings.</p>";
        html += "<div class=\"card\"><p>"+html_escape(c->about.empty()?"No description.":c->about)+"</p>";
        if (!member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/join\"><button>Join</button></form>";
        } else if (!owner) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/leave\"><button class=\"ghost\">Leave</button></form>";
        } else {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/delete\" onsubmit=\"return confirm('Delete this community?');\">"
                    "<button class=\"danger\">Delete community</button></form>";
        }
        html += "</div>";
        if (member) {
            html += "<div class=\"card\"><form method=\"POST\" action=\"/c/"+html_escape(slug)+"\">"
                    "<label>Post in "+html_escape(c->name)+"</label>"
                    "<textarea name=\"text\" required maxlength=\"2000\"></textarea><button>Post</button></form></div>";
            html += "<div class=\"card\"><h2>Invite</h2>"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/invite\">"
                    "<label>Username</label><div class=\"row\"><input name=\"who\" required><button>Invite</button></div></form>"
                    "<p class=\"muted\">People can also join themselves. An invite that conflicts with contact settings is refused with a generic error.</p></div>";
        }
        html += "<div class=\"card\"><h2>Members</h2><ul class=\"plain\">";
        for (auto& n : g_store.members_of(slug)) {
            html += "<li><a href=\"/u/"+html_escape(n)+"\">"+html_escape(n)+"</a>"+(n==c->owner?" <span class=\"muted\">starter</span>":"")+"</li>";
        }
        html += "</ul></div>";
        auto posts = g_store.posts_in(slug, 80);
        if (posts.empty()) html += "<p class=\"empty\">No posts yet.</p>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p)+"</div>";
        return pg(c->name, html);
    }

    if(method=="GET"&&path=="/notices") {
        if(!acc) return http_redir("/login");
        g_store.mark_notices_read(user);
        std::string html = "<h1>Notices</h1><div class=\"card\">";
        auto notes = g_store.notices_for(user, 50);
        if (notes.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& n : notes) {
            std::string line;
            if (n.type=="friend_request") line = html_escape(n.from)+" sent a friend request.";
            else if (n.type=="friend_accept") line = html_escape(n.from)+" accepted your request.";
            else if (n.type=="message") line = html_escape(n.from)+" sent mail.";
            else if (n.type=="group_invite") line = html_escape(n.from)+" added you to /"+html_escape(n.extra)+".";
            else line = html_escape(n.type);
            html += "<div class=\"person\"><div>"+line+"<div class=\"muted\">"+html_escape(fmt_time(n.ts))+"</div></div></div>";
        }
        html += "<p class=\"muted\">There is no notice for a declined friend request.</p></div>";
        return pg("Notices", html);
    }

    if(method=="GET"&&path=="/settings") {
        if(!acc) return http_redir("/login");
        auto p = g_store.prefs_of(user);
        auto find_sel = [&](Findability f){ return p.findability==f?" checked":""; };
        std::string html = "<h1>Settings</h1>"
            "<p class=\"muted\">Five sections. Not a junk drawer. Not kernel seats.</p>";
        if (qs["ok"]=="password") html += "<p class=\"ok\">Password changed.</p>";
        if (qs["err"]=="password") html += "<p class=\"warn\">Could not change password. Check the current one.</p>";
        if (qs["err"]=="delete") html += "<p class=\"warn\">Type your username to delete.</p>";

        html += "<div class=\"card\"><h2>Security</h2>"
            "<form method=\"POST\" action=\"/settings/password\">"
            "<label>Current password</label><input type=\"password\" name=\"current\" required autocomplete=\"current-password\">"
            "<label>New password</label><input type=\"password\" name=\"next\" required autocomplete=\"new-password\">"
            "<button>Change password</button></form><h2>Sessions</h2>";
        auto all = g_store.sessions_for(acc->id);
        int shown = 0;
        for (const auto& s : all) {
            if (s.revoked) continue;
            if (s.expires_at <= now()) continue;
            ++shown;
            const bool current = s.id == sid;
            html += "<div class=\"sess\"><div><strong>"+html_escape(s.device_label.empty()?"web":s.device_label)+"</strong>"
                    +(current?" <span class=\"muted\">this device</span>":"")+
                    "<div class=\"muted\">Started "+html_escape(fmt_time(s.created_at))+
                    " · last seen "+html_escape(fmt_time(s.last_seen))+"</div></div>";
            if (!current) {
                html += "<form method=\"POST\" action=\"/settings/revoke\">"
                        "<input type=\"hidden\" name=\"sid\" value=\""+html_escape(s.id)+"\">"
                        "<button class=\"danger\" type=\"submit\">Revoke</button></form>";
            }
            html += "</div>";
        }
        if (!shown) html += "<p class=\"muted\">None.</p>";
        html += "<form method=\"POST\" action=\"/settings/logout-all\" style=\"margin-top:1rem\">"
                "<button class=\"danger\" type=\"submit\">Log out other devices</button></form>"
                "<p class=\"muted\">Phone change with delay is not in this slice. Passkeys are later.</p></div>";

        html += std::string("<div class=\"card\"><h2>Privacy</h2><form method=\"POST\" action=\"/settings/privacy\">")
            + "<label>Findability</label>"
            + "<p class=\"muted\">One axis, three states. Each level includes the one above it.</p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"0\"" + find_sel(Findability::FriendsOnly) + "> Friends only</label></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"1\"" + find_sel(Findability::ExactMatch) + "> Exact match</label></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"2\"" + find_sel(Findability::Typeahead) + "> Typeahead</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"group_invite_confirm\" value=\"1\"" + (p.group_invite_confirm?" checked":"") + "> "
            + "Don't add me to groups without asking</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"presence_hide\" value=\"1\"" + (p.presence_hide?" checked":"") + "> "
            + "Hide presence (presence is not shown in this release)</label></p>"
            + "<p class=\"muted\">Direct messages are friends-only for everyone until age can be assured. "
            + "That is policy, not a self-reported minor toggle.</p>"
            + "<button>Save privacy</button></form><h2>Block list</h2>";
        auto bl = g_store.blocked_by(user);
        if (bl.empty()) html += "<p class=\"empty\">No one blocked.</p>";
        for (auto& n : bl) {
            html += "<div class=\"person\"><span>"+html_escape(n)+"</span>"
                    "<form method=\"POST\" action=\"/settings/unblock\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(n)+"\">"
                    "<button class=\"ghost\">Unblock</button></form></div>";
        }
        html += "</div>";

        html += "<div class=\"card\"><h2>Identity</h2>"
            "<p>No civic identity is linked to this platform account.</p>"
            "<p class=\"muted\">Combined verification for voting is not live. Unlink is not available because there is nothing to unlink. "
            "Unlink would drop voting access until re-verified; it would not resign citizenship. "
            "This page does not write CitizenId, GrantCitizenship, points, seats, or parcels.</p></div>";

        html += "<div class=\"card\"><h2>Data</h2>"
            "<p><a class=\"btn ghost\" href=\"/export\">Download your export</a></p>"
            "<p class=\"muted\">Export includes username, blurb, your friend list, communities you started or joined, "
            "posts you wrote, settings, and messages you sent. It does not include other people’s text or your password hash.</p>"
            "<form method=\"POST\" action=\"/settings/delete\">"
            "<label>Delete account — type your username</label>"
            "<input name=\"confirm\" placeholder=\""+html_escape(user)+"\" autocomplete=\"off\">"
            "<button class=\"danger\">Delete account</button></form>"
            "<p class=\"muted\">Sessions end immediately. Mail you sent is tombstoned as [deleted account]. "
            "The phone binding is held ninety days so a delete cannot shed a suspension. This is not ResignCitizen.</p></div>";

        html += std::string("<div class=\"card\"><h2>Notifications</h2><form method=\"POST\" action=\"/settings/notify\">")
            + "<p><label><input type=\"checkbox\" name=\"notify_friend\" value=\"1\"" + (p.notify_friend?" checked":"") + "> Friend requests and accepts</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_message\" value=\"1\"" + (p.notify_message?" checked":"") + "> New mail</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_group\" value=\"1\"" + (p.notify_group?" checked":"") + "> Community invites</label></p>"
            + "<p class=\"muted\">New-device login, moderation, and age re-verification are always on when those events exist. "
            + "There is no FriendRequestDeclined.</p>"
            + "<button>Save notifications</button></form></div>";
        return pg("Settings", html);
    }

    if(method=="GET"&&path=="/export") {
        if(!acc) return http_redir("/login");
        std::ostringstream o;
        o << "LiquidGov.US platform export\n";
        o << "This is platform data. It is not the governance kernel.\n\n";
        o << "username: " << user << "\n";
        o << "created_at: " << fmt_time(acc->created_at) << "\n";
        o << "blurb: " << g_store.blurb(acc->id) << "\n\n";
        o << "friends:\n";
        for (auto& n : g_store.friends_of(user)) o << "  " << n << "\n";
        o << "\ncommunities:\n";
        for (auto& c : g_store.communities_of(user)) o << "  " << c.slug << " " << c.name << "\n";
        o << "\nposts you wrote:\n";
        for (auto& p : g_store.posts_by(user)) {
            o << "  [" << fmt_time(p.ts) << "] ";
            if (!p.community.empty()) o << "/" << p.community << " ";
            o << p.text << "\n";
        }
        o << "\nmessages you sent:\n";
        for (auto& m : g_store.mail_sent_by(user)) {
            o << "  to " << m.to << " [" << fmt_time(m.ts) << "] " << m.text << "\n";
        }
        auto pr = g_store.prefs_of(user);
        o << "\nsettings:\n";
        o << "  findability: " << static_cast<int>(pr.findability) << "\n";
        o << "  group_invite_confirm: " << (pr.group_invite_confirm?"yes":"no") << "\n";
        std::string extra = "Content-Disposition: attachment; filename=\"liquidgov-export.txt\"\r\n";
        extra += "Cache-Control: no-store\r\n";
        return http_raw("200 OK", "text/plain; charset=utf-8", o.str(), extra);
    }

    if(method=="POST"&&path=="/signup") {
        auto f=parse_form(body);
        auto uname=trim_ws(f["username"]);
        auto phone=trim_ws(f["phone"]);
        auto pass=f["password"];
        if (!valid_username(uname)) {
            std::string err = uname.empty()
                ? "Need a username: one word, 1–32 letters/numbers (dots, underscores, hyphens ok). No spaces, no @."
                : ("“"+uname+"” isn’t a valid username. One word: letters, numbers, dots, underscores, hyphens. No spaces.");
            return http_ok(signup_page(user, unread, err, uname, phone));
        }
        if (pass.empty() || phone.empty())
            return http_ok(signup_page(user, unread, "Need username, password, and phone.", uname, phone));
        if (g_store.phone_quarantined(fingerprint_phone(phone)))
            return http_ok(signup_page(user, unread, "Could not create that account.", uname, phone));
        auto r=create_account(g_store,g_breach,uname,pass,phone);
        if(r.error==AuthError::None) {
            if (auto created = g_store.find_by_username(uname))
                g_store.set_phone_fp(created->id, fingerprint_phone(phone));
            r=login(g_store,g_limiter,uname,pass,device_label_from(req,peer_ip),peer_ip);
        }
        if(r.error==AuthError::UsernameTaken)
            return http_ok(signup_page(user, unread, "That username is taken. Pick another.", uname, phone));
        if(r.error!=AuthError::None||!r.session)
            return http_ok(signup_page(user, unread, "Need username, password, and phone.", uname, phone));
        return http_redir("/me", set_cookie(r.session->id));
    }
    if(method=="POST"&&path=="/login") {
        auto f=parse_form(body);
        auto r=login(g_store,g_limiter,trim_ws(f["username"]),f["password"],device_label_from(req,peer_ip),peer_ip);
        if(r.error!=AuthError::None||!r.session)
            return http_ok(wrap("Log in","",0,"<div class=\"card\"><p>Invalid username or password. <a href=\"/login\">Back</a></p></div>"));
        return http_redir("/feed", set_cookie(r.session->id));
    }
    if(method=="POST"&&path=="/me") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        auto b=f["blurb"]; if(b.size()>2000) b.resize(2000);
        g_store.set_blurb(acc->id, b);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/avatar") {
        if(!acc) return http_redir("/login");
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "photo");
        if (!file || file->empty()) return http_redir("/me?err=image");
        auto r = save_avatar(g_blobs, g_store, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me?err=large");
        if (r.error == SaveAvatarError::FailedScan) return http_redir("/me?err=scan");
        if (r.error != SaveAvatarError::None) return http_redir("/me?err=image");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/avatar/clear") {
        if(!acc) return http_redir("/login");
        clear_avatar(g_blobs, g_store, acc->id);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/add") {
        if(!acc) return http_redir("/login");
        auto url = trim_ws(parse_form(body)["url"]);
        auto id = extract_youtube_id(url);
        if (!id) return http_redir("/me?jerr=url");
        if (!g_store.add_jukebox_track(user, *id)) return http_redir("/me?jerr=full");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/remove") {
        if(!acc) return http_redir("/login");
        g_store.remove_jukebox_track(user, parse_form(body)["video_id"]);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/feed") {
        if(!acc) return http_redir("/login");
        auto text=trim_ws(parse_form(body)["text"]);
        if(!text.empty()) {
            if(text.size()>2000) text.resize(2000);
            g_store.add_post(user, text, "");
        }
        return http_redir("/feed");
    }
    if(method=="POST"&&path=="/friends") {
        if(!acc) return http_redir("/login");
        auto who=trim_ws(parse_form(body)["who"]);
        if(valid_username(who) && who!=user && g_store.find_by_username(who)
           && !g_store.is_blocked(user, who) && !g_store.is_blocked(who, user)) {
            g_store.add_friend_request(user, who);
        }
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/accept") {
        if(!acc) return http_redir("/login");
        auto who=parse_form(body)["who"];
        g_store.accept_friend_request(who, user);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/unfriend") {
        if(!acc) return http_redir("/login");
        g_store.unfriend(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/block") {
        if(!acc) return http_redir("/login");
        auto who=parse_form(body)["who"];
        if (valid_username(who) && who!=user) g_store.block(user, who);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner") {
        if(!acc) return http_redir("/login");
        if (!g_store.add_inner(user, parse_form(body)["who"]))
            return http_redir("/friends?err=inner");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner-remove") {
        if(!acc) return http_redir("/login");
        g_store.remove_inner(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/mail") {
        if(!acc) return http_redir("/login");
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        std::string ctype = header_value(hdrs, "Content-Type");
        std::string to, text;
        std::optional<std::vector<std::uint8_t>> file;
        if (ctype.find("multipart/form-data") != std::string::npos) {
            to = multipart_field(ctype, body, "to");
            text = multipart_field(ctype, body, "text");
            file = multipart_file(ctype, body, "attachment");
        } else {
            auto f = parse_form(body);
            to = f["to"]; text = f["text"];
        }
        if(text.size()>2000) text.resize(2000);
        // Do not open stranger DMs: friends-only, both directions unblocked,
        // exactly as before attachments existed.
        if(to.empty() || !g_store.are_friends(user,to) || g_store.is_blocked(user,to) || g_store.is_blocked(to,user))
            return http_redir("/mail?with="+to);

        std::optional<std::string> blob_id, mime;
        if (file && !file->empty()) {
            auto r = ingest_file(g_blobs, g_mail_scanner, g_mail_caps, *file);
            if (r.error != FileTransferError::None || !r.blob_id) {
                const char* code = "type";
                switch (r.error) {
                    case FileTransferError::TooLarge: code = "large"; break;
                    case FileTransferError::ActiveContentBlocked: code = "active"; break;
                    case FileTransferError::FailedScan: code = "scan"; break;
                    default: code = "type"; break;
                }
                return http_redir("/mail?with="+to+"&err="+code);
            }
            blob_id = r.blob_id;
            mime = r.stored_mime_type;
        }
        if (!text.empty() || blob_id) g_store.add_mail(user,to,text,blob_id,mime);
        return http_redir("/mail?with="+to);
    }

    if(method=="POST"&&path=="/mail/group/new") {
        if(!acc) return http_redir("/login");
        const auto who = parse_form(body)["who"];
        std::vector<AccountId> invitees;
        std::string cur;
        auto flush_tok = [&]() {
            const auto name = trim_ws(cur);
            cur.clear();
            if (name.empty() || name == user) return;
            auto found = g_store.find_by_username(name);
            if (!found) return; // unknown username: silently dropped
            if (g_store.is_blocked(user, name) || g_store.is_blocked(name, user)) return;
            for (auto id : invitees) if (id == found->id) return; // dedupe
            invitees.push_back(found->id);
        };
        for (char c : who) {
            if (c==','||c==' '||c=='\t'||c=='\n'||c=='\r') flush_tok();
            else cur += c;
        }
        flush_tok();
        auto result = create_conversation(g_store, g_social_graph, acc->id, invitees, /*is_group=*/true);
        return http_redir("/mail/g/"+std::to_string(*result.conversation_id));
    }

    if(method=="POST"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return http_redir("/login");
        std::string rest = path.substr(8);
        auto slash = rest.find('/');
        const std::string id_str = (slash==std::string::npos) ? rest : rest.substr(0,slash);
        const std::string action = (slash==std::string::npos) ? std::string() : rest.substr(slash+1);
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(id_str); } catch(...) { return http_redir("/mail"); }
        auto conv = g_store.get_conversation(conv_id);
        if (!conv || !conv->is_group) return http_redir("/mail");
        const ConversationMember* self_member = nullptr;
        for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        const std::string base = "/mail/g/"+id_str;

        if (action=="accept") {
            if (!self_member) return http_redir("/mail");
            accept_invite(g_store, acc->id, conv_id);
            return http_redir(base);
        }
        if (action=="leave") {
            if (!self_member) return http_redir("/mail");
            leave_conversation(g_store, acc->id, conv_id); // Pending "decline" and Active "leave" alike; never notified
            return http_redir("/mail");
        }
        // Everything below requires being an Active member.
        if (!self_member || self_member->state != MemberState::Active) return http_redir("/mail");

        if (action=="invite") {
            const auto who = trim_ws(parse_form(body)["who"]);
            auto found = g_store.find_by_username(who);
            bool refuse = !found || g_store.is_blocked(user, who) || g_store.is_blocked(who, user);
            if (!refuse) {
                auto r = add_member(g_store, g_social_graph, acc->id, found->id, conv_id);
                refuse = r.error != MessengerError::None;
            }
            return http_redir(refuse ? (base+"?err=invite") : base);
        }
        if (action=="send") {
            auto text = trim_ws(parse_form(body)["text"]);
            if (!text.empty()) {
                if (text.size()>2000) text.resize(2000);
                send_message(g_store, acc->id, conv_id, text);
            }
            return http_redir(base);
        }
        if (action.rfind("message/",0)==0) {
            const std::string mrest = action.substr(8);
            const auto mslash = mrest.find('/');
            if (mslash==std::string::npos) return http_redir(base);
            MessageId msg_id = 0;
            try { msg_id = std::stoull(mrest.substr(0,mslash)); } catch(...) { return http_redir(base); }
            const std::string mact = mrest.substr(mslash+1);
            if (mact=="edit") {
                auto text = trim_ws(parse_form(body)["text"]);
                if (!text.empty()) {
                    if (text.size()>2000) text.resize(2000);
                    edit_message(g_store, acc->id, msg_id, text);
                }
            } else if (mact=="delete") {
                delete_message(g_store, acc->id, msg_id, /*for_everyone=*/true);
            }
            return http_redir(base);
        }
        return http_redir(base);
    }
    if(method=="POST"&&path=="/communities") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        auto slug=trim_ws(f["slug"]);
        auto name=trim_ws(f["name"]);
        auto about=f["about"]; if(about.size()>2000) about.resize(2000);
        Community c;
        c.slug = slug; c.name = name.empty()?slug:name; c.owner = user; c.about = about; c.created_at = now();
        if (!g_store.create_community(c)) return http_redir("/communities?err=slug");
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path.rfind("/c/",0)==0) {
        if(!acc) return http_redir("/login");
        std::string rest = path.substr(3);
        std::string slug, action;
        auto slash = rest.find('/');
        if (slash==std::string::npos) { slug = rest; action = "post"; }
        else { slug = rest.substr(0,slash); action = rest.substr(slash+1); }
        if (!g_store.community(slug)) return http_redir("/communities");
        if (action=="join") { g_store.join_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="leave") { g_store.leave_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="delete") { g_store.delete_community(slug, user); return http_redir("/communities"); }
        if (action=="invite") {
            auto who=trim_ws(parse_form(body)["who"]);
            if (!g_store.invite_community(slug, user, who)) return http_redir("/c/"+slug+"?err=invite");
            return http_redir("/c/"+slug);
        }
        if (action=="post") {
            auto text=trim_ws(parse_form(body)["text"]);
            if (!text.empty() && g_store.is_member(slug, user)) {
                if (text.size()>2000) text.resize(2000);
                g_store.add_post(user, text, slug);
            }
            return http_redir("/c/"+slug);
        }
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path=="/settings/revoke") {
        if(!acc) return http_redir("/login");
        auto target = parse_form(body)["sid"];
        bool current = target == sid;
        bool owned = false;
        for (const auto& s : g_store.sessions_for(acc->id)) {
            if (s.id == target) { owned = true; break; }
        }
        if (owned) logout(g_store, target);
        if (current) return http_redir("/login", clear_cookie());
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/logout-all") {
        if(!acc) return http_redir("/login");
        logout_all_devices(g_store, acc->id, sid);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/password") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        if (!verify_password(f["current"], acc->password_hash) || f["next"].empty())
            return http_redir("/settings?err=password");
        if (g_breach.is_breached(f["next"]))
            return http_redir("/settings?err=password");
        g_store.set_password_hash(acc->id, hash_password(f["next"]));
        return http_redir("/settings?ok=password");
    }
    if(method=="POST"&&path=="/settings/privacy") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        int fv = 1;
        try { fv = std::stoi(f["findability"]); } catch (...) { fv = 1; }
        if (fv < 0 || fv > 2) fv = 1;
        p.findability = static_cast<Findability>(fv);
        p.group_invite_confirm = f["group_invite_confirm"] == "1";
        p.presence_hide = f["presence_hide"] == "1";
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/notify") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        p.notify_friend = f["notify_friend"]=="1";
        p.notify_message = f["notify_message"]=="1";
        p.notify_group = f["notify_group"]=="1";
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/unblock") {
        if(!acc) return http_redir("/login");
        g_store.unblock(user, parse_form(body)["who"]);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/delete") {
        if(!acc) return http_redir("/login");
        auto confirm = trim_ws(parse_form(body)["confirm"]);
        if (confirm != user) return http_redir("/settings?err=delete");
        clear_avatar(g_blobs, g_store, acc->id);
        g_store.delete_account(acc->id);
        return http_redir("/", clear_cookie());
    }
    return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>Not found.</p></div>"));
}
}
int main() {
    for (const auto& av : g_store.all_avatars())
        g_blobs.adopt(av.blob_id, "image/png");
    for (const auto& m : g_store.all_mail_with_attachments())
        g_blobs.adopt(*m.attachment_blob_id, m.attachment_mime.value_or("application/octet-stream"));
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return 1;
    int yes=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(kPort); addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0){ perror("bind"); return 1; }
    listen(fd,16);
    std::cerr<<"LiquidGov http://0.0.0.0:"<<kPort<<" store="<<g_store.path()
             <<" blobs="<<blob_path_from_env()<<" (auth.cpp + FileStore LGP1)\n";
    for(;;){
        sockaddr_in peer{}; socklen_t plen=sizeof(peer);
        int c=accept(fd,reinterpret_cast<sockaddr*>(&peer),&plen); if(c<0) continue;
        timeval tv{}; tv.tv_sec=30; tv.tv_usec=0;
        setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        char ipbuf[INET_ADDRSTRLEN]="0.0.0.0"; inet_ntop(AF_INET,&peer.sin_addr,ipbuf,sizeof(ipbuf));
        std::string req;
        read_http_request(c, req);

        // /world/ws is the one route that doesn't get the ordinary
        // request/response treatment: a successful upgrade hands the fd to
        // its own thread and keeps it open, instead of sending one response
        // and closing. Every other route is untouched — still handled
        // synchronously on this single accept-loop thread, exactly as
        // before this feature existed.
        {
            auto line_end = req.find("\r\n");
            std::string method, rawpath;
            if (line_end != std::string::npos) {
                std::istringstream is(req.substr(0, line_end));
                is >> method >> rawpath;
            }
            if (method == "GET" && rawpath == "/world/ws" &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) username = acc->username;
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && !key.empty()) {
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread(run_world_session, c, username).detach();
                        continue; // socket now owned by the world session thread
                    }
                }
                close(c);
                continue;
            }
        }

        std::string resp;
        try {
            resp = handle(req,ipbuf);
        } catch (const std::exception& e) {
            std::cerr<<"handle error: "<<e.what()<<"\n";
            resp = http_code("500 Internal Server Error",
                page_wrap("Error", nav("",0), "<div class=\"card\"><p>Could not save. Try again.</p></div>"));
        }
        send(c,resp.data(),resp.size(),0); close(c);
    }
}
