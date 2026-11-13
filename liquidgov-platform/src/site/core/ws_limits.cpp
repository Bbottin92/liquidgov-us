// ws_limits.cpp: see ws_limits.hpp.
#include "site/core/ws_limits.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <map>
#include <mutex>

#include "lg_log.hpp"
#include "site/core/http.hpp"

namespace liquidgov::site {
namespace {

int env_cap(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    long n = std::strtol(v, &end, 10);
    return (end && *end == '\0' && n > 0 && n <= 100000) ? static_cast<int>(n) : fallback;
}

struct Limits {
    std::mutex mu;
    int total = 0;
    std::map<std::string, int> per_ip;
    const int max = env_cap("LIQUIDGOV_WS_MAX", kWsMaxDefault);
    const int per_ip_max = env_cap("LIQUIDGOV_WS_PER_IP", kWsPerIpDefault);
};

Limits& limits() {
    static Limits l;
    return l;
}

}  // namespace

WsSlot::WsSlot(std::string ip) : ip_(std::move(ip)) {}

WsSlot::~WsSlot() {
    auto& l = limits();
    std::lock_guard<std::mutex> lock(l.mu);
    --l.total;
    auto it = l.per_ip.find(ip_);
    if (it != l.per_ip.end() && --it->second <= 0) l.per_ip.erase(it);
}

std::unique_ptr<WsSlot> ws_take_slot(const std::string& ip) {
    auto& l = limits();
    std::lock_guard<std::mutex> lock(l.mu);
    if (l.total >= l.max) return nullptr;
    int& mine = l.per_ip[ip];
    if (mine >= l.per_ip_max) return nullptr;
    ++mine;
    ++l.total;
    return std::make_unique<WsSlot>(ip);
}

void ws_refuse(int fd, const char* path) {
    static const std::string no =
        "HTTP/1.1 503 Service Unavailable\r\nRetry-After: 5\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send(fd, no.data(), no.size(), MSG_NOSIGNAL);
    close(fd);
    lglog::emit("ws", std::string("ws cap: refused ") + path);  // no address: /logs is shown to operators
}

std::string ws_client_ip(const std::string& req, const std::string& peer) { return forwarded_ip(req, peer); }

}  // namespace liquidgov::site
