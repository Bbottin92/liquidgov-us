#include "site/features/jukebox/jukebox_personal.hpp"
#include "site/core/adapters_store.hpp"
#include "site/features/feed/composer.hpp"

namespace liquidgov::site {
struct PersonalPlayhead {
    std::string video_id; // which track frozen_offset/resumed_at apply to
    std::int64_t frozen_offset = 0;
    std::int64_t resumed_at = 0;
};
std::mutex g_personal_mu;
std::map<std::string, PersonalPlayhead> g_personal_playhead_by_station;                     // "u:<owner>" -> playhead
std::map<std::string, std::map<std::string, std::int64_t>> g_personal_listeners_by_station; // "u:<owner>" -> session id -> last_seen_unix

// Caller must hold g_personal_mu. A different track than last time
// (completed/skipped/reordered) always starts the playhead over at 0,
// same as community's track-advance -- persisted immediately so a
// restart right after a track change doesn't fall back to a stale entry
// for the wrong video. The only place any of the three functions below
// resets frozen_offset to 0; none of them mix an old elapsed value into
// a new track.
PersonalPlayhead& personal_playhead_for_locked(const std::string& station, const std::string& current_video_id,
                                                std::int64_t now) {
    auto& ph = g_personal_playhead_by_station[station];
    if (ph.video_id != current_video_id) {
        ph = PersonalPlayhead{current_video_id, 0, now};
        g_personal_listeners_by_station[station].clear();
        g_store.set_jukebox_pos(station, current_video_id, 0, now);
    }
    return ph;
}

// Caller must hold g_personal_mu. Pure cleanup, no opinion about what a
// live->empty transition means -- each caller below decides that for
// itself, in its own correct order (see each function's own comment;
// getting this order wrong is exactly what turned a real bug -- the
// community WS side's disconnect handler -- into a freeze that silently
// landed at 0 instead of the real elapsed, caught only by testing the
// running server directly, not by the build or the unit tests).
void personal_prune_locked(const std::string& station, std::int64_t now) {
    auto& listeners = g_personal_listeners_by_station[station];
    for (auto it = listeners.begin(); it != listeners.end();) {
        if (now - it->second > kPersonalHeartbeatGraceSeconds) it = listeners.erase(it);
        else ++it;
    }
}

// Safe to call from GET /jukebox/now, which must never itself count as a
// listen heartbeat (a paused-but-open widget still polls this) -- never
// touches or adds a heartbeat entry, only ever removes stale ones.
// Elapsed is snapshotted BEFORE pruning, using the true pre-prune live
// state, so the number returned here is never affected by whether this
// particular call happens to be the one that notices a stale listener.
std::int64_t personal_jukebox_elapsed(const std::string& station, const std::string& current_video_id) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> lock(g_personal_mu);
    auto& ph = personal_playhead_for_locked(station, current_video_id, now);
    auto& listeners = g_personal_listeners_by_station[station];
    const bool live_before = !listeners.empty();
    const std::int64_t elapsed = ph.frozen_offset + (live_before ? std::max<std::int64_t>(0, now - ph.resumed_at) : 0);
    personal_prune_locked(station, now);
    // A listener who vanished without ever sending a pause/pagehide
    // freeze (tab killed, network loss, crash): freeze right here at the
    // value already snapshotted above, instead of silently losing their
    // progress back to whatever frozen_offset was before this session
    // started, and persist it so a restart around now still resumes
    // from the right place.
    if (live_before && listeners.empty()) {
        ph.frozen_offset = elapsed;
        g_store.set_jukebox_pos(station, ph.video_id, elapsed, now);
    }
    return elapsed;
}

// Called only from POST /jukebox/pos while actually playing (no freeze
// flag) -- registers/refreshes this session as currently listening. Does
// NOT trust any client-supplied offset; the server clock is the only
// thing that ever advances resumed_at, precisely so two listeners
// reporting drifted currentTime values can't fight over the shared
// position. Touches this session's own heartbeat BEFORE pruning, so its
// own (possibly late) arrival can never be the reason the prune sees an
// empty station and invents a freeze in the middle of what is actually a
// live check-in; 0->1 (nobody else was here) just resumes -- it never
// writes jpos itself, since jpos already correctly holds wherever the
// last listener actually left off.
void personal_heartbeat_locked(const std::string& station, const std::string& current_video_id,
                                const std::string& session_id, std::int64_t now) {
    auto& ph = personal_playhead_for_locked(station, current_video_id, now);
    auto& listeners = g_personal_listeners_by_station[station];
    const bool was_live = !listeners.empty();
    listeners[session_id] = now;
    personal_prune_locked(station, now); // only genuinely-stale *other* sessions can be removed now
    if (!was_live) ph.resumed_at = now;
}

// Called only from POST /jukebox/pos with freeze=1 (a manual pause, or a
// pagehide sendBeacon) -- this session is stopping right now, not
// waiting out the grace window. Elapsed is computed while this session's
// own entry still counts (before it's erased) -- erasing or pruning
// first would silently drop this session's own listening time, the same
// ordering mistake already caught and fixed on the community WS side.
// client_offset is trusted only if it's within 3s of the server's own
// idea of elapsed; otherwise the server clock wins, same distrust as the
// live heartbeat path. Only actually freezes (and persists) if this was
// the last listener once other genuinely-stale sessions are pruned too
// -- someone else still fresh means the shared playhead keeps running
// for them.
void personal_freeze_locked(const std::string& station, const std::string& current_video_id,
                             const std::string& session_id, std::optional<std::int64_t> client_offset,
                             std::int64_t now) {
    auto& ph = personal_playhead_for_locked(station, current_video_id, now);
    auto& listeners = g_personal_listeners_by_station[station];
    const bool was_live = !listeners.empty();
    const std::int64_t server_elapsed = ph.frozen_offset +
        (was_live ? std::max<std::int64_t>(0, now - ph.resumed_at) : 0);
    listeners.erase(session_id);
    personal_prune_locked(station, now);
    if (!listeners.empty()) return; // someone else is still listening -- their playhead keeps running
    const std::int64_t diff = client_offset ?
        (*client_offset > server_elapsed ? *client_offset - server_elapsed : server_elapsed - *client_offset) : -1;
    const std::int64_t offset = (client_offset && diff <= 3) ? *client_offset : server_elapsed;
    ph.frozen_offset = offset;
    g_store.set_jukebox_pos(station, ph.video_id, offset, now);
}

}  // namespace liquidgov::site
