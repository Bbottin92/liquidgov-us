#include "site/core/http.hpp"
#include "site/core/adapters_store.hpp"
#include "site/ui/favicon_cookies.hpp"

namespace liquidgov::site {
std::string url_decode(const std::string& s) {
    std::string o;
    for (size_t i=0;i<s.size();++i) {
        if (s[i]=='+') o+=' ';
        else if (s[i]=='%' && i+2<s.size()) { int v=0; std::stringstream ss; ss<<std::hex<<s.substr(i+1,2); ss>>v; o+=char(v); i+=2; }
        else o+=s[i];
    }
    return o;
}
// Percent-encodes for safe embedding in a query string. Needed specifically
// for values that didn't come through our own validation (e.g. a raw login
// username attempt, which -- unlike a stored account's username -- isn't
// guaranteed to be in valid_username's restricted charset).
std::string url_encode(const std::string& s) {
    static const char* kHex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') o += char(c);
        else { o += '%'; o += kHex[c >> 4]; o += kHex[c & 0xf]; }
    }
    return o;
}

// Relative path only. Used as a post-login landing so /world and /feed
// don't dump a new member on a generic /login with no way back.
std::string safe_next_path(const std::string& n) {
    if (n.size() < 2 || n[0] != '/' || n[1] == '/') return {};
    if (n.find('\r') != std::string::npos || n.find('\n') != std::string::npos) return {};
    if (n.find('\\') != std::string::npos || n.find("://") != std::string::npos) return {};
    if (n.size() > 200) return {};
    if (n.rfind("/login", 0) == 0 || n.rfind("/signup", 0) == 0 || n.rfind("/logout", 0) == 0) return {};
    return n;
}
std::string login_url(const std::string& next) {
    auto n = safe_next_path(next);
    if (n.empty()) return "/login";
    return "/login?next=" + url_encode(n);
}
int count_digits(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) if (c >= '0' && c <= '9') ++n;
    return n;
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
// "dark", "light", or "" -- "" means no explicit choice was ever saved, so
// the page stamps no data-theme attribute at all and prefers-color-scheme
// alone decides (see assets/site.css's dark block). Set once per request (handle()
// is the whole site's single-threaded synchronous entry point, so a
// thread_local read at render time is exactly one request's worth of
// state, never shared or stale across requests).
std::string theme_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return {};
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_theme="); if(k==std::string::npos) return {};
    k+=9; auto sc=line.find(';',k);
    std::string v = line.substr(k, sc==std::string::npos?std::string::npos:sc-k);
    return (v=="dark"||v=="light") ? v : std::string{};
}

bool drop_off_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return false;
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_drop="); if(k==std::string::npos) return false;
    k+=8; auto sc=line.find(';',k);
    return line.substr(k, sc==std::string::npos?std::string::npos:sc-k)=="off";
}

bool nsfw_on_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return false;
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_nsfw="); if(k==std::string::npos) return false;
    k+=8; auto sc=line.find(';',k);
    return line.substr(k, sc==std::string::npos?std::string::npos:sc-k)=="on";
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
// The client's address. nginx (on this machine) appends the address it saw to X-Forwarded-For
// ($proxy_add_x_forwarded_for), so only the LAST entry is trustworthy; anything before it was sent by the client and
// can be made up (until 2026-11-13 this took the first entry, so login throttling could be dodged with a fake header).
// The header is believed only from a loopback peer (nginx); a direct connection is judged by its own address.
std::string forwarded_ip(const std::string& req, const std::string& peer) {
    const bool via_proxy = peer.rfind("127.", 0) == 0;
    auto xff = via_proxy ? header_value(req, "X-Forwarded-For") : std::string();
    if (xff.empty()) return peer;
    auto comma = xff.rfind(',');
    std::string last = comma == std::string::npos ? xff : xff.substr(comma + 1);
    while (!last.empty() && (last.front()==' '||last.front()=='\t')) last.erase(last.begin());
    while (!last.empty() && (last.back()==' '||last.back()=='\t')) last.pop_back();
    return last.empty() ? peer : last;
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
// Date only, not a UTC timestamp -- the server can't know a visitor's
// timezone, so "2026-09-13 05:18 UTC" was just engineer-speak with no way
// to be locally correct. A bare date reads honestly everywhere it's used
// (posts, "member since", session activity, jukebox history) without
// needing a client-side reformat script for something this minor.
// Back to the page a post action came from, at that post (CODE-SCAN-UX A1, Brief 4 item 9: a 303 without a fragment
// lands at the top on iPhone Safari). Any #fragment the page had is replaced; no usable Referer is /feed.
// with_anchor: the same rule for every action that should come back at its item (posts, proposals).
std::string with_anchor(std::string url, const std::string& anchor) {
    auto hash = url.find('#');
    if (hash != std::string::npos) url.erase(hash);
    return url + "#" + anchor;
}
std::string back_to_post(const std::string& referer, std::uint64_t pid) {
    std::string back = referer;
    if (back.empty() || back.rfind("http", 0) != 0) back = "/feed";
    return with_anchor(back, "post-" + std::to_string(pid));
}
// Back to the page a proposal action came from (its next=, else /officials for a race, else /proposals), at that
// proposal (CODE-SCAN-UX A1, Brief 6 item 3: votes and reactions sit mid-page on a long list).
// err: a flash_error code added to the query (Brief 7 item 5: a refused answer says so where you are).
std::string back_to_proposal(const std::string& next, const Proposal& p, const std::string& err) {
    std::string back = safe_next_path(next);
    if (back.empty()) back = p.is_office ? "/officials" : "/proposals";
    if (!err.empty()) {
        auto hash = back.find('#');
        if (hash != std::string::npos) back.erase(hash);
        back += (back.find('?') == std::string::npos ? "?err=" : "&err=") + err;
    }
    return with_anchor(back, "proposal-" + std::to_string(p.id));
}

std::string fmt_time(std::int64_t t) {
    if (t <= 0) return "";
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

std::optional<PlatformAccount> account_from_req(const std::string& headers) {
    auto sid = cookie_of(headers);
    auto sess=g_store.get_session(sid);
    if(!sess) return std::nullopt;
    g_store.touch_session(sid);
    return g_store.find_by_id(sess->account);
}
// Every response may be framed only by liquidgov.us itself (the map's right-hand
// field), so nobody can disguise our buttons (clickjacking). Sent on EVERY page,
// not only framed loads (Brandon 2026-11-13, security gate run 43503116478):
// browsers that send no Sec-Fetch-Dest were unguarded. object-src 'none' and
// base-uri 'self' block plugin objects and a planted <base> tag. A full
// script-src policy waits until the last inline scripts move to assets/.
std::string frame_guard() {
    return "Content-Security-Policy: frame-ancestors 'self'; object-src 'none'; base-uri 'self'\r\n"
           "X-Frame-Options: SAMEORIGIN\r\n";
}
std::string http_ok(const std::string& body, const std::string& extra) {
    std::ostringstream o;
    o<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<frame_guard()<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_raw(const char* status, const char* ctype, const std::string& body, const std::string& extra) {
    std::ostringstream o;
    o<<"HTTP/1.1 "<<status<<"\r\nContent-Type: "<<ctype<<"\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<frame_guard()<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_code(const char* status, const std::string& body) {
    return http_raw(status, "text/html; charset=utf-8", body);
}
std::string http_redir(const std::string& loc, const std::string& extra) {
    std::ostringstream o;
    o<<"HTTP/1.1 303 See Other\r\nLocation: "<<loc<<"\r\nContent-Length: 0\r\nConnection: close\r\n"<<frame_guard()<<extra<<"\r\n";
    return o.str();
}

std::string trim_ws(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\r'||s[a]=='\n')) ++a;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'||s[b-1]=='\n')) --b;
    return s.substr(a, b-a);
}

// ASCII-only is fine here -- every email this ever runs on already passed
// looks_like_email (an ASCII shape check), and lowercasing is only ever
// used for the ml-tag dedup key, not for anything that has to be
// Unicode-correct.
std::string lowercase_ascii(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// sha256_hex (liquidgov_blob) takes bytes, not a std::string -- this is
// the one place that conversion happens, for hashing a password-reset
// token before it's stored (see POST /forgot / POST /reset).
std::string sha256_hex_of(const std::string& s) {
    return sha256_hex(std::vector<std::uint8_t>(s.begin(), s.end()));
}

// Genesis operator tools (/admin, see its own comment) gate on this, not a
// second login -- same session as everyone else, just a username on a
// list. LIQUIDGOV_OPERATORS is comma-separated exact usernames (no
// wildcards, no roles); unset means the one-person default this site has
// actually always had. Checked fresh on every request, not cached, so
// changing the env var and restarting is the entire revocation story.
bool is_operator(const std::string& username) {
    if (username.empty()) return false;
    const char* env = std::getenv("LIQUIDGOV_OPERATORS");
    std::string list = (env && env[0]) ? env : "Bbottin";
    std::size_t i = 0;
    while (i <= list.size()) {
        auto comma = list.find(',', i);
        std::string name = trim_ws(list.substr(i, comma == std::string::npos ? std::string::npos : comma - i));
        if (!name.empty() && name == username) return true;
        if (comma == std::string::npos) break;
        i = comma + 1;
    }
    return false;
}

// The operator panel's delete confirmations (accounts, communities) ask
// for the word "delete" typed back, not the username/slug -- short enough
// to not be annoying, still enough friction that a stray click can't fire
// it. Case-insensitive so "Delete" doesn't quietly fail.
bool confirm_says_delete(const std::string& s) {
    if (s.size() != 6) return false;
    for (std::size_t i = 0; i < 6; ++i) {
        char c = s[i];
        char lc = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        if (lc != "delete"[i]) return false;
    }
    return true;
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

// Reads one request: `req` gets the request line and headers (ending in
// "\r\n\r\n"), `body` the body, kept apart so a 100 MB upload is held
// once, not copied into a header+body string and again for handle() (a
// 0.5 GB server, review 2026-10-27). max_body_for(headers) is this
// request's body limit (kMaxBody, or a program sender's /mail/big limit,
// see request_body_limit). Over it, *too_large is set and *declared gets
// the Content-Length (0 if unknown) so the caller can answer 413 and drain
// what the client still sends instead of handling a cut-off upload.
bool read_http_request(int fd, std::string& req, std::string& body,
                       const std::function<std::size_t(const std::string&)>& max_body_for,
                       bool* too_large, std::size_t* declared) {
    req.clear();
    body.clear();
    *too_large = false;
    *declared = 0;
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
    const std::size_t body_start = sep + 4;
    std::string headers = req.substr(0, sep);
    const std::size_t max_body = max_body_for(headers);

    std::string expect = header_line_value(headers, "Expect");
    std::string te = header_line_value(headers, "Transfer-Encoding");
    std::string cl = header_line_value(headers, "Content-Length");
    for (char& c : te) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    bool chunked = te.find("chunked") != std::string::npos;

    body.assign(req, body_start, std::string::npos); // what arrived with the headers (a few KB at most)
    req.resize(body_start);

    if (!chunked && !cl.empty()) {
        std::size_t need = 0;
        try { need = static_cast<std::size_t>(std::stoull(cl)); } catch (...) { need = 0; }
        if (need > max_body) { *too_large = true; *declared = need; return false; }
        if (!expect.empty() && body.empty()) {
            std::string low = expect;
            for (char& c : low) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            if (low.find("100-continue") != std::string::npos) {
                const char msg[] = "HTTP/1.1 100 Continue\r\n\r\n";
                send(fd, msg, sizeof(msg) - 1, 0);
            }
        }
        body.reserve(need);
        while (body.size() < need) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
        }
        if (body.size() > need) body.resize(need);
        return true;
    }

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
            if (body.size() > max_body) { *too_large = true; return false; } // chunked: no length to drain to
        }
        body = decode_chunked(body);
        if (body.size() > max_body) { *too_large = true; return false; }
    }
    return true;
}

}  // namespace liquidgov::site
