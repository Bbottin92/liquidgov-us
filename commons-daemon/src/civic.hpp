#pragma once
#include <optional>
#include <string>
#include <vector>

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

// True when the raw Cookie header carries lg_session at all -- the same
// gate resolve_username uses, exposed so callers can skip cache bookkeeping
// for anonymous guests without a socket round trip.
bool has_session_cookie(const std::string& headers_raw);

// The `Cookie:` header value itself (only when it carries lg_session), for
// callers that need a stable per-session key -- the whole request head
// changes with unrelated headers and is the wrong thing to key on.
std::optional<std::string> session_cookie(const std::string& headers_raw);

// Friends query (glue.md, GROK_V6_ANSWER.md): the mutual, non-blocked
// friends of *that cookie's* account, via civic's same-box
// /internal/friends. Empty on every failure path -- no cookie, civic
// unreachable, unresolved session -- because a caller can't do anything
// different for "no friends" vs "couldn't ask".
std::vector<std::string> resolve_friends(const std::string& headers_raw);

} // namespace commons::civic
