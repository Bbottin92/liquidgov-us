#pragma once
// records/jukebox.hpp: the jukebox record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
// moved, not rewritten). The LGP1 format of every record type stays in file_store.hpp's header comment.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

// Page-local jukebox queue entry for a profile (design: liquidgov_context_handoff.md
// "Jukebox", navigation option A — MPA, no site-wide SPA, stop on navigate away).
// The owner always curates their own queue; depending on the owner's
// JukeboxPolicy, friends or anyone else may also queue a track (see
// queued_by below). Only the owner can remove. No WebSocket sync — that is
// the in-world jukebox, a separate, unbuilt slice.
struct JukeboxTrack {
    std::string owner;      // username
    std::string video_id;   // validated 11-char YouTube id, never a raw URL
    std::int64_t added_at = 0;
    // Who requested this track. Equal to owner for the owner's own curated
    // additions, which stay queued after playing (today's behavior,
    // unaffected). Anything else is a guest request: one shot, removed from
    // the queue and logged to history the moment it finishes playing (see
    // complete_jukebox_track), and limited to one outstanding request per
    // guest at a time (enforced by the caller, same as policy/block checks).
    std::string queued_by;
};

// One past play, kept so a profile can show "things played in the past"
// even after a guest-requested track is gone from the live queue.
struct JukeboxHistoryEntry {
    std::string owner;
    std::string video_id;
    std::string queued_by;
    std::int64_t played_at = 0;
};

inline constexpr std::size_t kJukeboxQueueCap = 50;

inline constexpr std::size_t kJukeboxHistoryCap = 30;

// A station's saved playhead -- see the jpos tag comment above. offset_sec
// is seconds into video_id; updated_at is when this was last written, kept
// mostly for debugging/future use, not currently read for any decision.
struct JukeboxPos {
    std::string video_id;
    std::int64_t offset_sec = 0;
    std::int64_t updated_at = 0;
};

// Extracts an 11-char YouTube video id from a small allow-listed set of URL
// shapes (youtube.com/watch?v=, youtu.be/<id>, youtube.com/embed/<id>,
// youtube-nocookie.com/embed/<id>). Returns nullopt for anything else. This
// is the only thing ever stored or rendered from a submitted URL — never the
// URL itself, never author HTML; the embed src is built by the platform from
// this id alone.
std::optional<std::string> extract_youtube_id(const std::string& url);

}  // namespace liquidgov::platform
