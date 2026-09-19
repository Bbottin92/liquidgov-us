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
//
// The answer is remembered for 15s per Cookie value (RAM only; see civic.cpp)
// so a bound player's frequent requests don't each cost a loopback call to
// civic. Only a positive answer is cached; failures and "no such session" are
// asked again, and a guest (no lg_session) never reaches civic at all. A
// logged-out session can therefore still resolve for up to 15s.
std::optional<std::string> resolve_username(const std::string& headers_raw);

// True when the raw Cookie header carries lg_session at all -- the same
// gate resolve_username uses, exposed so callers can skip cache bookkeeping
// for anonymous guests without a socket round trip.
bool has_session_cookie(const std::string& headers_raw);

// The `Cookie:` header value itself (only when it carries lg_session), for
// callers that need a stable per-session key -- the whole request head
// changes with unrelated headers and is the wrong thing to key on.
std::optional<std::string> session_cookie(const std::string& headers_raw);

// `open` houses (BUILD open A, GROK_REST_4_ANSWER.md): is there a block, in either
// direction, between the cookie's account and `owner`? nullopt means "no real
// answer" (no session, civic down or slow, a malformed name, an unparseable
// reply) and callers must treat it as BLOCKED: only a real `false` may open a
// door. Real answers (true or false) are cached 15s in RAM keyed on (Cookie
// value, owner); a failure is never cached, never remembered.
std::optional<bool> block_answer(const std::string& headers_raw, const std::string& owner);

// Orphan watch (BUILD orphans log-only, rest slice 4b): does this LiquidGov
// account exist? Gone means civic explicitly answered {"exists":false} for a
// well-formed name. Everything else -- civic down or slow, a malformed name,
// {"exists":null}, an unreadable reply -- is Unknown, and Unknown is NEVER read
// as gone. No cookie, no caching (the caller paces itself).
enum class Existence { Exists, Gone, Unknown };
Existence user_exists(const std::string& username);

// Friends query (glue.md, GROK_V6_ANSWER.md): the mutual, non-blocked
// friends of *that cookie's* account, via civic's same-box
// /internal/friends. Empty on every failure path -- no cookie, civic
// unreachable, unresolved session -- because a caller can't do anything
// different for "no friends" vs "couldn't ask".
std::vector<std::string> resolve_friends(const std::string& headers_raw);

} // namespace commons::civic
