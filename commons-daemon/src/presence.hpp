#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace commons {

// presence.md's occupant shape: { id, display_name, x, y, facing,
// last_update_ms }. x,y are region-local tile coordinates -- the caller
// already knows which region it asked for.
struct Occupant {
    std::string id;            // ephemeral guest id, or "u:<lg_username>" once bound (post-glue)
    std::string display_name;
    double x = 0.0;
    double y = 0.0;
    int facing = 0;
    long long last_update_ms = 0;
};

// RAM only, never a SQLite table (schema.sql says so explicitly). Keyed
// by region, not global: an outdoor region key is "<col>,<row>" (atlas.md
// grid), an interior key is "instance:<id>" (map_instances.instance_id).
// A region with no occupants simply has no entry -- presence.md: "don't
// run 12 empty loops... for no reason."
//
// An id appears in at most one region, at most once. join of a live id is
// replace (same region) or move (other region). rekey does not --total_
// unless that id was actually in from_region. Destination-full does not
// drop the occupant from the source list.
class PresenceStore {
public:
    static constexpr std::size_t kPerRegionCap = 24;
    static constexpr std::size_t kGlobalCap = 96;

    enum class JoinResult { Ok, RegionFull, GlobalFull };

    JoinResult join(const std::string& region, const Occupant& occ);
    bool move(const std::string& region, const std::string& id, double x, double y, int facing);

    // Crossing an outdoor AABB: remove from the old region's list, add to
    // the new one. presence.md: "do not leave a ghost in the old list."
    JoinResult rekey(const std::string& from_region, const std::string& to_region, const Occupant& occ);

    void leave(const std::string& region, const std::string& id);
    std::vector<Occupant> occupants(const std::string& region);
    std::size_t global_count();

private:
    std::mutex mu_;
    std::map<std::string, std::vector<Occupant>> regions_; // guarded by mu_
    std::size_t total_ = 0;                                  // guarded by mu_

    // Callers must hold mu_.
    JoinResult join_locked(const std::string& region, const Occupant& occ);
    bool erase_id_locked(const std::string& id); // any region; --total_ only if found
};

} // namespace commons
