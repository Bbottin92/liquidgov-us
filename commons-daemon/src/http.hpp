#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace commons {

struct HttpRequest {
    std::string method;
    std::string path;   // decoded path only, no query string
    std::map<std::string, std::string> query;
    std::string headers_raw; // kept raw so a future cookie-forward can reuse civic's own cookie_of()-style parsing rather than a second implementation
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string status_text = "OK";
    std::string content_type = "application/json";
    std::string body;
};

using Handler = std::function<HttpResponse(const HttpRequest&)>;

// Single-threaded, synchronous accept loop -- deliberately matching
// civic's own posture (liquidgov_http is single-threaded synchronous
// too; see http_server.cpp's own comments on that). This is a tiny
// internal daemon, not a public-facing server: sibling.md has it behind
// nginx on loopback only. No keep-alive, no TLS (nginx's job), no
// threading, no epoll -- accept, read, dispatch, write, close.
class HttpServer {
public:
    HttpServer(std::string bind_addr, int port);
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // path is an exact match unless prefix=true, in which case any path
    // starting with it matches (used for /presence/<region>).
    void route(const std::string& method, const std::string& path, Handler h, bool prefix = false);

    void run(); // blocks forever (or until the process is killed)

private:
    struct Route { std::string method, path; Handler handler; bool prefix; };
    std::string bind_addr_;
    int port_;
    int listen_fd_ = -1;
    std::vector<Route> routes_;

    void serve_one(int client_fd);
    HttpResponse dispatch(const HttpRequest& req);
};

} // namespace commons
