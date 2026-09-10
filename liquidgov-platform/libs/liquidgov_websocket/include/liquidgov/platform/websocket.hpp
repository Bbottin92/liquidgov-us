#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace liquidgov::platform {

// RFC 6455 handshake: Sec-WebSocket-Accept from the client's Sec-WebSocket-Key.
std::string websocket_accept_value(const std::string& client_key);

enum class WsOpcode : std::uint8_t {
    Continuation = 0,
    Text = 1,
    Binary = 2,
    Close = 8,
    Ping = 9,
    Pong = 10,
};

struct WsFrame {
    WsOpcode opcode = WsOpcode::Text;
    std::string payload;
};

enum class WsReadStatus { Ok, Timeout, Closed };

struct WsReadResult {
    WsReadStatus status = WsReadStatus::Closed;
    WsFrame frame;
};

// Reads one WebSocket frame from a blocking socket fd (SO_RCVTIMEO governs
// how long this waits before returning Timeout). Client->server frames are
// always masked per RFC 6455 and this unmasks them. Single-frame messages
// only (FIN=1) — enough for small JSON control messages; a fragmented
// message is treated as Closed rather than reassembled.
WsReadResult ws_read_frame(int fd);

// Writes one unmasked (server->client) WebSocket frame. Returns false on
// write failure (caller should treat the connection as dead).
bool ws_write_frame(int fd, WsOpcode opcode, const std::string& payload);

} // namespace liquidgov::platform
