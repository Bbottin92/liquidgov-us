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
    // presence.md's crash-recovery pin, applied here as the idle-reap
    // threshold: a tab that stops calling /presence/move (crash, close
    // without a clean leave) should not hold a cap slot forever.
    static constexpr long long kIdleMs = 30000;

    enum class JoinResult { Ok, RegionFull, GlobalFull };

    // `out_display_name`, when non-null, is set to the name actually
    // stored (post-disambiguation) on Ok -- GROK_NAME_REVIEW.md: a
    // self-nameplate must draw the *accepted* name, not the raw typed
    // input, since this store may have appended " (2)" etc.
    JoinResult join(const std::string& region, const Occupant& occ, std::string* out_display_name = nullptr);
    bool move(const std::string& region, const std::string& id, double x, double y, int facing);

    // Crossing an outdoor AABB: remove from the old region's list, add to
    // the new one. presence.md: "do not leave a ghost in the old list."
    JoinResult rekey(const std::string& from_region, const std::string& to_region, const Occupant& occ,
                      std::string* out_display_name = nullptr);

    void leave(const std::string& region, const std::string& id);
    std::vector<Occupant> occupants(const std::string& region);
    std::size_t global_count();

    // Global erase, public: the guest -> bound handoff (cookie-bind.md)
    // needs to retire a *different* id than the one it's about to insert
    // (the old g_... row, once a cookie resolves to u:<name>), which
    // join_locked's own erase_id_locked never does on its own -- it only
    // ever erases the id it's currently inserting.
    void forget(const std::string& id);

    // Every bound username currently present in any region (occupant ids
    // "u:<name>", prefix stripped), for the friends-lot doors: atlas.md
    // resolves a dynamic Homestead door only for a friend who is online
    // *and* bound. Presence itself still knows nothing about who is
    // friends with whom -- the caller intersects this with the civic query.
    std::vector<std::string> bound_usernames();

    // True if this exact id currently has a row in `region`. Used to tell
    // whether a cookie-resolved bound id has actually been promoted into
    // *this* region yet -- promotion only happens at join/rekey
    // (cookie-bind.md: "promote at room change"), so a guest who logs in
    // mid-walk should keep moving under their existing guest row until
    // their next room change, not have every move rejected because
    // u:<name> isn't here yet.
    bool contains(const std::string& region, const std::string& id);

private:
    std::mutex mu_;
    std::map<std::string, std::vector<Occupant>> regions_; // guarded by mu_
    std::size_t total_ = 0;                                  // guarded by mu_

    // Callers must hold mu_.
    JoinResult join_locked(const std::string& region, const Occupant& occ, std::string* out_display_name);
    bool erase_id_locked(const std::string& id); // any region; --total_ only if found
    // Drops any occupant whose last_update_ms is more than kIdleMs behind
    // `now`. Called at the top of every public method, under mu_, so no
    // caller ever sees or mutates a stale entry.
    void reap_locked(long long now);
    // Should-fix (GROK_PRESENCE_REVIEW.md): display_name is still
    // client-chosen -- id/secret closes the *row* hijack, but nothing
    // stopped a second guest from typing the exact name of someone
    // already standing in the same region and visually passing as them.
    // Guests have no bound identity to protect yet (no lg_username until
    // glue lands), so this doesn't try to reserve a name globally or
    // across time -- it only guarantees no two *simultaneously present*
    // occupants in one region share a display_name, appending " (2)",
    // " (3)", etc. `exclude_id` skips the caller's own current row (the
    // same-id "replace" path in join_locked, which would otherwise always
    // collide with itself).
    std::string disambiguate_name_locked(const std::string& region, const std::string& base_name,
                                          const std::string& exclude_id);
};

} // namespace commons
