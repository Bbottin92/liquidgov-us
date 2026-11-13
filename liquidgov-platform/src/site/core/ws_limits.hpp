#pragma once
// ws_limits.hpp: caps on open WebSocket connections (Brandon 2026-11-13). Every live socket (/community/ws,
// /here/ws, a group's jukebox) holds a thread and a file descriptor for as long as it is open, so each upgrade takes
// a slot first: at most LIQUIDGOV_WS_PER_IP per client address and LIQUIDGOV_WS_MAX in all (defaults below, read once
// at startup). A new live feature calls ws_take_slot before its 101 and moves the slot into its session thread.
#include <memory>
#include <string>

namespace liquidgov::site {

constexpr int kWsMaxDefault = 200;
constexpr int kWsPerIpDefault = 20;

// One taken slot; destroying it gives the slot back (so it lives exactly as long as the session that owns it).
class WsSlot {
public:
    explicit WsSlot(std::string ip);
    ~WsSlot();
    WsSlot(const WsSlot&) = delete;
    WsSlot& operator=(const WsSlot&) = delete;
private:
    std::string ip_;
};

// A slot for a client at <ip>, or null when that address or the whole server is at its cap.
std::unique_ptr<WsSlot> ws_take_slot(const std::string& ip);

// Answers 503 (Retry-After) on fd, logs the refusal (the path, not the address) and closes fd.
void ws_refuse(int fd, const char* path);

// The address a WebSocket counts against: the last X-Forwarded-For entry (the one nginx appends) when the peer is
// loopback (nginx), else the peer itself. Same rule as forwarded_ip.
std::string ws_client_ip(const std::string& req, const std::string& peer);

}  // namespace liquidgov::site
