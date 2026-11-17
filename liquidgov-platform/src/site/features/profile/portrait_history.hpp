#pragma once
// portrait_history.hpp: earlier self-portraits (Brandon 2026-11-16: "save and load prior self portraits"). The last
// 10 are kept per person; the owner picks one on /me/portrait to load it back into the canvas; they are private to
// the owner unless the owner turns on "show on my profile" (avhs). Store records: avh, avhs (file_store.hpp).
#include "site/common.hpp"
#include "site/core/route.hpp"

namespace liquidgov::site {

// /me/portrait/old/<n> (owner's picture), /portrait-history/<user>/<n> (when the owner made them public),
// POST /me/portrait/old/delete and /me/portrait/old/share. nullopt when the request is not one of these.
std::optional<std::string> portrait_history_routes(Route& r);

// Saves a new self-portrait exactly as save_avatar does, but the portrait it replaces moves into the person's
// history instead of being released (the oldest beyond 10 are released). Same result as save_avatar.
SaveAvatarResult save_portrait_keeping_history(AccountId owner, const std::vector<std::uint8_t>& bytes);

// The owner's "Earlier portraits" card on /me/portrait: current + history thumbnails (each a button the canvas
// script loads), delete buttons and the show-on-profile choice. Empty when there is nothing to show.
std::string portrait_history_owner_html(const std::string& username);

// The strip on a public profile: only when the owner chose to show them and there are some. Empty otherwise.
std::string portrait_history_public_html(const std::string& username);

}  // namespace liquidgov::site
