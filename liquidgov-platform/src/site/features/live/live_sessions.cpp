#include "site/features/live/live_sessions.hpp"
#include "site/core/state.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"

namespace liquidgov::site {
// Minimal ad-hoc extractor for a top-level {"key":"value"} string field —
// same "only shape this endpoint ever receives" reasoning as the world
// socket's x/y number parser. Not escape-aware: fine for URLs, which never
// contain a literal quote character.
std::optional<std::string> extract_json_string_field(const std::string& s, const char* key) {
    auto p = s.find(key);
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    auto open = s.find('"', p);
    if (open == std::string::npos) return std::nullopt;
    auto close_pos = s.find('"', open + 1);
    if (close_pos == std::string::npos) return std::nullopt;
    return s.substr(open + 1, close_pos - open - 1);
}

// One thread per connected station client (/community/ws for the "" site-
// wide station, /c/<slug>/jukebox/ws for a community's own) — same shape
// as the other socket sessions (/here/ws), entirely separate shared state (g_community_mu).
// Membership (for a real station) was already checked before this thread
// was spawned, same as everything else here trusts the caller. Any
// connected member can add a track (open queue within the station, same
// posture the site-wide one always had); the queue auto-advances when any
// connected client's player reports the current track ended.
void run_community_session(int fd, const std::string& username, const std::string& station) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 500000; // 500ms — a shared queue doesn't need world's 100ms cadence
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Joining is a 0->1 transition on this station's listener count: if
    // nobody else is here either, resume from wherever the freeze left
    // off (durable jpos, so this survives a process restart too, not just
    // a run of the same process with everyone gone and back). If someone
    // else is already listening (count already >0), there's nothing to
    // do here -- community_elapsed_locked already gives this new
    // connection the same live position everyone else has.
    {
        std::lock_guard<std::mutex> lock(g_community_mu);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int& count = g_community_listeners_by_station[station];
        const bool was_zero = (count == 0);
        ++count;
        if (was_zero) {
            if (auto pos = g_store.jukebox_pos(station)) {
                if (pos->offset_sec > kJukeboxResumeMaxSec) {
                    g_store.clear_jukebox_pos(station);
                } else {
                    auto& np = g_community_now_by_station[station];
                    np.video_id = pos->video_id;
                    np.frozen_offset = pos->offset_sec;
                    np.resumed_at = now;
                    np.active = true;
                }
            } else if (g_community_now_by_station[station].active) {
                g_community_now_by_station[station].resumed_at = now;
            }
        }
    }

    std::string reported_unplayable; // this connection's one vote per track (see g_community_unplayable_by_station)
    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                const std::string& s = result.frame.payload;
                if (auto raw = extract_json_string_field(s, "\"add\"")) {
                    if (auto id = extract_youtube_id(trim_ws(*raw))) {
                        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& now_playing = g_community_now_by_station[station];
                        auto& upcoming = g_community_upcoming_by_station[station];
                        if (!now_playing.active && upcoming.empty()) {
                            now_playing = CommunityNowPlaying{*id, username, 0, now, true};
                            g_store.set_jukebox_pos(station, *id, 0, now);
                            g_community_last_flush_by_station[station] = now;
                        } else {
                            upcoming.push_back(CommunityTrack{*id, username});
                        }
                    }
                } else if (auto raw = extract_json_string_field(s, "\"play\"")) {
                    // A click starts the video when this listener is the only one
                    // connected, or when the station is idle. With someone else
                    // listening, it queues, so one person cannot skip the room.
                    if (auto id = extract_youtube_id(trim_ws(*raw))) {
                        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& now_playing = g_community_now_by_station[station];
                        auto& upcoming = g_community_upcoming_by_station[station];
                        const int listeners = g_community_listeners_by_station[station];
                        const bool alone = listeners <= 1;
                        if (alone || (!now_playing.active && upcoming.empty())) {
                            if (now_playing.active && now_playing.video_id != *id) {
                                auto& hist = g_community_history_by_station[station];
                                hist.push_back(CommunityTrack{now_playing.video_id, now_playing.queued_by});
                                if (hist.size() > kCommunityHistoryCap) hist.pop_front();
                                // saved too, so a restart (every deploy) no longer wipes it; tests/e2e/juketube_history.sh
                                g_store.record_jukebox_play(kStationHistoryPrefix + station, now_playing.video_id,
                                                            now_playing.queued_by);
                            }
                            now_playing = CommunityNowPlaying{*id, username, 0, now, true};
                            g_store.set_jukebox_pos(station, *id, 0, now);
                            g_community_last_flush_by_station[station] = now;
                        } else {
                            upcoming.push_back(CommunityTrack{*id, username});
                        }
                        lglog::emit("jukebox", "play "+station+" "+*id+" by "+username);
                    }
                } else if (s.find("\"ended\"") != std::string::npos || s.find("\"unplayable\"") != std::string::npos) {
                    const bool unplayable = s.find("\"unplayable\"") != std::string::npos;
                    // {"ended":"<id>"} / {"unplayable":"<id>"} name the track; an old client's {"ended":true} names none.
                    const auto named = extract_json_string_field(s, unplayable ? "\"unplayable\"" : "\"ended\"");
                    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::lock_guard<std::mutex> lock(g_community_mu);
                    auto& now_playing = g_community_now_by_station[station];
                    const int listeners = g_community_listeners_by_station[station];
                    const bool alone = listeners <= 1;
                    bool advance = false;
                    if (!now_playing.active) {
                        // nothing to end
                    } else if (named && *named != now_playing.video_id) {
                        lglog::emit("jukebox", "ended-stale " + station + " by " + username);
                    } else if (unplayable) {
                        if (named && reported_unplayable != now_playing.video_id) {
                            reported_unplayable = now_playing.video_id;
                            auto& votes = g_community_unplayable_by_station[station];
                            if (votes.first != now_playing.video_id) votes = {now_playing.video_id, 0};
                            ++votes.second;
                        }
                        const auto& votes = g_community_unplayable_by_station[station];
                        const int n = votes.first == now_playing.video_id ? votes.second : 0;
                        // Brandon 2026-11-08: skip "if it wont play for at least 25% or more people trying to use
                        // juketube" -- so one silent listener (background tab, old page) can't stall the room.
                        advance = n > 0 && (alone || n * 4 >= listeners);
                        lglog::emit("jukebox", "unplayable " + station + " " + now_playing.video_id + " " +
                                    std::to_string(n) + "/" + std::to_string(listeners) + " by " + username);
                    } else {
                        advance = alone || (named && community_elapsed_locked(station, now) >= kCommunityMinEndedSec);
                        if (!advance) lglog::emit("jukebox", "ended-ignored " + station + " by " + username);
                    }
                    if (advance) {
                        // Bug (Brandon: "i dont see an accurate history"): nothing ever
                        // recorded a shared station's plays anywhere, so the History
                        // section's /jukebox/history?station= call always came back empty
                        // and silently fell back to the viewer's OWN personal plays --
                        // "inaccurate," not missing. Recorded here, the one place a track
                        // on this station is actually known to be done.
                        auto& hist = g_community_history_by_station[station];
                        hist.push_back(CommunityTrack{now_playing.video_id, now_playing.queued_by});
                        if (hist.size() > kCommunityHistoryCap) hist.pop_front();
                        // saved too (Brandon 2026-11-21 "theres no history appearing": the in-memory list above is
                        // emptied by every restart, so each deploy wiped it); tests/e2e/juketube_history.sh
                        g_store.record_jukebox_play(kStationHistoryPrefix + station, now_playing.video_id,
                                                    now_playing.queued_by);
                        community_advance_locked(station, now);
                        const auto& np = g_community_now_by_station[station];
                        if (np.active) g_store.set_jukebox_pos(station, np.video_id, 0, now);
                        else g_store.clear_jukebox_pos(station);
                        g_community_last_flush_by_station[station] = now;
                    }
                }
            }
        }

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_community_mu);
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const auto& now_playing = g_community_now_by_station[station];
            const auto elapsed = community_elapsed_locked(station, now);
            // Roughly every 15s while playing, not every ~500ms tick --
            // shared across however many listeners are connected (only
            // whichever one's tick crosses the threshold actually writes).
            if (now_playing.active) {
                auto& last_flush = g_community_last_flush_by_station[station];
                if (now - last_flush >= 15) {
                    last_flush = now;
                    g_store.set_jukebox_pos(station, now_playing.video_id, elapsed, now);
                }
            }
            std::ostringstream os;
            os << "{";
            if (now_playing.active) {
                os << "\"now\":{\"video_id\":\"" << now_playing.video_id
                   << "\",\"queued_by\":\"" << now_playing.queued_by
                   << "\",\"elapsed\":" << elapsed << "}";
            } else {
                os << "\"now\":null";
            }
            os << ",\"listeners\":" << g_community_listeners_by_station[station];
            os << ",\"upcoming\":" << g_community_upcoming_by_station[station].size();
            os << ",\"queue\":[";
            {
                bool qfirst = true;
                for (const auto& t : g_community_upcoming_by_station[station]) {
                    if (!qfirst) os << ",";
                    qfirst = false;
                    os << "{\"video_id\":\"" << t.video_id << "\",\"queued_by\":\"" << t.queued_by << "\"}";
                }
            }
            os << "]}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }

    // Leaving is a ...->0 transition (or not, if someone else is still
    // here): only the actual last listener freezes and saves. WS close is
    // enough on its own to catch this -- no client-side pagehide beacon
    // needed for community/site-wide stations, unlike the personal
    // (HTTP-polled) ones below.
    {
        std::lock_guard<std::mutex> lock(g_community_mu);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        // Elapsed has to be computed while the count still reflects this
        // connection as live -- community_elapsed_locked only adds live
        // time when count>0, so doing this after decrementing would
        // freeze at frozen_offset alone, silently dropping this session's
        // own listening time (caught live: a 5s hold froze at 0, not 5).
        auto& np = g_community_now_by_station[station];
        const auto elapsed = community_elapsed_locked(station, now);
        int& count = g_community_listeners_by_station[station];
        if (count > 0) --count;
        if (count == 0 && np.active) {
            if (elapsed > kJukeboxResumeMaxSec) {
                np = CommunityNowPlaying{};
                g_store.clear_jukebox_pos(station);
            } else {
                np.frozen_offset = elapsed;
                g_store.set_jukebox_pos(station, np.video_id, elapsed, now);
            }
        }
    }
    close(fd);
}

}  // namespace liquidgov::site
