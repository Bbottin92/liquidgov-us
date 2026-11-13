#include "site/core/state.hpp"

namespace liquidgov::site {

std::mutex g_mu;
std::mutex g_yt_mu;
std::map<std::string, YtCacheEntry> g_yt_cache;
std::mutex g_community_mu;
std::map<std::string, CommunityNowPlaying> g_community_now_by_station;
std::map<std::string, std::deque<CommunityTrack>> g_community_upcoming_by_station;
std::map<std::string, int> g_community_listeners_by_station;
std::map<std::string, std::int64_t> g_community_last_flush_by_station;
std::map<std::string, std::deque<CommunityTrack>> g_community_history_by_station;
// Juketube fix (Brandon 2026-11-08, "do a and b"): one listener's player error or a bare {"ended":true} used to end the
// track for the whole station. Now a playback error is a vote: {"unplayable":"<id>"} per connection, and the track is
// skipped once at least 25% of connected listeners have reported it. Keyed by station: the track it counts for, and how many.
std::map<std::string, std::pair<std::string, int>> g_community_unplayable_by_station;

// Caller must hold g_community_mu.
std::int64_t community_elapsed_locked(const std::string& station, std::int64_t now) {
    const auto& np = g_community_now_by_station[station];
    const auto count = g_community_listeners_by_station[station];
    return np.frozen_offset + (count > 0 ? std::max<std::int64_t>(0, now - np.resumed_at) : 0);
}

// Caller must hold g_community_mu. A fresh track always starts at 0 --
// jpos for it gets written by the caller right after (see
// run_community_session), not here, since this file doesn't have g_store
// declared yet at this point in the translation unit.
void community_advance_locked(const std::string& station, std::int64_t now) {
    auto& upcoming = g_community_upcoming_by_station[station];
    if (upcoming.empty()) {
        g_community_now_by_station[station] = CommunityNowPlaying{};
        return;
    }
    auto next = upcoming.front();
    upcoming.pop_front();
    g_community_now_by_station[station] = CommunityNowPlaying{next.video_id, next.queued_by, 0, now, true};
}

}  // namespace liquidgov::site
