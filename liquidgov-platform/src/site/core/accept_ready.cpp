// accept_ready.cpp: see accept_ready.hpp.
#include "site/core/accept_ready.hpp"

#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace liquidgov::site {

namespace {
struct Silent {
    int fd;
    sockaddr_in peer;
    std::chrono::steady_clock::time_point since;
};
}

int accept_ready(int listen_fd, sockaddr_in& peer) {
    // Only the accept-loop thread calls this, so the waiting set needs no lock.
    static std::vector<Silent> silent; // oldest first
    static bool listen_nonblocking = false;
    if (!listen_nonblocking) {
        // poll() can report a connection that is gone again by the time accept() runs; never block there.
        int fl = fcntl(listen_fd, F_GETFL, 0);
        if (fl >= 0) fcntl(listen_fd, F_SETFL, fl | O_NONBLOCK);
        listen_nonblocking = true;
    }
    const auto limit = std::chrono::seconds(kSilentConnectionSeconds);
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        while (!silent.empty() && (now - silent.front().since >= limit ||
                                   silent.size() > static_cast<std::size_t>(kSilentConnectionsMax))) {
            close(silent.front().fd);
            silent.erase(silent.begin());
        }
        std::vector<pollfd> fds(silent.size() + 1);
        fds[0].fd = listen_fd; fds[0].events = POLLIN; fds[0].revents = 0;
        for (std::size_t i = 0; i < silent.size(); ++i) {
            fds[i + 1].fd = silent[i].fd; fds[i + 1].events = POLLIN; fds[i + 1].revents = 0;
        }
        int wait_ms = -1; // nothing waiting: sleep until someone connects
        if (!silent.empty()) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(silent.front().since + limit - now).count();
            wait_ms = left < 1 ? 1 : static_cast<int>(left);
        }
        if (poll(fds.data(), fds.size(), wait_ms) < 0) continue; // EINTR: look again

        // a waiting connection that now has bytes (or has gone away) comes before a new one: it arrived first
        for (std::size_t i = 0; i < silent.size(); ++i) {
            if (!(fds[i + 1].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) continue;
            const Silent s = silent[i];
            silent.erase(silent.begin() + static_cast<std::ptrdiff_t>(i));
            char probe;
            if (recv(s.fd, &probe, 1, MSG_PEEK | MSG_DONTWAIT) > 0) { // bytes are waiting; MSG_PEEK leaves them there
                peer = s.peer;
                return s.fd;
            }
            close(s.fd); // closed or failed without sending a request
            break;       // the indexes moved: poll again
        }
        if (fds[0].revents & POLLIN) {
            Silent s{};
            socklen_t plen = sizeof(s.peer);
            s.fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&s.peer), &plen);
            if (s.fd < 0) continue;
            s.since = std::chrono::steady_clock::now();
            silent.push_back(s);
        }
    }
}

}
