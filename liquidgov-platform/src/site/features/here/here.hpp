#pragma once
// here.hpp: dots on profile pages (Brandon 2026-11-12): who is on a profile page right now, as moving portrait dots.
#include "site/common.hpp"

namespace liquidgov::site {

inline constexpr std::size_t kHereRoomCap = 50;   // dots in one room

// The room key for a page, or "" when the page has no room: only "/u/<existing username>" (profile pages first).
std::string here_room_for(const std::string& page);
// Whether `viewer` may be in `room` (the caller holds g_mu): signed in, chose Yes, can see that profile, and no block
// either way with its owner.
bool here_may_join(const std::string& viewer, const std::string& room);
// GET /here/ws?page=<path> with Upgrade: websocket. Returns true when it took the socket (a session thread now owns
// it, or it was answered and closed); false when the request is not this route. Called from main()'s accept loop.
bool here_ws_upgrade(int fd, const std::string& req, const std::string& method, const std::string& rawpath);
// What a profile page adds for the viewer: the one-time choice card (not chosen yet), the dots script (Yes), or "".
std::string here_page_bits(const std::string& viewer, const std::string& page);
// The Settings card with the Yes/No choice (nothing checked while not chosen).
std::string here_settings_card(const std::string& user);
// The required Yes/No radios on the signup form (nothing pre-selected).
std::string here_signup_fields();

}  // namespace liquidgov::site
