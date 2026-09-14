#pragma once
// Tiny process logger. stderr -> systemd journal. Last 200 lines in RAM for GET /logs.
// No files, no threads, no deps. C++17.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <mutex>
#include <sstream>
#include <string>

namespace lglog {

inline std::mutex mu;
inline std::deque<std::string> ring;
constexpr std::size_t kRing = 200;

inline void emit(const char* cat, const std::string& msg) {
    using clock = std::chrono::system_clock;
    const auto t = clock::to_time_t(clock::now());
    std::tm tm{};
    gmtime_r(&t, &tm);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm);
    std::string line = std::string(ts) + " [" + cat + "] " + msg;
    {
        std::lock_guard<std::mutex> g(mu);
        ring.push_back(line);
        while (ring.size() > kRing) ring.pop_front();
    }
    std::fprintf(stderr, "%s\n", line.c_str());
}

inline bool skip_path(const std::string& method, const std::string& path) {
    if (method != "GET" && method != "HEAD") return false;
    if (path.rfind("/assets/", 0) == 0) return true;
    if (path == "/favicon.ico" || path == "/favicon.svg" || path == "/robots.txt") return true;
    if (path == "/jukebox/meta" || path == "/manifest.webmanifest") return true;
    if (path == "/icon" || path == "/apple-icon") return true;
    if (path.size() > 12 && path.compare(path.size() - 12, 12, "/jukebox/now") == 0) return true;
    auto dot = path.rfind('.');
    if (dot != std::string::npos && dot + 1 < path.size()) {
        const char* ext = path.c_str() + dot + 1;
        if (!std::strcmp(ext, "js") || !std::strcmp(ext, "css") || !std::strcmp(ext, "mp3") ||
            !std::strcmp(ext, "mp4") || !std::strcmp(ext, "png") || !std::strcmp(ext, "jpg") ||
            !std::strcmp(ext, "svg") || !std::strcmp(ext, "ico") || !std::strcmp(ext, "webp"))
            return true;
    }
    return false;
}

inline int status_of(const std::string& resp) {
    auto sp = resp.find(' ');
    if (sp == std::string::npos || sp + 1 >= resp.size()) return 0;
    return std::atoi(resp.c_str() + sp + 1);
}

inline std::string sanitize_qs(std::string q) {
    if (q.find("key=") != std::string::npos) return "(redacted)";
    if (q.size() > 80) q.resize(80);
    for (char& c : q) if (c < 32) c = ' ';
    return q;
}

inline void access(const std::string& req, const std::string& resp) {
    auto le = req.find("\r\n");
    if (le == std::string::npos) le = req.find('\n');
    if (le == std::string::npos) return;
    std::string method, raw;
    {
        std::istringstream is(req.substr(0, le));
        is >> method >> raw;
    }
    if (method.empty()) return;
    std::string path = raw;
    std::string qs;
    auto qpos = raw.find('?');
    if (qpos != std::string::npos) {
        path = raw.substr(0, qpos);
        qs = sanitize_qs(raw.substr(qpos + 1));
    }
    if (skip_path(method, path)) return;
    const int st = status_of(resp);
    std::string line = method + " " + path + " " + std::to_string(st);
    if (!qs.empty()) line += " ?" + qs;
    emit("http", line);
}

inline std::string dump_text() {
    std::lock_guard<std::mutex> g(mu);
    std::string o;
    o.reserve(ring.size() * 96);
    for (const auto& l : ring) {
        o += l;
        o += '\n';
    }
    return o;
}

inline std::string clip_client(std::string s) {
    if (s.size() > 400) s.resize(400);
    for (char& c : s) if (c == '\n' || c == '\r') c = ' ';
    return s;
}

} // namespace lglog
