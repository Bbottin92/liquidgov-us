#pragma once
// records/prefs.hpp: the prefs record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
// moved, not rewritten). The LGP1 format of every record type stays in file_store.hpp's header comment.

#include "liquidgov/platform/records/social.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

// A minimum-trust ladder, most to least open: anyone at all, then anyone
// signed in, then friends only, then the owner alone. Each level implies
// every level after it can also see it -- there's no way to express "show
// strangers but hide friends" here. Used for whole-account defaults
// (AccountPrefs' profile_visibility/posts_visibility) and the
// Visibility-tiered /u/<username> gating -- NOT for a post's own audience,
// which needs per-post exceptions/allowlists a flat ladder can't express
// (see PostAudience below).
enum class Visibility : int {
    Public = 0,
    Members = 1,
    Friends = 2,
    OnlyMe = 3,
};

// Who besides the owner may add tracks to an account's jukebox. The owner can
// always curate their own queue (subject only to kJukeboxQueueCap) regardless
// of this setting.
enum class JukeboxPolicy : int {
    OwnerOnly = 0,
    Friends = 1,
    Anyone = 2,
};

// Which slice of activity /feed shows -- see feed_view's comment for what
// each one actually pulls in.
enum class FeedFilter : int {
    All = 0,
    Friends = 1,
    Committees = 2,
    // 3 was Drawings (removed: Brandon 2026-11-10, no Drawings feed category)
};

struct AccountPrefs {
    // Most open by default (Brandon: "all of the settings should be set to
    // the highest amount of visibility by default... I want people to
    // network, not isolate"). profile_visibility/posts_visibility were
    // already Public, the most open point on their own ladder; findability
    // was the one still defaulting to the middle setting. There's no
    // age-based exception here because there's nothing to condition it on
    // yet -- is_minor() is hardcoded false everywhere (see http_server.cpp)
    // until real age assurance exists.
    Findability findability = Findability::Typeahead;
    bool friends_only_dm = true;
    bool group_invite_confirm = true;
    bool presence_hide = false;
    bool notify_friend = true;
    bool notify_message = true;
    bool notify_group = true;
    JukeboxPolicy jukebox_policy = JukeboxPolicy::OwnerOnly;
    Visibility profile_visibility = Visibility::Public;
    Visibility posts_visibility = Visibility::Public;
    // Off by default: the landing intro plays once per browser (see
    // intro_overlay_html), tracked client-side in localStorage. Turning
    // this on makes it play on every visit instead.
    bool intro_every_visit = false;
};

}  // namespace liquidgov::platform
