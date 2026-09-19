#pragma once
#include <optional>
#include <string>
#include <vector>

#include "sqlite3.h"

// Durable storage for a bound player's own house (GROK_V5_ANSWER.md: the
// door was real, the room was still RAM -- restart the daemon and the
// chalk is gone). Demo and lab stay exactly as they were, RAM-only,
// shared -- this store is only ever consulted for an
// "instance:house:<username>" region/instance key, never for those.
//
// No mutex: this daemon is single-threaded and synchronous (http.hpp),
// and SQLite serializes its own access regardless -- the RAM stores'
// mutexes guard a std::map a request thread could otherwise race with
// itself; there is no equivalent race here to guard against.
namespace commons {

class HouseStore {
public:
    explicit HouseStore(sqlite3* db) : db_(db) {}

    // Finds or creates the players row and the map_instances row for this
    // bound username's house, in one step -- idempotent, safe to call on
    // every join/rekey into that player's own house. Returns the durable
    // map_instances.instance_id (never the RAM-side "instance:house:..."
    // string, which stays what PresenceStore/the client actually key on).
    int house_instance_for(const std::string& username);

    // Read-only counterpart for routes that must not create a house just
    // because someone looked (GET /placed/instance:house:<anyone>, public,
    // no bound identity required) -- nullopt if this username has never
    // actually entered their own house yet.
    std::optional<int> existing_house_instance_for(const std::string& username);

    struct Position { int x = 0, y = 0, facing = 0; };

    // The last snapshotted position, only if that snapshot's own
    // last_instance_id is actually this house -- a snapshot taken while
    // leaving from outdoors (or never taken at all) correctly yields
    // nullopt, so the caller falls back to the normal fixed entry tile.
    std::optional<Position> last_house_position(const std::string& username);

    // Presence.md's "leave / ~30s snapshot": called on leave, on a rekey
    // away from the owner's own house, and by main.cpp's ~30s periodic
    // pass over bound players standing in their own house. No-op if the
    // username has no players row (can't happen via the normal join path,
    // which always creates one first, but this stays defensive rather
    // than assumed).
    void snapshot_leave(const std::string& username, int x, int y, int facing);

    // House policy. "friends" (the default, and what any unknown value or a
    // username with no row reads as), "locked" (nobody but the owner), or
    // "open" (BUILD open A: any bound, non-blocked player while the owner is in
    // the game). Read on every visit decision, so it is one indexed SELECT.
    std::string visit_policy_of(const std::string& username);
    // False for any policy other than friends/locked/open, or a username with no
    // players row (a house that was never entered has nothing to lock; this
    // never creates a row).
    bool set_visit_policy(const std::string& username, const std::string& policy);

    // Orphan watch (rest slice 4b): READ-ONLY. Every username that has a players
    // row (guests have none), and how much game data hangs off one. These two
    // never write or delete; reap_orphan() below is the only thing that does.
    std::vector<std::string> all_usernames();
    struct Footprint {
        int house_instance = -1;   // -1 = never entered a house
        int inventory_rows = 0;
        int inventory_qty = 0;
        int placed_rows = 0;       // in that house
    };
    Footprint footprint_of(const std::string& username);

    // DESTRUCTIVE (BUILD orphans reap). Removes everything the game holds for one
    // username whose LiquidGov account is gone, in ONE transaction that rolls back
    // whole on any failure: their inventory rows, every placed row in their house,
    // their players row, then their house's map_instances row (only if that row is
    // owned by them). Nothing else is touched. The caller is responsible for every
    // safety check (civic said gone, twice in a row, once more just now; not
    // online; a backup exists) -- this only does the deleting.
    struct ReapResult { int inventory_rows = 0; int placed_rows = 0; bool player_row = false; bool house_row = false; };
    ReapResult reap_orphan(const std::string& username);

    // Bound inventory (GROK_V6_ANSWER.md: lab pickups "vanish on refresh"
    // for a bound player). schema.sql's `inventory` table, first time it is
    // ever written. Guests stay RAM-only -- nothing here is reachable
    // without a resolved civic username. A pickup counts up (capped),
    // it is never consumed: the lab is still real-name flavor, not a
    // crafting economy (pushback.md).
    //
    // False if `item_slug` isn't a real catalog item.
    bool add_pickup(const std::string& username, const std::string& item_slug, long long now_ms);

    struct InventoryEntry { std::string slug; int quantity = 0; };
    // Most recently picked-up first -- element 0 is what the wall places.
    std::vector<InventoryEntry> inventory_of(const std::string& username);

    // Persistent, per-house wall slot -- schema.sql's placed table,
    // scoped by instance_id, never the RAM PlacedStore demo/lab use.
    // "" clears it, same convention as PlacedStore::set.
    std::string get_wall_slot(int instance_id);
    void set_wall_slot(int instance_id, const std::string& item_slug);

private:
    sqlite3* db_;
    int find_or_create_player(const std::string& username);
};

} // namespace commons
