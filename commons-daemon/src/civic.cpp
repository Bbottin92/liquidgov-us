#include "civic.hpp"
#include "reqjson.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <chrono>
#include <unordered_map>

namespace commons::civic {
namespace {

// civic's own kPort (liquidgov-platform/src/http_server.cpp) -- hardcoded
// there too, not env-configurable, so hardcoding the matching value here
// carries no more coupling than the rest of this sibling's nginx configs
// already do.
constexpr int kCivicPort = 8080;

// Only the *value* of the Cookie header, and only when it actually contains
// lg_session -- civic sets lg_theme on every visitor regardless of login,
// so gating on "any Cookie header at all" would fire a resolver (and its
// loopback round trip) for ordinary logged-out guests on every single
// presence call. Matches civic's own cookie_of() substring check, but the
// full header value is still forwarded as-is and civic does its own
// parsing -- this daemon never touches the session id itself.
std::optional<std::string> session_cookie_header(const std::string& headers_raw) {
    auto pos = headers_raw.find("Cookie:");
    if (pos == std::string::npos) pos = headers_raw.find("cookie:");
    if (pos == std::string::npos) return std::nullopt;
    pos += 7;
    while (pos < headers_raw.size() && headers_raw[pos] == ' ') ++pos;
    auto end = headers_raw.find("\r\n", pos);
    std::string value = headers_raw.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    if (value.find("lg_session=") == std::string::npos) return std::nullopt;
    return value;
}

// One same-box GET to civic with the visitor's cookie; returns the response
// body, or nullopt on any transport failure (no cookie, civic down, timeout,
// malformed reply). Every caller treats nullopt as "nothing resolved".
std::optional<std::string> http_get_loopback(const std::string& path, const std::string* cookie) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return std::nullopt;

    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kCivicPort);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) { close(fd); return std::nullopt; }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) { close(fd); return std::nullopt; }

    const std::string request = "GET " + path + " HTTP/1.1\r\n"
                                 "Host: 127.0.0.1\r\n" +
                                 (cookie ? "Cookie: " + *cookie + "\r\n" : std::string()) +
                                 "Connection: close\r\n\r\n";
    if (send(fd, request.data(), request.size(), 0) < 0) { close(fd); return std::nullopt; }

    std::string response;
    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, static_cast<std::size_t>(n));
    }
    close(fd);

    auto sep = response.find("\r\n\r\n");
    if (sep == std::string::npos) return std::nullopt;
    return response.substr(sep + 4);
}

// The cookie-forwarding form every session-aware route uses: no lg_session, no socket.
std::optional<std::string> fetch_body(const std::string& path, const std::string& headers_raw) {
    auto cookie = session_cookie_header(headers_raw);
    if (!cookie) return std::nullopt;
    return http_get_loopback(path, &*cookie);
}

// lg_session -> username, 15s, RAM only (BUILD rest slice 2, GROK_REST_2.md).
// Every bound request used to cost one loopback round trip to civic; this keeps
// the answer for a short TTL so a walking player's ~2 Hz moves don't each ask.
//
// Keyed on the Cookie *value* (the FriendsCache lesson: not the whole request
// head, whose unrelated headers would make one session miss the cache). Only a
// positive answer is remembered: a civic that is down, times out, or says "no
// such session" is never cached, so a logged-out cookie can't be pinned as
// "guest" and a civic hiccup isn't remembered for 15s. Guests (no `lg_session=`)
// never reach civic and never touch this map. Nothing here parses the session
// id, opens a file, or writes to disk.
//
// A revoked or logged-out session can therefore still resolve for at most
// kSessionTtlMs -- the same order of staleness FriendsCache already accepts.
// No mutex: the daemon is single-threaded (http.hpp).
constexpr long long kSessionTtlMs = 15000;
constexpr std::size_t kSessionCap = 256;

struct SessionEntry {
    std::string username;
    long long fetched_ms = 0;
};
std::unordered_map<std::string, SessionEntry>& session_cache() {
    static std::unordered_map<std::string, SessionEntry> cache;
    return cache;
}
long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// (Cookie value, owner) -> blocked?, 15s, RAM only. Same shape as the session
// cache above; only real answers are stored.
struct BlockEntry {
    bool blocked = true;
    long long fetched_ms = 0;
};
std::unordered_map<std::string, BlockEntry>& block_cache() {
    static std::unordered_map<std::string, BlockEntry> cache;
    return cache;
}

// The owner name goes straight into a request line to civic, and it comes from a
// client-supplied region string, so it must be a plain username: nothing that
// could smuggle a CR/LF, a space or a path segment.
bool plain_username(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (unsigned char c : s) {
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}

} // namespace

std::optional<bool> block_answer(const std::string& headers_raw, const std::string& owner) {
    auto cookie = session_cookie_header(headers_raw);
    if (!cookie || !plain_username(owner)) return std::nullopt;

    auto& cache = block_cache();
    const long long now = now_ms();
    const std::string key = *cookie + "\x01" + owner;
    auto it = cache.find(key);
    if (it != cache.end()) {
        if (now - it->second.fetched_ms < kSessionTtlMs) return it->second.blocked;
        cache.erase(it);
    }

    auto body = fetch_body("/internal/blocked/" + owner, headers_raw);
    if (!body) return std::nullopt;
    bool blocked;
    if (body->find("\"blocked\":false") != std::string::npos) blocked = false;
    else if (body->find("\"blocked\":true") != std::string::npos) blocked = true;
    else return std::nullopt; // not the reply we know how to read: no answer, not "open"

    if (cache.size() >= kSessionCap) {
        for (auto e = cache.begin(); e != cache.end();) {
            if (now - e->second.fetched_ms >= kSessionTtlMs) e = cache.erase(e);
            else ++e;
        }
        if (cache.size() >= kSessionCap) cache.clear();
    }
    cache[key] = BlockEntry{blocked, now};
    return blocked;
}

Existence user_exists(const std::string& username) {
    // The name goes into a request line: same plain-username gate as block_answer.
    if (!plain_username(username)) return Existence::Unknown;
    auto body = http_get_loopback("/internal/user-exists/" + username, nullptr);
    if (!body) return Existence::Unknown;
    if (body->find("\"exists\":true") != std::string::npos) return Existence::Exists;
    if (body->find("\"exists\":false") != std::string::npos) return Existence::Gone;
    return Existence::Unknown; // {"exists":null}, an error page, anything unreadable
}

bool has_session_cookie(const std::string& headers_raw) {
    return session_cookie_header(headers_raw).has_value();
}

std::optional<std::string> session_cookie(const std::string& headers_raw) {
    return session_cookie_header(headers_raw);
}

std::optional<std::string> resolve_username(const std::string& headers_raw) {
    auto cookie = session_cookie_header(headers_raw);
    if (!cookie) return std::nullopt; // a guest: no civic call, no cache entry

    auto& cache = session_cache();
    const long long now = now_ms();
    auto it = cache.find(*cookie);
    if (it != cache.end()) {
        if (now - it->second.fetched_ms < kSessionTtlMs) return it->second.username;
        cache.erase(it); // expired: ask civic again, and drop it if civic no longer knows the session
    }

    auto body = fetch_body("/internal/session-user", headers_raw);
    if (!body) return std::nullopt;
    auto username = commons::reqjson::field_str(*body, "username");
    if (!username) return std::nullopt;

    // Room for one more: drop expired entries first, and if the map is still
    // at its cap, clear it -- at Square scale that is a handful of entries and
    // a reset just costs each session one extra civic call.
    if (cache.size() >= kSessionCap) {
        for (auto e = cache.begin(); e != cache.end();) {
            if (now - e->second.fetched_ms >= kSessionTtlMs) e = cache.erase(e);
            else ++e;
        }
        if (cache.size() >= kSessionCap) cache.clear();
    }
    cache[*cookie] = SessionEntry{*username, now};
    return username;
}

std::vector<std::string> resolve_friends(const std::string& headers_raw) {
    std::vector<std::string> out;
    auto body = fetch_body("/internal/friends", headers_raw);
    if (!body) return out;

    // civic emits {"username":"...","friends":["a","b"]} -- usernames are
    // restricted to [A-Za-z0-9._-] (valid_username), so a plain quote-to-
    // quote scan needs no escape handling. Not a JSON parser, same posture
    // as reqjson.hpp.
    auto key = body->find("\"friends\"");
    if (key == std::string::npos) return out;
    auto open = body->find('[', key);
    if (open == std::string::npos) return out;
    auto close_bracket = body->find(']', open);
    if (close_bracket == std::string::npos) return out;

    std::size_t pos = open + 1;
    while (pos < close_bracket) {
        auto q1 = body->find('"', pos);
        if (q1 == std::string::npos || q1 >= close_bracket) break;
        auto q2 = body->find('"', q1 + 1);
        if (q2 == std::string::npos || q2 > close_bracket) break;
        out.push_back(body->substr(q1 + 1, q2 - q1 - 1));
        pos = q2 + 1;
    }
    return out;
}

} // namespace commons::civic
