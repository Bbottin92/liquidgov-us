#pragma once
// accept_ready.hpp: the next connection that has SENT something (2026-11-14, wip/77).
// Every ordinary request is answered on the one accept-loop thread in http_server.cpp. That loop used to accept a
// connection and then wait up to 30 s for its request, so one client that connected and stayed silent held up
// everyone behind it. Browsers do exactly that: Chrome opens a spare "preconnect" socket when a page starts loading,
// and when the page cannot use it (the sandboxed /styles/<id>/preview document is in another network partition) it
// sits there unused while the page's stylesheet waits behind it in the listen queue.
// Silent connections now wait in a small set, watched with poll() next to the listening socket, and only a
// connection with bytes to read is handed to the loop. Not covered: a client that sends PART of a request and then
// stops still holds the loop while read_http_request waits (30 s receive timeout), as before.
#include <netinet/in.h>

namespace liquidgov::site {

constexpr int kSilentConnectionSeconds = 30;  // a connection that sends nothing for this long is closed
constexpr int kSilentConnectionsMax = 64;     // more silent connections than this: the oldest is closed

// Blocks until some accepted connection on listen_fd has data to read; returns its fd and fills peer.
// Connections that close without sending, or stay silent past the limit, are closed here and never returned.
int accept_ready(int listen_fd, sockaddr_in& peer);

}
