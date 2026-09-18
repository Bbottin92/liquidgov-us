#include "civic.hpp"
#include "reqjson.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

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
std::optional<std::string> fetch_body(const std::string& path, const std::string& headers_raw) {
    auto cookie = session_cookie_header(headers_raw);
    if (!cookie) return std::nullopt;

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
                                 "Host: 127.0.0.1\r\n"
                                 "Cookie: " + *cookie + "\r\n"
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

} // namespace

bool has_session_cookie(const std::string& headers_raw) {
    return session_cookie_header(headers_raw).has_value();
}

std::optional<std::string> resolve_username(const std::string& headers_raw) {
    auto body = fetch_body("/internal/session-user", headers_raw);
    if (!body) return std::nullopt;
    return commons::reqjson::field_str(*body, "username");
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
