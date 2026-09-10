#include "liquidgov/platform/websocket.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace liquidgov::platform {

namespace {

// Minimal SHA-1 (RFC 3174). WebSocket's handshake mandates SHA-1 by spec —
// this is not swappable for the project's SHA-256 (liquidgov_hash is a
// different algorithm for a different job, the ledger's content hash).
struct Sha1 {
    std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    static std::uint32_t rol(std::uint32_t v, int bits) {
        return (v << bits) | (v >> (32 - bits));
    }

    std::array<std::uint8_t, 20> digest(const std::string& input) {
        std::string msg = input;
        const std::uint64_t bit_len = static_cast<std::uint64_t>(msg.size()) * 8;
        msg += static_cast<char>(0x80);
        while (msg.size() % 64 != 56) msg += static_cast<char>(0x00);
        for (int i = 7; i >= 0; --i)
            msg += static_cast<char>((bit_len >> (i * 8)) & 0xff);

        for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
            std::uint32_t w[80];
            for (int i = 0; i < 16; ++i) {
                w[i] = (static_cast<std::uint8_t>(msg[chunk + i * 4]) << 24) |
                       (static_cast<std::uint8_t>(msg[chunk + i * 4 + 1]) << 16) |
                       (static_cast<std::uint8_t>(msg[chunk + i * 4 + 2]) << 8) |
                       (static_cast<std::uint8_t>(msg[chunk + i * 4 + 3]));
            }
            for (int i = 16; i < 80; ++i)
                w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

            std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
            for (int i = 0; i < 80; ++i) {
                std::uint32_t f, k;
                if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
                else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
                std::uint32_t temp = rol(a, 5) + f + e + k + w[i];
                e = d; d = c; c = rol(b, 30); b = a; a = temp;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
        }

        std::array<std::uint8_t, 20> out{};
        for (int i = 0; i < 5; ++i) {
            out[i * 4 + 0] = static_cast<std::uint8_t>((h[i] >> 24) & 0xff);
            out[i * 4 + 1] = static_cast<std::uint8_t>((h[i] >> 16) & 0xff);
            out[i * 4 + 2] = static_cast<std::uint8_t>((h[i] >> 8) & 0xff);
            out[i * 4 + 3] = static_cast<std::uint8_t>(h[i] & 0xff);
        }
        return out;
    }
};

std::string base64_encode(const std::uint8_t* data, std::size_t len) {
    static const char* kTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= len) {
        std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kTable[(n >> 18) & 0x3f];
        out += kTable[(n >> 12) & 0x3f];
        out += kTable[(n >> 6) & 0x3f];
        out += kTable[n & 0x3f];
        i += 3;
    }
    const std::size_t rem = len - i;
    if (rem == 1) {
        std::uint32_t n = data[i] << 16;
        out += kTable[(n >> 18) & 0x3f];
        out += kTable[(n >> 12) & 0x3f];
        out += "==";
    } else if (rem == 2) {
        std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out += kTable[(n >> 18) & 0x3f];
        out += kTable[(n >> 12) & 0x3f];
        out += kTable[(n >> 6) & 0x3f];
        out += "=";
    }
    return out;
}

bool recv_exact(int fd, void* buf, std::size_t len) {
    auto* p = static_cast<std::uint8_t*>(buf);
    std::size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n < 0) return false; // includes EAGAIN/EWOULDBLOCK timeout — caller distinguishes via first-read path
        if (n == 0) return false;
        got += static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace

std::string websocket_accept_value(const std::string& client_key) {
    static const std::string kMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    Sha1 sha;
    auto digest = sha.digest(client_key + kMagic);
    return base64_encode(digest.data(), digest.size());
}

WsReadResult ws_read_frame(int fd) {
    std::uint8_t header[2];
    ssize_t n = recv(fd, header, 2, 0);
    if (n == 0) return {WsReadStatus::Closed, {}};
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return {WsReadStatus::Timeout, {}};
        return {WsReadStatus::Closed, {}};
    }
    if (n < 2) return {WsReadStatus::Closed, {}};

    const bool fin = (header[0] & 0x80) != 0;
    const auto opcode = static_cast<WsOpcode>(header[0] & 0x0f);
    const bool masked = (header[1] & 0x80) != 0;
    std::uint64_t len = header[1] & 0x7f;

    if (!fin) return {WsReadStatus::Closed, {}}; // fragmented messages unsupported, treat as a hard stop

    if (len == 126) {
        std::uint8_t ext[2];
        if (!recv_exact(fd, ext, 2)) return {WsReadStatus::Closed, {}};
        len = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
        std::uint8_t ext[8];
        if (!recv_exact(fd, ext, 8)) return {WsReadStatus::Closed, {}};
        len = 0;
        for (int i = 0; i < 8; ++i) len = (len << 8) | ext[i];
    }
    if (len > (1u << 20)) return {WsReadStatus::Closed, {}}; // 1 MiB cap — this is small JSON control traffic

    std::uint8_t mask_key[4] = {0, 0, 0, 0};
    if (masked) {
        if (!recv_exact(fd, mask_key, 4)) return {WsReadStatus::Closed, {}};
    }

    std::string payload(static_cast<std::size_t>(len), '\0');
    if (len > 0 && !recv_exact(fd, payload.data(), static_cast<std::size_t>(len)))
        return {WsReadStatus::Closed, {}};

    if (masked) {
        for (std::size_t i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<char>(static_cast<std::uint8_t>(payload[i]) ^ mask_key[i % 4]);
    }

    return {WsReadStatus::Ok, WsFrame{opcode, std::move(payload)}};
}

bool ws_write_frame(int fd, WsOpcode opcode, const std::string& payload) {
    std::string out;
    out += static_cast<char>(0x80 | static_cast<std::uint8_t>(opcode)); // FIN=1

    const std::size_t len = payload.size();
    if (len <= 125) {
        out += static_cast<char>(len);
    } else if (len <= 0xffff) {
        out += static_cast<char>(126);
        out += static_cast<char>((len >> 8) & 0xff);
        out += static_cast<char>(len & 0xff);
    } else {
        out += static_cast<char>(127);
        for (int i = 7; i >= 0; --i)
            out += static_cast<char>((static_cast<std::uint64_t>(len) >> (i * 8)) & 0xff);
    }
    out += payload;

    std::size_t sent = 0;
    while (sent < out.size()) {
        ssize_t n = send(fd, out.data() + sent, out.size() - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace liquidgov::platform
