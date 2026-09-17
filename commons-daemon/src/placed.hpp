#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

namespace commons {

// V2 (GROK_V2_ANSWER.md): lab output placed in a shared interior's wall
// slot as a game tile -- explicitly "not a civic PNG," a different,
// simpler thing than the real profile-canvas wall slot (glue.md,
// canvas-frame.md, still parked on a civic /latest route). This is one
// placed item slug per instance -- this game has exactly one placeable
// slot (the demo house wall-slot) and one shared instance of it, so
// "one item per instance key" is the actual shape needed, not a
// premature simplification of something bigger.
//
// Session RAM only, same as presence and guest credentials -- a restart
// clears it, matching v1.md's "vanishes on refresh" for the pickup
// itself. Never the real `placed` SQLite table (schema.sql) -- that
// table is keyed by a real map_instances.instance_id and player_id,
// the post-glue bound-player shape; guests still never hit disk.
class PlacedStore {
public:
    // Empty string means nothing is placed there.
    std::string get(const std::string& instance);
    void set(const std::string& instance, const std::string& item);

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::string> placed_; // guarded by mu_
};

} // namespace commons
