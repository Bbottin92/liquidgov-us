#pragma once
// route.hpp: what handle() hands each feature's routes: the parsed request and its helpers.
// handle() (site/router.cpp) parses the request once, then asks each <feature>_routes(Route&) in a fixed order;
// the first one that answers wins. LG_ROUTE_LOCALS(r) gives a route block the same local names it had inside
// handle(), so the blocks were moved without changing a character.
#include "site/common.hpp"

namespace liquidgov::site {

struct Route {
    const std::string& req;
    const std::string& peer_ip;
    std::string& body;
    std::string& method;
    std::string& rawpath;
    std::string& path;
    std::map<std::string, std::string>& qs;
    const std::string& ip;
    std::optional<PlatformAccount>& acc;
    std::string& user;
    const std::string& sid;
    const int& unread;
    std::function<std::string(const std::string&, const std::string&)> pg;
    std::function<std::string()> need_login;
    std::function<std::string()> admin_404;
};

#define LG_ROUTE_LOCALS(r) \
    auto& req = (r).req; (void)req; \
    auto& peer_ip = (r).peer_ip; (void)peer_ip; \
    auto& body = (r).body; (void)body; \
    auto& method = (r).method; (void)method; \
    auto& rawpath = (r).rawpath; (void)rawpath; \
    auto& path = (r).path; (void)path; \
    auto& qs = (r).qs; (void)qs; \
    auto& ip = (r).ip; (void)ip; \
    auto& acc = (r).acc; (void)acc; \
    auto& user = (r).user; (void)user; \
    auto& sid = (r).sid; (void)sid; \
    auto& unread = (r).unread; (void)unread; \
    auto& pg = (r).pg; (void)pg; \
    auto& need_login = (r).need_login; (void)need_login; \
    auto& admin_404 = (r).admin_404; (void)admin_404; \

}  // namespace liquidgov::site
