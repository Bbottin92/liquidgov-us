#include "http.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace commons {

namespace {

std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = std::isxdigit(static_cast<unsigned char>(s[i + 1])) ? std::stoi(s.substr(i + 1, 1), nullptr, 16) : -1;
            int lo = std::isxdigit(static_cast<unsigned char>(s[i + 2])) ? std::stoi(s.substr(i + 2, 1), nullptr, 16) : -1;
            if (hi >= 0 && lo >= 0) { out += static_cast<char>(hi * 16 + lo); i += 2; continue; }
        }
        out += (s[i] == '+') ? ' ' : s[i];
    }
    return out;
}

std::map<std::string, std::string> parse_query(const std::string& qs) {
    std::map<std::string, std::string> m;
    std::size_t start = 0;
    while (start <= qs.size()) {
        auto amp = qs.find('&', start);
        std::string part = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        auto eq = part.find('=');
        if (!part.empty()) {
            if (eq == std::string::npos) m[url_decode(part)] = "";
            else m[url_decode(part.substr(0, eq))] = url_decode(part.substr(eq + 1));
        }
        if (amp == std::string::npos) break;
        start = amp + 1;
    }
    return m;
}

long content_length_of(const std::string& headers) {
    // Case-insensitive-ish search: only the exact casing civic itself
    // emits/expects matters here, this is our own daemon's own header
    // reading, not parsing an adversarial input in the security sense --
    // still bounded and defensive against a missing/garbage value.
    auto pos = headers.find("Content-Length:");
    if (pos == std::string::npos) pos = headers.find("content-length:");
    if (pos == std::string::npos) return 0;
    pos = headers.find(':', pos) + 1;
    auto end = headers.find("\r\n", pos);
    std::string v = headers.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    try { return std::stol(v); } catch (...) { return 0; }
}

} // namespace

HttpServer::HttpServer(std::string bind_addr, int port) : bind_addr_(std::move(bind_addr)), port_(port) {}

HttpServer::~HttpServer() {
    if (listen_fd_ >= 0) close(listen_fd_);
}

void HttpServer::route(const std::string& method, const std::string& path, Handler h, bool prefix) {
    routes_.push_back({method, path, std::move(h), prefix});
}

HttpResponse HttpServer::dispatch(const HttpRequest& req) {
    for (const auto& r : routes_) {
        if (r.method != req.method) continue;
        bool matches = r.prefix ? (req.path.rfind(r.path, 0) == 0) : (req.path == r.path);
        if (matches) {
            try {
                return r.handler(req);
            } catch (const std::exception& e) {
                HttpResponse resp;
                resp.status = 500; resp.status_text = "Internal Server Error";
                resp.body = std::string("{\"error\":\"") + e.what() + "\"}";
                return resp;
            }
        }
    }
    HttpResponse resp;
    resp.status = 404; resp.status_text = "Not Found";
    resp.body = "{\"error\":\"not found\"}";
    return resp;
}

void HttpServer::serve_one(int client_fd) {
    std::string raw;
    char buf[4096];
    std::size_t header_end = std::string::npos;
    // Read until the header terminator, same shape as civic's own
    // req.find("\r\n\r\n") split (see http_server.cpp).
    while (header_end == std::string::npos) {
        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0) { close(client_fd); return; }
        raw.append(buf, static_cast<std::size_t>(n));
        header_end = raw.find("\r\n\r\n");
        if (raw.size() > 1 << 20) { close(client_fd); return; } // 1MB header guard
    }
    std::string head = raw.substr(0, header_end);
    std::string body = raw.substr(header_end + 4);
    long need = content_length_of(head);
    while (static_cast<long>(body.size()) < need) {
        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        body.append(buf, static_cast<std::size_t>(n));
    }

    HttpRequest req;
    req.headers_raw = head;
    std::istringstream hs(head);
    std::string request_line;
    std::getline(hs, request_line);
    std::istringstream rl(request_line);
    std::string full_path;
    rl >> req.method >> full_path;
    auto qpos = full_path.find('?');
    if (qpos == std::string::npos) {
        req.path = full_path;
    } else {
        req.path = full_path.substr(0, qpos);
        req.query = parse_query(full_path.substr(qpos + 1));
    }
    req.body = body;

    HttpResponse resp = dispatch(req);
    std::ostringstream out;
    out << "HTTP/1.1 " << resp.status << " " << resp.status_text << "\r\n"
        << "Content-Type: " << resp.content_type << "\r\n"
        << "Content-Length: " << resp.body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << resp.body;
    std::string out_str = out.str();
    send(client_fd, out_str.data(), out_str.size(), 0);
    close(client_fd);
}

void HttpServer::run() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("HttpServer: socket() failed");

    int yes = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (inet_pton(AF_INET, bind_addr_.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("HttpServer: invalid bind address '" + bind_addr_ + "'");
    }
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::runtime_error("HttpServer: bind() failed on " + bind_addr_ + ":" + std::to_string(port_));
    }
    if (listen(listen_fd_, 16) != 0) {
        throw std::runtime_error("HttpServer: listen() failed");
    }

    std::cerr << "commons-daemon listening on " << bind_addr_ << ":" << port_ << "\n";
    for (;;) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (client_fd < 0) continue; // e.g. EINTR; keep serving, don't die on one bad accept
        serve_one(client_fd);
    }
}

} // namespace commons
