// LiquidGov.US site — C++ HTTP + platform auth.
// Persistence: FileStore (LGP1 snapshot). Not liquidgov-core's ledger.
// The site is split into modules under site/ (see site/sources.cmake); this file is main() only.
#include "site/common.hpp"
#include "site/core/state.hpp"
#include "site/core/adapters_store.hpp"
#include "site/ui/shell.hpp"
#include "site/core/http.hpp"
#include "site/features/mail/mail_attachments.hpp"
#include "site/features/people/people.hpp"
#include "site/features/live/live_sessions.hpp"
#include "site/features/here/here.hpp"
#include "site/router.hpp"
#include "site/core/ws_limits.hpp"
using namespace liquidgov::site;
#ifndef LG_BUILD_COMMIT
#define LG_BUILD_COMMIT "unknown"
#endif
#ifndef LG_BUILD_TREE
#define LG_BUILD_TREE "unknown"
#endif

int main(int argc, char** argv) {
    // --version: which commit and liquidgov-platform tree this program was built from (set by CMake's LG_BUILD_COMMIT /
    // LG_BUILD_TREE; "unknown" for a local build). A prebuilt deploy checks it before installing (deploy/PLAN-prebuilt.md).
    if (argc > 1 && std::string(argv[1]) == "--version") {
        std::cout << "liquidgov_http " << LG_BUILD_COMMIT << " tree " << LG_BUILD_TREE << std::endl;
        return 0;
    }
    // A write to a socket the other side already closed (a /world or jukebox player who just left, a browser
    // that hung up mid-download) or to the password-reset mail pipe after curl quit raises SIGPIPE, whose
    // default action kills this whole process -- every route, every connected user, all in-memory room state.
    // Ignored, that write just fails with EPIPE, which every write here already handles (ws_write_frame returns
    // false, page sends close the socket, the mail path logs pclose's status). Found by a capacity bench of the
    // /world room code: 200 players leaving at once killed it with exit 141 (2026-10-30).
    std::signal(SIGPIPE, SIG_IGN);

    // FilesystemBlobStore::get()/meta() both gate on an in-memory refcount
    // entry that ONLY put() or adopt() ever create -- bytes sitting on
    // disk from a previous process are otherwise invisible, not just
    // unreferenced. Every blob id this server can later re-fetch by id
    // (as opposed to one only ever read back inside the same put() call
    // that created it) has to be re-adopted here on every startup, or it
    // 404s the moment the process restarts. Canvas frames and post
    // attachments were missed when those features were first built --
    // found via a live report of a "missing" image that was actually
    // sitting on disk the whole time, invisible only because of this.
    for (const auto& av : g_store.all_avatars())
        g_blobs.adopt(av.blob_id, "image/png");
    for (const auto& m : g_store.all_mail_with_attachments())
        g_blobs.adopt(*m.attachment_blob_id, m.attachment_mime.value_or("application/octet-stream"));
    for (const auto& m : g_store.all_group_messages_with_attachments())
        g_blobs.adopt(m.attachment_blob_id, m.attachment_mime.empty() ? "application/octet-stream" : m.attachment_mime);
    constexpr std::int64_t kMailRetention = 7 * 24 * 60 * 60;
    purge_expired_deletions(g_store, now(), kMailRetention);
    map_friends_default_on(); // once per person: writes the friends-on default + a note (2026-11-10)
    for (const auto& blob_id : g_store.all_canvas_frame_blob_ids())
        g_blobs.adopt(blob_id, "image/png");
    for (const auto& p : g_store.all_posts_with_attachments())
        g_blobs.adopt(p.attachment_blob_id, p.attachment_mime);
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return 1;
    int yes=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    // LIQUIDGOV_PORT only for a local test copy (8080 is taken on the dev PC); production leaves it unset.
    int port = kPort;
    if (const char* p = std::getenv("LIQUIDGOV_PORT")) { try { port = std::stoi(p); } catch (...) { port = kPort; } }
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(static_cast<std::uint16_t>(port)); addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0){ perror("bind"); return 1; }
    listen(fd,16);
    lglog::emit("boot", "listen");
    std::cerr<<"LiquidGov http://0.0.0.0:"<<port<<" store="<<g_store.path()
             <<" blobs="<<blob_path_from_env()<<" (auth.cpp + FileStore LGP1)\n";
    for(;;){
        sockaddr_in peer{}; socklen_t plen=sizeof(peer);
        int c=accept(fd,reinterpret_cast<sockaddr*>(&peer),&plen); if(c<0) continue;
        timeval tv{}; tv.tv_sec=30; tv.tv_usec=0;
        setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        char ipbuf[INET_ADDRSTRLEN]="0.0.0.0"; inet_ntop(AF_INET,&peer.sin_addr,ipbuf,sizeof(ipbuf));
        std::string req, body;
        bool too_large = false;
        std::size_t declared = 0;
        read_http_request(c, req, body, request_body_limit, &too_large, &declared);
        if (too_large) {
            // Answer instead of handling a cut-off upload, then read and drop what the client is still sending:
            // closing with unread bytes resets the connection, which nginx reports as a 502, not this page
            // (review 2026-10-27). Bounded: the declared length, 2 s per read, 15 s in all.
            std::string resp = http_code("413 Payload Too Large",
                page_wrap("Too large", nav("",0), "<div class=\"card\"><p>That upload is too large to send. "
                          "<a href=\"/mail\">Back to mail</a></p></div>"));
            send(c,resp.data(),resp.size(),0);
            shutdown(c, SHUT_WR);
            timeval drain_tv{}; drain_tv.tv_sec = 2;
            setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&drain_tv,sizeof(drain_tv));
            const auto drain_until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            char sink[65536];
            std::size_t drained = 0;
            while ((declared == 0 || drained < declared) && std::chrono::steady_clock::now() < drain_until) {
                ssize_t n = recv(c, sink, sizeof(sink), 0);
                if (n <= 0) break;
                drained += static_cast<std::size_t>(n);
            }
            close(c);
            continue;
        }

        // The WebSocket routes (/here/ws, /community/ws, a group's jukebox) don't get the ordinary
        // request/response treatment: a successful upgrade hands the fd to
        // its own thread and keeps it open, instead of sending one response
        // and closing. Every other route is untouched — still handled
        // synchronously on this single accept-loop thread, exactly as
        // before this feature existed.
        {
            auto line_end = req.find("\r\n");
            std::string method, rawpath;
            if (line_end != std::string::npos) {
                std::istringstream is(req.substr(0, line_end));
                is >> method >> rawpath;
            }
            // Dots on profile pages (/here/ws?page=...): the feature's own module checks and runs it.
            if (here_ws_upgrade(c, req, method, rawpath, ipbuf)) continue;
            if (method == "GET" && rawpath == "/community/ws" &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) username = acc->username;
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && !key.empty()) {
                    auto slot = ws_take_slot(ws_client_ip(req, ipbuf));
                    if (!slot) { ws_refuse(c, "/community/ws"); continue; }
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread([c, username, st = std::string(""), slot = std::move(slot)] { run_community_session(c, username, st); }).detach();
                        continue;
                    }
                }
                close(c);
                continue;
            }
            // A group's own live jukebox -- same real-time station as
            // /community/ws above, just keyed by this community's slug
            // instead of the "" site-wide one, and gated to members (the
            // site-wide station stays open to anyone logged in; a group's
            // own station is for the group).
            if (method == "GET" && rawpath.rfind("/c/",0)==0 && rawpath.size()>11 &&
                rawpath.compare(rawpath.size()-11,11,"/jukebox/ws")==0 &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                const std::string slug = rawpath.substr(3, rawpath.size()-3-11);
                std::string username;
                bool member = false;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) {
                        username = acc->username;
                        member = valid_username(slug) && g_store.is_member(slug, username);
                    }
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && member && !key.empty()) {
                    auto slot = ws_take_slot(ws_client_ip(req, ipbuf));
                    if (!slot) { ws_refuse(c, "jukebox/ws"); continue; }
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread([c, username, st = slug, slot = std::move(slot)] { run_community_session(c, username, st); }).detach();
                        continue;
                    }
                }
                close(c);
                continue;
            }
        }

        std::string resp;
        try {
            resp = handle(req, ipbuf, std::move(body));
            lglog::access(req, resp);
        } catch (const std::exception& e) {
            lglog::emit("error", e.what());
            std::cerr<<"handle error: "<<e.what()<<"\n";
            resp = http_code("500 Internal Server Error",
                page_wrap("Error", nav("",0), "<div class=\"card\"><p>Could not save. Try again.</p></div>"));
        }
        {
            auto le = req.find("\r\n");
            std::string meth;
            if (le != std::string::npos) { std::istringstream is(req.substr(0, le)); is >> meth; }
            if (meth == "HEAD") {
                auto pos = resp.find("\r\n\r\n");
                if (pos != std::string::npos) resp.resize(pos + 4);
            }
        }
        send(c,resp.data(),resp.size(),0); close(c);
    }
}
