#pragma once
#include <optional>
#include <string>

// Cookie resolver (cookie-bind.md, Persistence/TheCommons): forwards a
// visitor's raw Cookie header to civic's own /internal/session-user route
// over loopback and returns the resolved lg_username, if any. Civic already
// holds the token -> account chain (account_from_req()); this is the one
// piece cookie-bind.md flagged as missing -- "something on civic's side
// still has to receive that forwarded header and answer with an account."
// That civic route is same-box-only (rejects anything that arrives with an
// X-Forwarded-For header, i.e. anything that came through nginx instead of
// a direct loopback call) -- see its own comment in http_server.cpp.
namespace commons::civic {

// nullopt on every failure path: no Cookie header, no lg_session in it,
// civic unreachable, civic times out, or civic doesn't resolve it. Callers
// must treat nullopt as "this is a guest request," never as an error --
// cookie-bind.md is explicit that the two auth paths never mix and that a
// missing/expired/unknown cookie is not a rejection, just a fallback.
std::optional<std::string> resolve_username(const std::string& headers_raw);

} // namespace commons::civic
