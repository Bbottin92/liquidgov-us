namespace {
constexpr int kPort = 8080;
// Mail attachments run through file_transfer's default caps (image 10 MiB,
// document/PDF/OOXML 25 MiB — see SizeCaps in file_transfer.hpp). The read
// loop's ceiling has to cover the largest of those plus multipart overhead.
constexpr std::size_t kMaxBody = 25ull * 1024 * 1024 + 64 * 1024;
// Room for the multipart wrapping and form fields around a program upload on /mail/big.
constexpr std::size_t kMultipartRoom = 1024 * 1024;
std::mutex g_mu;

// --- World: one shared room, connected users see each other move. ---
// Deliberately separate from g_mu / g_store: this is ephemeral in-memory
// position state, never persisted, touched only by world session threads
// (one per connected /world/ws client) — the rest of the server stays on
// its existing single-threaded accept loop, untouched by this feature.
struct WorldPlayer {
    double x = 128, y = 128;
    std::string username;
    std::string color;
};
std::mutex g_world_mu;
std::unordered_map<int, WorldPlayer> g_world_players; // keyed by client socket fd

std::string world_color_for(const std::string& username) {
    static const char* kPalette[] = {"#a23b3b", "#3b6ea2", "#3ba25e", "#a2823b",
                                      "#6e3ba2", "#3ba297", "#a23b8f", "#5f7a2e"};
    std::size_t h = std::hash<std::string>{}(username);
    return kPalette[h % 8];
}

// --- Live jukebox "stations": one shared, synced-elapsed-position queue
// per station that everyone connected to that station's socket hears
// "live" (new joiners seek to the current track's elapsed position).
// Stations are keyed by slug -- "" is the one original site-wide station
// (still /community/ws, unchanged for existing callers), any other slug
// is that community's own jukebox (/c/<slug>/jukebox/ws), gated to
// members of that community since "everyone in the group can hear" means
// the group, not the whole site the way the site-wide one is. One mutex,
// one pair of maps, same one-thread-per-connection pattern as /world/ws.
struct CommunityTrack {
    std::string video_id;
    std::string queued_by;
};
// A shared freeze, not a radio that keeps running in an empty room: while
// at least one listener is connected there's one playhead, ticking off
// resumed_at; the moment the last one disconnects it's frozen at whatever
// elapsed had reached (frozen_offset), and stays there -- no wall-clock
// addition -- until someone shows up again. See community_elapsed_locked,
// the one formula every reader shares so they can't drift apart from each
// other. Durable copy lives in FileStore under the jpos tag (keyed the
// same as station: "" for site-wide, "c:<slug>" for a community, "u:
// <username>" for a personal jukebox), written on last-listener-leave and
// roughly every 15s while playing, so a process restart resumes from
// there too instead of jumping back to 0:00.
struct CommunityNowPlaying {
    std::string video_id;
    std::string queued_by;
    std::int64_t frozen_offset = 0; // seconds elapsed before the current listening session (all of it, if nobody's connected)
    std::int64_t resumed_at = 0;    // wall-clock unix when listener_count last went 0->1; meaningless while listener_count==0
    bool active = false;
};
struct YtCacheEntry { std::string body; std::int64_t exp = 0; };
std::mutex g_yt_mu;
std::map<std::string, YtCacheEntry> g_yt_cache;
std::mutex g_community_mu;
std::map<std::string, CommunityNowPlaying> g_community_now_by_station;
std::map<std::string, std::deque<CommunityTrack>> g_community_upcoming_by_station;
std::map<std::string, int> g_community_listeners_by_station;
std::map<std::string, std::int64_t> g_community_last_flush_by_station;
// Recently-played, per station (bug: the History section never showed a
// shared station's actual history -- it fell back to the viewer's OWN
// personal plays instead, since nothing ever recorded a shared station's
// plays anywhere and the one route that served history only understood a
// personal owner). In-memory only, capped (kCommunityHistoryCap), same
// posture as every other g_community_* map here: real history for a
// personal jukebox is durable (FileStore), a shared station's "now
// playing" position is durable too (jpos), but a shared station's past
// tracklist resetting across a restart is an honest, low-risk trade-off
// for not touching the LGP file format for this.
constexpr std::size_t kCommunityHistoryCap = 15;
// A saved spot past this is not resumed. The client used to restart it at
// 0:00, so the track never reached its end and every later click was queued
// behind it. Three hours matches the old client cap.
constexpr std::int64_t kJukeboxResumeMaxSec = 3 * 3600;
std::map<std::string, std::deque<CommunityTrack>> g_community_history_by_station;
// Juketube fix (Brandon 2026-11-08, "do a and b"): one listener's player error or a bare {"ended":true} used to end the
// track for the whole station. Now a playback error is a vote: {"unplayable":"<id>"} per connection, and the track is
// skipped once at least 25% of connected listeners have reported it. Keyed by station: the track it counts for, and how many.
std::map<std::string, std::pair<std::string, int>> g_community_unplayable_by_station;
// A named "ended" from someone who is not alone counts only after this many seconds of the track, so a crafted message
// can't skip a song the room just started.
constexpr std::int64_t kCommunityMinEndedSec = 20;

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

